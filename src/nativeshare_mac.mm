#include "nativeshare.h"
#include <QGuiApplication>
#import <AppKit/AppKit.h>

@interface ArbiterShareDelegate : NSObject <NSSharingServicePickerDelegate> {
@public
    QPointer<NativeShare> controller;
    quint64 generation;
}
@property(nonatomic,strong) NSSharingServicePicker *picker;
@property(nonatomic,strong) NSSharingService *clipboardService;
@end
@implementation ArbiterShareDelegate
- (NSArray<NSSharingService *> *)sharingServicePicker:(NSSharingServicePicker *)picker sharingServicesForItems:(NSArray *)items proposedSharingServices:(NSArray<NSSharingService *> *)services {
    Q_UNUSED(picker); Q_UNUSED(items);
    NSMutableArray<NSSharingService *> *result=[services mutableCopy];
    NSImage *image=[NSImage imageWithSystemSymbolName:@"doc.on.doc" accessibilityDescription:@"Copy link"];
    self.clipboardService=[[NSSharingService alloc] initWithTitle:QObject::tr("Copy link").toNSString() image:image alternateImage:nil handler:^{}];
    [result insertObject:self.clipboardService atIndex:0];
    return result;
}
- (id<NSSharingServiceDelegate>)sharingServicePicker:(NSSharingServicePicker *)picker delegateForSharingService:(NSSharingService *)service {
    Q_UNUSED(picker);
    if (controller) service.subject=controller->title().toNSString();
    return nil;
}
- (void)sharingServicePicker:(NSSharingServicePicker *)picker didChooseSharingService:(NSSharingService *)service {
    Q_UNUSED(picker);
    if (controller) {
        if (service && service==self.clipboardService) controller->copyLink(generation);
        controller->finished((__bridge void *)self,generation);
    }
}
@end
void *createNativeShare(NativeShare *controller,QString *error) {
    if (!controller->host() || QGuiApplication::platformName()!=QStringLiteral("cocoa")) { *error=QObject::tr("Native sharing requires a macOS desktop window."); return nullptr; }
    auto *delegate=[[ArbiterShareDelegate alloc] init];
    delegate->controller=controller;
    delegate->generation=controller->generation();
    NSURL *url=[NSURL URLWithString:controller->link().toString(QUrl::FullyEncoded).toNSString()];
    if (!url) { *error=QObject::tr("This page link cannot be shared."); return nullptr; }
    delegate.picker=[[NSSharingServicePicker alloc] initWithItems:@[url]];
    delegate.picker.delegate=delegate;
    return (__bridge_retained void *)delegate;
}
bool presentNativeShare(void *state,const QPointF &anchor,QString *error) {
    auto *delegate=(__bridge ArbiterShareDelegate *)state;
    if (!delegate->controller || !delegate->controller->canShare()) return false;
    NSView *view=(__bridge NSView *)reinterpret_cast<void *>(delegate->controller->host()->winId());
    if (!view || !view.window) { *error=QObject::tr("The browser window is not available for sharing."); return false; }
    const CGFloat x=qBound(16.0,anchor.x(),qMax(16.0,view.bounds.size.width-16.0));
    const CGFloat top=qBound(16.0,anchor.y(),qMax(16.0,view.bounds.size.height-16.0));
    const CGFloat y=view.isFlipped ? top : view.bounds.size.height-top;
    [delegate.picker showRelativeToRect:NSMakeRect(x-1,y-1,2,2) ofView:view preferredEdge:view.isFlipped ? NSMinYEdge : NSMaxYEdge];
    return true;
}
void destroyNativeShare(void *state) {
    auto *delegate=(__bridge_transfer ArbiterShareDelegate *)state;
    delegate.picker.delegate=nil;
    [delegate.picker close];
    delegate->controller=nullptr;
}
