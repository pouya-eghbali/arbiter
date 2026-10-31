#include "mobilemodel.h"
#include "mobileplatform.h"
#include "nativepage.h"
#import <AuthenticationServices/AuthenticationServices.h>
#import <BackgroundTasks/BackgroundTasks.h>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#include <memory>
extern "C" {
char *arbiter_ios_vault_directory();
char *arbiter_ios_vault_key(int);
char *arbiter_ios_crypto(const unsigned char *, int, const unsigned char *, int, const unsigned char *, int,
                         int);
void arbiter_ios_verify(const char *, void (*)(int, void *), void *);
void arbiter_ios_refresh_identities();
char *arbiter_ios_legacy_session();
void arbiter_ios_language(const char *);
void arbiter_ios_enable_provider();
}
namespace {
QString take(char *value) {
    if (!value)
        return {};
    const auto s = QString::fromUtf8(value);
    free(value);
    return s;
}
UIViewController *presenter() {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes)
        if ([scene isKindOfClass:UIWindowScene.class])
            for (UIWindow *window in ((UIWindowScene *)scene).windows)
                if (window.isKeyWindow) {
                    UIViewController *controller = window.rootViewController;
                    while (controller.presentedViewController)
                        controller = controller.presentedViewController;
                    return controller;
                }
    return nil;
}
QVariantMap map(NSDictionary *data) {
    if (![NSJSONSerialization isValidJSONObject:data])
        return {};
    NSData *bytes = [NSJSONSerialization dataWithJSONObject:data options:0 error:nil];
    return QJsonDocument::fromJson(QByteArray((const char *)bytes.bytes, bytes.length))
        .object()
        .toVariantMap();
}
NSString *string(const QString &s) {
    return s.toNSString();
}
bool protectedScreen = false;
WKContentRuleList *compiledRules = nil;
bool rulesReady = false, rulesLoading = false;
NSMutableArray *ruleCallbacks = nil;
void prepareRules(void (^callback)(void)) {
    if (rulesReady) {
        callback();
        return;
    }
    if (!ruleCallbacks)
        ruleCallbacks = [NSMutableArray new];
    [ruleCallbacks addObject:[callback copy]];
    if (rulesLoading)
        return;
    rulesLoading = true;
    QFile file(":/adblock/webkit.json");
    if (!file.open(QIODevice::ReadOnly)) {
        rulesReady = true;
        rulesLoading = false;
        for (id block in ruleCallbacks)
            ((void (^)(void))block)();
        [ruleCallbacks removeAllObjects];
        return;
    }
    auto data = file.readAll();
    NSString *source = QString::fromUtf8(data).toNSString();
    NSString *identifier =
        ("Arbiter-" + QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()))
            .toNSString();
    auto done = ^(WKContentRuleList *value, NSError *) {
      compiledRules = value;
      rulesReady = true;
      rulesLoading = false;
      NSArray *callbacks = [ruleCallbacks copy];
      [ruleCallbacks removeAllObjects];
      for (id block in callbacks)
          ((void (^)(void))block)();
    };
    [WKContentRuleListStore.defaultStore
        lookUpContentRuleListForIdentifier:identifier
                         completionHandler:^(WKContentRuleList *value, NSError *error) {
                           if (value)
                               done(value, nil);
                           else
                               [WKContentRuleListStore.defaultStore
                                   compileContentRuleListForIdentifier:identifier
                                                encodedContentRuleList:source
                                                     completionHandler:done];
                         }];
}
} // namespace
@interface ArbiterDownloads : NSObject <WKDownloadDelegate>
@property(nonatomic, strong) NSMutableDictionary *records;
@property(nonatomic, strong) NSMutableDictionary *pending;
+ (instancetype)shared;
- (void)start:(WKDownload *)download privateMode:(BOOL)privateMode;
- (void)finish:(WKDownload *)download error:(BOOL)error;
@end
@implementation ArbiterDownloads
+ (instancetype)shared {
    static ArbiterDownloads *center = nil;
    if (!center) {
        center = [ArbiterDownloads new];
        center.records = [NSMutableDictionary new];
        center.pending = [NSMutableDictionary new];
        QTimer *timer = new QTimer(qApp);
        timer->setInterval(750);
        QObject::connect(timer, &QTimer::timeout, qApp, [] {
            for (NSString *key in [center.records.allKeys copy]) {
                NSMutableDictionary *record = center.records[key];
                WKDownload *download = record[@"download"];
                NSProgress *progress = download.progress;
                auto stamp = QDateTime::currentMSecsSinceEpoch();
                auto previous = [record[@"stamp"] longLongValue];
                auto bytes = [record[@"bytes"] longLongValue];
                auto received = progress.completedUnitCount;
                auto speed = previous ? ((received - bytes) * 1000 / qMax<qint64>(1, stamp - previous)) : 0;
                record[@"stamp"] = @(stamp);
                record[@"bytes"] = @(received);
                if (MobileModel::instance)
                    MobileModel::instance->downloadEvent("downloadProgress",
                                                         {{"id", QString::fromNSString(key)},
                                                          {"received", received},
                                                          {"total", progress.totalUnitCount},
                                                          {"speed", speed}},
                                                         [record[@"private"] boolValue]);
            }
        });
        timer->start();
    }
    return center;
}
- (void)start:(WKDownload *)download privateMode:(BOOL)privateMode {
    self.pending[[NSValue valueWithNonretainedObject:download]] = @(privateMode);
    download.delegate = self;
}
- (void)download:(WKDownload *)download
    decideDestinationUsingResponse:(NSURLResponse *)response
                 suggestedFilename:(NSString *)filename
                 completionHandler:(void (^)(NSURL *))reply {
    auto key = NSUUID.UUID.UUIDString;
    auto name = filename.lastPathComponent.length ? filename.lastPathComponent : @"Download";
    auto root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Downloads";
    QDir().mkpath(root);
    auto path = root + "/" + QString::fromNSString(key) + "-" + QString::fromNSString(name);
    BOOL privateMode = [self.pending[[NSValue valueWithNonretainedObject:download]] boolValue];
    [self.pending removeObjectForKey:[NSValue valueWithNonretainedObject:download]];
    self.records[key] = [@{
        @"download" : download,
        @"path" : string(path),
        @"private" : @(privateMode),
        @"bytes" : @0,
        @"stamp" : @0
    } mutableCopy];
    if (MobileModel::instance)
        MobileModel::instance->downloadEvent("download",
                                             {{"id", QString::fromNSString(key)},
                                              {"name", QString::fromNSString(name)},
                                              {"path", QUrl::fromLocalFile(path).toString()},
                                              {"url", QUrl::fromNSURL(response.URL).toString()},
                                              {"received", 0},
                                              {"total", response.expectedContentLength}},
                                             privateMode);
    reply([NSURL fileURLWithPath:string(path)]);
}
- (void)downloadDidFinish:(WKDownload *)download {
    [self finish:download error:NO];
}
- (void)download:(WKDownload *)download didFailWithError:(NSError *)error resumeData:(NSData *)data {
    [self finish:download error:YES];
}
- (void)finish:(WKDownload *)download error:(BOOL)error {
    for (NSString *key in [self.records.allKeys copy]) {
        auto record = self.records[key];
        if (record[@"download"] != download)
            continue;
        if (MobileModel::instance)
            MobileModel::instance->downloadEvent("downloadProgress",
                                                 {{"id", QString::fromNSString(key)},
                                                  {"received", download.progress.completedUnitCount},
                                                  {"total", download.progress.totalUnitCount},
                                                  {"complete", !error},
                                                  {"error", bool(error)}},
                                                 [record[@"private"] boolValue]);
        [self.records removeObjectForKey:key];
    }
}
@end

// Foreign native views sit above Qt Quick. A rounded cutout exposes the QML
// pill below them while preserving a full-height, normally scrolling page.
// Hit testing uses the same cutout so gestures reach the QML controls.
@interface ArbiterPageView : WKWebView
@property(nonatomic) CGRect overlayFraction;
@end
@implementation ArbiterPageView
- (CGRect)overlayFrame {
    CGRect r = self.overlayFraction;
    return CGRectMake(r.origin.x * self.bounds.size.width, r.origin.y * self.bounds.size.height,
                      r.size.width * self.bounds.size.width, r.size.height * self.bounds.size.height);
}
- (void)layoutSubviews {
    [super layoutSubviews];
    [self updateOverlayMask];
}
- (void)setOverlayFraction:(CGRect)rect {
    _overlayFraction = rect;
    [self updateOverlayMask];
}
- (void)updateOverlayMask {
    CGRect rect = [self overlayFrame];
    if (CGRectIsEmpty(rect)) {
        self.layer.mask = nil;
        return;
    }
    UIBezierPath *path = [UIBezierPath bezierPathWithRect:self.bounds];
    [path appendPath:[UIBezierPath bezierPathWithRoundedRect:rect
                                                cornerRadius:MIN(32, rect.size.height / 2)]];
    CAShapeLayer *mask = [CAShapeLayer layer];
    mask.frame = self.bounds;
    mask.fillRule = kCAFillRuleEvenOdd;
    mask.path = path.CGPath;
    self.layer.mask = mask;
}
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event {
    if (CGRectContainsPoint([self overlayFrame], point))
        return nil;
    return [super hitTest:point withEvent:event];
}
@end

@interface ArbiterWeb
    : NSObject <WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandler, WKDownloadDelegate> {
  @public
    QPointer<NativePage> page;
}
@property(nonatomic, strong) ArbiterPageView *view;
@property(nonatomic, strong) NSMutableDictionary *downloads;
@property(nonatomic) NSUInteger navigationGeneration;
@property(nonatomic, copy) NSString *privateProfile;
- (instancetype)initWithPage:(NativePage *)owner profile:(NSString *)profile privateMode:(BOOL)privateMode;
- (void)configure;
- (void)captureForPreview;
@end
static NSMutableDictionary<NSString *, WKWebsiteDataStore *> *privateStores;
static NSMutableDictionary<NSString *, NSNumber *> *privateUsers;
@implementation ArbiterWeb
- (instancetype)initWithPage:(NativePage *)owner profile:(NSString *)profile privateMode:(BOOL)privateMode {
    if (!(self = [super init]))
        return nil;
    page = owner;
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration new];
    if (privateMode) {
        if (!privateStores) {
            privateStores = [NSMutableDictionary new];
            privateUsers = [NSMutableDictionary new];
        }
        self.privateProfile = profile;
        if (!privateStores[profile])
            privateStores[profile] = WKWebsiteDataStore.nonPersistentDataStore;
        configuration.websiteDataStore = privateStores[profile];
        privateUsers[profile] = @([privateUsers[profile] intValue] + 1);
    } else if (![profile isEqualToString:@"shared"]) {
        NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:profile];
        if (uuid)
            configuration.websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:uuid];
    }
    configuration.allowsInlineMediaPlayback = YES;
    configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAll;
    [configuration.userContentController addScriptMessageHandler:self
                                                    contentWorld:WKContentWorld.defaultClientWorld
                                                            name:@"arbiterForms"];
    self.view = [[ArbiterPageView alloc] initWithFrame:CGRectMake(0, 0, 400, 700)
                                         configuration:configuration];
    self.view.navigationDelegate = self;
    self.view.UIDelegate = self;
    self.view.allowsBackForwardNavigationGestures = YES;
    [self.view addObserver:self forKeyPath:@"title" options:NSKeyValueObservingOptionNew context:nil];
    [self.view addObserver:self
                forKeyPath:@"estimatedProgress"
                   options:NSKeyValueObservingOptionNew
                   context:nil];
    [self.view addObserver:self forKeyPath:@"scrollView.contentOffset" options:0 context:nil];
    [self configure];
    return self;
}
- (void)dealloc {
    if (self.privateProfile) {
        int remaining = [privateUsers[self.privateProfile] intValue] - 1;
        if (remaining > 0)
            privateUsers[self.privateProfile] = @(remaining);
        else {
            [privateUsers removeObjectForKey:self.privateProfile];
            [privateStores removeObjectForKey:self.privateProfile];
        }
    }
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self.view removeObserver:self forKeyPath:@"scrollView.contentOffset"];
    [self.view stopLoading];
    [self.view removeObserver:self forKeyPath:@"title"];
    [self.view removeObserver:self forKeyPath:@"estimatedProgress"];
    [self.view.configuration.userContentController
        removeScriptMessageHandlerForName:@"arbiterForms"
                             contentWorld:WKContentWorld.defaultClientWorld];
    self.view.navigationDelegate = nil;
    self.view.UIDelegate = nil;
}
- (void)captureForPreview {
    if (!page || !MobileModel::instance || MobileModel::instance->activeId() != page->tabId ||
        (page->privateMode && MobileModel::instance->privateLocked()) ||
        qGuiApp->applicationState() != Qt::ApplicationActive)
        return;
    QPointer<NativePage> target = page;
    WKSnapshotConfiguration *config = [WKSnapshotConfiguration new];
    config.afterScreenUpdates = NO;
    config.snapshotWidth = @(MIN(640, self.view.bounds.size.width));
    [self.view
        takeSnapshotWithConfiguration:config
                    completionHandler:^(UIImage *image, NSError *error) {
                      if (!target || !image)
                          return;
                      NSData *png = UIImagePNGRepresentation(image);
                      if (png.length > 5 * 1024 * 1024)
                          return;
                      target->event(
                          "preview",
                          {{"image", "data:image/png;base64," +
                                         QString::fromNSString([png base64EncodedStringWithOptions:0])}});
                    }];
}
- (void)configure {
    if (!page || !MobileModel::instance)
        return;
    auto model = MobileModel::instance;
    auto controller = self.view.configuration.userContentController;
    [controller removeAllUserScripts];
    [controller
        addUserScript:[[WKUserScript alloc] initWithSource:string(model->formScript(page->privateMode))
                                             injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                          forMainFrameOnly:YES
                                            inContentWorld:WKContentWorld.defaultClientWorld]];
    auto prefs = QJsonObject{
        {"savePasswords", model->settings().value("savePasswords").toBool() && !page->privateMode},
        {"suggestPasswords", model->settings().value("suggestPasswords").toBool()}};
    [self.view
        callAsyncJavaScript:@"globalThis.__arbiterUpdatePreferences?.(prefs)"
                  arguments:@{
                      @"prefs" : [NSJSONSerialization
                          JSONObjectWithData:[NSData dataWithBytes:QJsonDocument(prefs).toJson().constData()
                                                            length:QJsonDocument(prefs).toJson().size()]
                                     options:0
                                       error:nil]
                  }
                    inFrame:nil
             inContentWorld:WKContentWorld.defaultClientWorld
          completionHandler:nil];
    [controller removeAllContentRuleLists];
    if (!model->blockingFor(page->url()))
        return;
    if (compiledRules)
        [controller addContentRuleList:compiledRules];
}
- (void)observeValueForKeyPath:(NSString *)key
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context {
    if ([key isEqualToString:@"scrollView.contentOffset"]) {
        [NSObject cancelPreviousPerformRequestsWithTarget:self
                                                 selector:@selector(captureForPreview)
                                                   object:nil];
        [self performSelector:@selector(captureForPreview) withObject:nil afterDelay:.25];
        return;
    }

    if (!page)
        return;
    if ([key isEqualToString:@"title"])
        page->event("title", {{"title", QString::fromNSString(self.view.title ?: @"")}});
    else
        page->event("progress", {{"progress", int(self.view.estimatedProgress * 100)}});
}
- (void)location {
    if (page)
        page->event("location", {{"url", QString::fromNSString(self.view.URL.absoluteString ?: @"")},
                                 {"back", bool(self.view.canGoBack)},
                                 {"forward", bool(self.view.canGoForward)}});
}
- (void)webView:(WKWebView *)web didStartProvisionalNavigation:(WKNavigation *)navigation {
    if (page)
        page->event("start");
}
- (void)webView:(WKWebView *)web didCommitNavigation:(WKNavigation *)navigation {
    [self location];
    [self configure];
}
- (void)webView:(WKWebView *)web didFinishNavigation:(WKNavigation *)navigation {
    [self location];
    if (page)
        page->event("finish", {{"success", true}});
}
- (void)webView:(WKWebView *)web
    didFailProvisionalNavigation:(WKNavigation *)navigation
                       withError:(NSError *)error {
    if (page)
        page->event("error", {{"message", QString::fromNSString(error.localizedDescription)}});
}
- (void)webView:(WKWebView *)web didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    [self webView:web didFailProvisionalNavigation:navigation withError:error];
}
- (void)webViewWebContentProcessDidTerminate:(WKWebView *)web {
    if (page)
        page->event("error", {{"message", QObject::tr("The page stopped responding. Reload to try again.")}});
}
- (void)userContentController:(WKUserContentController *)controller
      didReceiveScriptMessage:(WKScriptMessage *)message {
    if (!page || !message.frameInfo.isMainFrame || ![message.body isKindOfClass:NSDictionary.class] ||
        !MobileModel::instance)
        return;
    auto data = map(message.body);
    auto type = data.value("type").toString();
    if (!QStringList{"credentialFocus", "loginSubmitted", "loginSucceeded", "activityState"}.contains(type))
        return;
    auto frame = message.frameInfo.securityOrigin;
    auto url = page->url();
    if (url.scheme() != "https" && url.scheme() != "http")
        return;
    if (QString::fromNSString(frame.host) != url.host() ||
        QString::fromNSString(frame.protocol) != url.scheme() ||
        (frame.port ?: (url.scheme() == "https" ? 443 : 80)) != url.port(url.scheme() == "https" ? 443 : 80))
        return;
    if (type == "loginSubmitted" &&
        (page->privateMode || !MobileModel::instance->settings().value("savePasswords").toBool()))
        return;
    data.remove("id");
    data.remove("type");
    page->event(type, data);
}
- (void)webView:(WKWebView *)web
    decidePolicyForNavigationAction:(WKNavigationAction *)action
                    decisionHandler:(void (^)(WKNavigationActionPolicy))reply {
    auto url = QUrl::fromNSURL(action.request.URL);
    if (url.scheme() == "https" || url.scheme() == "http" || url.toString() == "about:blank") {
        if (action.targetFrame == nil) {
            if (page && action.navigationType == WKNavigationTypeLinkActivated)
                MobileModel::instance->addTab(url.toString());
            reply(WKNavigationActionPolicyCancel);
            return;
        }
        if (action.targetFrame.isMainFrame && MobileModel::instance) {
            auto controller = web.configuration.userContentController;
            [controller removeAllContentRuleLists];
            if (compiledRules && MobileModel::instance->blockingFor(url))
                [controller addContentRuleList:compiledRules];
        }
        reply(action.shouldPerformDownload ? WKNavigationActionPolicyDownload
                                           : WKNavigationActionPolicyAllow);
        return;
    }
    if (action.navigationType == WKNavigationTypeLinkActivated &&
        QStringList{"mailto", "tel", "sms"}.contains(url.scheme()))
        [UIApplication.sharedApplication openURL:action.request.URL options:@{} completionHandler:nil];
    reply(WKNavigationActionPolicyCancel);
}
- (void)webView:(WKWebView *)web
    decidePolicyForNavigationResponse:(WKNavigationResponse *)response
                      decisionHandler:(void (^)(WKNavigationResponsePolicy))reply {
    reply(response.canShowMIMEType ? WKNavigationResponsePolicyAllow : WKNavigationResponsePolicyDownload);
}
- (void)webView:(WKWebView *)web
     navigationAction:(WKNavigationAction *)action
    didBecomeDownload:(WKDownload *)download {
    [[ArbiterDownloads shared] start:download privateMode:page ? page->privateMode : NO];
}
- (void)webView:(WKWebView *)web
    navigationResponse:(WKNavigationResponse *)response
     didBecomeDownload:(WKDownload *)download {
    [[ArbiterDownloads shared] start:download privateMode:page ? page->privateMode : NO];
}
- (void)webView:(WKWebView *)web
    requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin
                          initiatedByFrame:(WKFrameInfo *)frame
                                      type:(WKMediaCaptureType)type
                           decisionHandler:(void (^)(WKPermissionDecision))reply {
    reply(WKPermissionDecisionPrompt);
}
- (void)webView:(WKWebView *)web
    runJavaScriptAlertPanelWithMessage:(NSString *)message
                      initiatedByFrame:(WKFrameInfo *)frame
                     completionHandler:(void (^)(void))reply {
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:web.URL.host
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *action) {
                                              reply();
                                            }]];
    [presenter() presentViewController:alert animated:YES completion:nil];
}
- (void)webView:(WKWebView *)web
    runJavaScriptConfirmPanelWithMessage:(NSString *)message
                        initiatedByFrame:(WKFrameInfo *)frame
                       completionHandler:(void (^)(BOOL))reply {
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:web.URL.host
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                              style:UIAlertActionStyleCancel
                                            handler:^(UIAlertAction *action) {
                                              reply(NO);
                                            }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *action) {
                                              reply(YES);
                                            }]];
    [presenter() presentViewController:alert animated:YES completion:nil];
}
- (void)webView:(WKWebView *)web
    runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt
                              defaultText:(NSString *)text
                         initiatedByFrame:(WKFrameInfo *)frame
                        completionHandler:(void (^)(NSString *))reply {
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:web.URL.host
                                                                   message:prompt
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
      field.text = text;
    }];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                              style:UIAlertActionStyleCancel
                                            handler:^(UIAlertAction *action) {
                                              reply(nil);
                                            }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *action) {
                                              reply(alert.textFields.firstObject.text);
                                            }]];
    [presenter() presentViewController:alert animated:YES completion:nil];
}
@end
namespace MobilePlatform {
void initialize() {
    QTimer::singleShot(0, qApp, [] {
        if (!MobileModel::instance)
            return;
        QObject::connect(MobileModel::instance, &MobileModel::credentialProviderRequested, qApp,
                         [] { arbiter_ios_enable_provider(); });
        QObject::connect(MobileModel::instance, &MobileModel::defaultBrowserRequested, qApp, [] {
#ifdef ARBITER_IOS_DEVICE_TESTING
            emit MobileModel::instance->message(
                QObject::tr("Default-browser eligibility needs Apple approval. This is a local test build."));
#else
 [UIApplication.sharedApplication openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString] options:@{} completionHandler:nil];
#endif
        });
    });
    [BGTaskScheduler.sharedScheduler
        registerForTaskWithIdentifier:@"com.arbiter.mobile.sync"
                           usingQueue:dispatch_get_main_queue()
                        launchHandler:^(__kindof BGTask *task) {
                          if ([NSUserDefaults.standardUserDefaults boolForKey:@"arbiter.backgroundSync"])
                              MobilePlatform::configureBackgroundSync(true);
                          __block BOOL finished = NO;
                          auto complete = ^(bool success) {
                            if (finished)
                                return;
                            finished = YES;
                            [task setTaskCompletedWithSuccess:success];
                          };
                          task.expirationHandler = ^{
                            if (MobileModel::instance)
                                MobileModel::instance->cancelBackgroundSync();
                            complete(false);
                          };
                          if (MobileModel::instance)
                              MobileModel::instance->backgroundSync([complete](bool ok) { complete(ok); });
                          else
                              complete(false);
                        }];
    [NSNotificationCenter.defaultCenter
        addObserverForName:UIApplicationWillResignActiveNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(NSNotification *) {
                  if (!protectedScreen)
                      return;
                  UIView *view = presenter().view.window;
                  if (!view)
                      return;
                  UIVisualEffectView *cover = [[UIVisualEffectView alloc]
                      initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleDark]];
                  cover.frame = view.bounds;
                  cover.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
                  cover.tag = 438917;
                  cover.backgroundColor = [UIColor colorWithWhite:0 alpha:.7];
                  [view addSubview:cover];
                }];
    [NSNotificationCenter.defaultCenter
        addObserverForName:UIApplicationDidBecomeActiveNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(NSNotification *) {
                  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_MSEC),
                                 dispatch_get_main_queue(), ^{
                                   [presenter().view.window viewWithTag:438917].hidden = YES;
                                   [[presenter().view.window viewWithTag:438917] removeFromSuperview];
                                 });
                }];
}
void shutdown() {}
QString vaultDirectory() {
    return take(arbiter_ios_vault_directory());
}
QByteArray vaultKey(bool create) {
    return QByteArray::fromBase64(take(arbiter_ios_vault_key(create)).toLatin1());
}
QByteArray crypt(const QByteArray &data, const QByteArray &key, const QByteArray &aad, bool decrypt) {
    return QByteArray::fromBase64(
        take(arbiter_ios_crypto((const unsigned char *)data.constData(), data.size(),
                                (const unsigned char *)key.constData(), key.size(),
                                (const unsigned char *)aad.constData(), aad.size(), decrypt))
            .toLatin1());
}
QByteArray seal(const QByteArray &data, const QByteArray &key, const QByteArray &aad) {
    return crypt(data, key, aad, false);
}
QByteArray open(const QByteArray &data, const QByteArray &key, const QByteArray &aad, bool *ok) {
    auto clear = crypt(data, key, aad, true);
    *ok = !clear.isEmpty();
    return clear;
}
void verify(const QString &reason, std::function<void(bool)> callback) {
    if (MobileModel::instance)
        MobileModel::instance->verificationActive = true;
    auto cb = new std::function<void(bool)>(std::move(callback));
    arbiter_ios_verify(
        reason.toUtf8().constData(),
        [](int result, void *pointer) {
            auto cb =
                std::unique_ptr<std::function<void(bool)>>(static_cast<std::function<void(bool)> *>(pointer));
            auto function = *cb;
            QMetaObject::invokeMethod(qApp, [function, result] {
                function(result != 0);
                QTimer::singleShot(500, qApp, [] {
                    if (MobileModel::instance)
                        MobileModel::instance->verificationActive = false;
                });
            });
        },
        cb);
}
void refreshIdentities() {
    arbiter_ios_refresh_identities();
}
void setLanguage(const QString &code) {
    arbiter_ios_language(code.toUtf8().constData());
}
QString legacySession() {
    return take(arbiter_ios_legacy_session());
}
void share(const QUrl &url) {
    auto controller = [[UIActivityViewController alloc] initWithActivityItems:@[ url.toNSURL() ]
                                                        applicationActivities:nil];
    controller.popoverPresentationController.sourceView = presenter().view;
    [presenter() presentViewController:controller animated:YES completion:nil];
}
void secureScreen(bool value) {
    protectedScreen = value;
}
void configureBackgroundSync(bool value) {
    [NSUserDefaults.standardUserDefaults setBool:value forKey:@"arbiter.backgroundSync"];
    [BGTaskScheduler.sharedScheduler cancelTaskRequestWithIdentifier:@"com.arbiter.mobile.sync"];
    if (!value)
        return;
    auto request = [[BGAppRefreshTaskRequest alloc] initWithIdentifier:@"com.arbiter.mobile.sync"];
    request.earliestBeginDate = [NSDate dateWithTimeIntervalSinceNow:15 * 60];
    [BGTaskScheduler.sharedScheduler submitTaskRequest:request error:nil];
}
QWindow *createWebView(NativePage *page, const QString &profile, bool privateMode) {
    ArbiterWeb *bridge = [[ArbiterWeb alloc] initWithPage:page
                                                  profile:string(profile)
                                              privateMode:privateMode];
    page->handle = (__bridge_retained void *)bridge;
    return QWindow::fromWinId(WId((__bridge void *)bridge.view));
}
void destroyWebView(NativePage *page) {
    if (page->handle) {
        ArbiterWeb *bridge = (__bridge_transfer ArbiterWeb *)page->handle;
        bridge->page = nullptr;
        [bridge.view.configuration.userContentController
            removeScriptMessageHandlerForName:@"arbiterForms"
                                 contentWorld:WKContentWorld.defaultClientWorld];
        page->handle = nullptr;
    }
}
void mediaPlaying(NativePage *page, std::function<void(bool)> callback) {
    ArbiterWeb *bridge = (__bridge ArbiterWeb *)page->handle;
    if (!bridge) {
        callback(false);
        return;
    }
    [bridge.view requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState state) {
      callback(state == WKMediaPlaybackStatePlaying);
    }];
}
void setOverlayRect(NativePage *page, const QRectF &rect) {
    if (!page->handle)
        return;
    ArbiterWeb *bridge = (__bridge ArbiterWeb *)page->handle;
    bridge.view.overlayFraction = CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
}
void capture(NativePage *page) {
    if (!page->handle)
        return;
    ArbiterWeb *bridge = (__bridge ArbiterWeb *)page->handle;
    [bridge captureForPreview];
}
void find(NativePage *page, const QString &text, bool backwards) {
    if (!page->handle)
        return;
    ArbiterWeb *bridge = (__bridge ArbiterWeb *)page->handle;
    WKFindConfiguration *config = [WKFindConfiguration new];
    config.backwards = backwards;
    config.wraps = YES;
    [bridge.view findString:string(text)
          withConfiguration:config
          completionHandler:^(WKFindResult *){
          }];
}
void configurePage(NativePage *page) {
    ArbiterWeb *bridge = (__bridge ArbiterWeb *)page->handle;
    [bridge configure];
}
void navigate(NativePage *page, const QUrl &url) {
    if (!page->handle || !QStringList{"http", "https"}.contains(url.scheme()) || !url.userInfo().isEmpty())
        return;
    ArbiterWeb *bridge = (__bridge ArbiterWeb *)page->handle;
    [bridge configure];
    NSUInteger generation = ++bridge.navigationGeneration;
    __weak ArbiterWeb *weak = bridge;
    prepareRules(^{
      ArbiterWeb *current = weak;
      if (!current || current.navigationGeneration != generation || !current->page)
          return;
      [current configure];
      [current.view loadRequest:[NSURLRequest requestWithURL:url.toNSURL()]];
    });
}
void action(NativePage *page, const QString &name) {
    ArbiterWeb *bridge = (__bridge ArbiterWeb *)page->handle;
    if (name == "back")
        [bridge.view goBack];
    if (name == "forward")
        [bridge.view goForward];
    if (name == "reload")
        [bridge.view reload];
}
void evaluate(NativePage *page, const QString &script, std::function<void(QVariant)> callback) {
    ArbiterWeb *bridge = (__bridge ArbiterWeb *)page->handle;
    if (!bridge) {
        if (callback)
            callback({});
        return;
    }
    [bridge.view
        evaluateJavaScript:string(script)
                   inFrame:nil
            inContentWorld:WKContentWorld.defaultClientWorld
         completionHandler:^(id value, NSError *) {
           if (callback)
               callback([value isKindOfClass:NSNumber.class] ? QVariant([(NSNumber *)value boolValue])
                                                             : QVariant());
         }];
}
} // namespace MobilePlatform
