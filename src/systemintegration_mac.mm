#include "systemintegration.h"
#import <AppKit/AppKit.h>

bool macReducedMotion() { return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion; }
bool macIsDefaultBrowser() {
    NSURL *bundle=NSBundle.mainBundle.bundleURL;
    for(NSString *scheme in @[@"http",@"https"]) {
        NSURL *handler=[NSWorkspace.sharedWorkspace URLForApplicationToOpenURL:[NSURL URLWithString:[scheme stringByAppendingString:@"://example.invalid"]]];
        if(!handler || ![handler.URLByResolvingSymlinksInPath isEqual:bundle.URLByResolvingSymlinksInPath]) return false;
    }
    return true;
}
void macRequestDefaultBrowser(std::function<void(QString)> completion) {
    NSURL *bundle=NSBundle.mainBundle.bundleURL;
    if(![bundle.pathExtension isEqualToString:@"app"]) { completion(QObject::tr("Launch the Arbiter app bundle to set it as your default browser.")); return; }
    [NSWorkspace.sharedWorkspace setDefaultApplicationAtURL:bundle toOpenURLsWithScheme:@"http" completionHandler:^(NSError *error) {
        if(error) { completion(QString::fromNSString(error.localizedDescription)); return; }
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSWorkspace.sharedWorkspace setDefaultApplicationAtURL:bundle toOpenURLsWithScheme:@"https" completionHandler:^(NSError *secondError) {
                completion(secondError ? QString::fromNSString(secondError.localizedDescription) : QString());
            }];
        });
    }];
}
