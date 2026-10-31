#include "nativeglasshost.h"
#include <TargetConditionals.h>
#include <QGuiApplication>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#if (TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000) || (!TARGET_OS_IPHONE && __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000)
@interface ArbiterNativeGlassContext : NSObject
#if TARGET_OS_IPHONE
@property(nonatomic, weak) UIView *owner;
@property(nonatomic, weak) UIView *foreground;
@property(nonatomic, strong) UIVisualEffectView *effect;
@property(nonatomic, strong) UIView *containerView;
#else
@property(nonatomic, weak) NSView *owner;
@property(nonatomic, weak) NSView *foreground;
@property(nonatomic, strong) NSView *containerView;
@property(nonatomic, strong) NSGlassEffectView *effect API_AVAILABLE(macos(26.0));
#endif
@property(nonatomic) QWindow *container;
@end
@implementation ArbiterNativeGlassContext
@end

namespace NativeGlass {
bool supported() {
    if (!qGuiApp || (qGuiApp->platformName() != "cocoa" && qGuiApp->platformName() != "ios")) return false;
#if TARGET_OS_IPHONE
    if (@available(iOS 26.0, *)) return true;
#else
    if (@available(macOS 26.0, *)) return true;
#endif
    return false;
}
bool reducedTransparency() {
#if TARGET_OS_IPHONE
    return UIAccessibilityIsReduceTransparencyEnabled();
#else
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceTransparency;
#endif
}
void *create(QQuickWindow *owner, QQuickWindow *foreground) {
    if (!supported()) return nullptr;
    auto context = [ArbiterNativeGlassContext new];
#if TARGET_OS_IPHONE
    if (@available(iOS 26.0, *)) {
        context.owner = (__bridge UIView *)(void *)owner->winId();
        context.foreground = (__bridge UIView *)(void *)foreground->winId();
        auto effect = [UIGlassEffect effectWithStyle:UIGlassEffectStyleRegular];
        effect.interactive = YES;
        effect.tintColor = nil;
        context.effect = [[UIVisualEffectView alloc] initWithEffect:effect];
        context.foreground.opaque = NO;
        context.foreground.backgroundColor = UIColor.clearColor;
        context.containerView = [UIView new];
        [context.containerView addSubview:context.effect];
        [context.owner addSubview:context.containerView];
        context.container = QWindow::fromWinId(WId((__bridge void *)context.containerView));
        context.container->setParent(owner);
        foreground->setParent(context.container);
        [context.effect.contentView addSubview:context.foreground];
    }
#else
    if (@available(macOS 26.0, *)) {
        context.owner = (__bridge NSView *)(void *)owner->winId();
        context.foreground = (__bridge NSView *)(void *)foreground->winId();
        context.effect = [NSGlassEffectView new];
        context.effect.style = NSGlassEffectViewStyleRegular;
        context.effect.tintColor = nil;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 270000
        if (@available(macOS 27.0, *)) context.effect.effectIsInteractive = YES;
#endif
        context.containerView = [NSView new];
        [context.containerView addSubview:context.effect];
        [context.owner addSubview:context.containerView positioned:NSWindowAbove relativeTo:nil];
        context.container = QWindow::fromWinId(WId((__bridge void *)context.containerView));
        context.container->setParent(owner);
        foreground->setParent(context.container);
        context.effect.contentView = context.foreground;
    }
#endif
    return (__bridge_retained void *)context;
}
QWindow *container(void *value) {
    return ((__bridge ArbiterNativeGlassContext *)value).container;
}
void update(void *value, const QRectF &rect, qreal radius) {
    auto context = (__bridge ArbiterNativeGlassContext *)value;
#if TARGET_OS_IPHONE
    if (@available(iOS 26.0, *)) {
        context.containerView.frame = CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
        context.effect.frame = context.containerView.bounds;
        context.effect.layer.cornerRadius = radius;
        context.effect.layer.cornerCurve = kCACornerCurveContinuous;
        context.effect.cornerConfiguration = [UICornerConfiguration configurationWithRadius:[UICornerRadius fixedRadius:radius]];
        context.foreground.frame = context.effect.contentView.bounds;
        [context.owner bringSubviewToFront:context.containerView];
    }
#else
    if (@available(macOS 26.0, *)) {
        const auto y = context.owner.isFlipped ? rect.y() : context.owner.bounds.size.height - rect.bottom();
        context.containerView.frame = NSMakeRect(rect.x(), y, rect.width(), rect.height());
        context.effect.frame = context.containerView.bounds;
        context.effect.cornerRadius = radius;
        // Cocoa may restore the QWindow's native parent while showing it.
        // Keep its Qt parent at the material container and its native content
        // inside the effect, with the same local origin on both sides.
        if (![context.foreground isDescendantOf:context.effect]) {
            context.effect.contentView = nil;
            context.effect.contentView = context.foreground;
        }
        context.foreground.frame = context.effect.bounds;
    }
#endif
}
void setVisible(void *value, bool visible) {
    auto context = (__bridge ArbiterNativeGlassContext *)value;
    context.effect.hidden = !visible;
}
bool geometryAligned(void *value) {
    auto context = (__bridge ArbiterNativeGlassContext *)value;
    const auto glass = [context.effect convertRect:context.effect.bounds toView:context.owner];
    const auto controls = [context.foreground convertRect:context.foreground.bounds toView:context.owner];
    const bool aligned = qAbs(glass.origin.x - controls.origin.x) < 1 && qAbs(glass.origin.y - controls.origin.y) < 1
        && qAbs(glass.size.width - controls.size.width) < 1 && qAbs(glass.size.height - controls.size.height) < 1;
    if (!aligned) qWarning("Native glass and controls disagree: glass %.1f,%.1f %.1fx%.1f; controls %.1f,%.1f %.1fx%.1f", glass.origin.x, glass.origin.y, glass.size.width, glass.size.height, controls.origin.x, controls.origin.y, controls.size.width, controls.size.height);
    return aligned;
}
QColor foregroundColor(void *value) {
    auto context = (__bridge ArbiterNativeGlassContext *)value;
#if TARGET_OS_IPHONE
    CGFloat r = 0, g = 0, b = 0, a = 1;
    [[UIColor.labelColor resolvedColorWithTraitCollection:context.effect.traitCollection] getRed:&r green:&g blue:&b alpha:&a];
    return QColor::fromRgbF(r, g, b, a);
#else
    if (@available(macOS 26.0, *)) {
        __block NSColor *color;
        [context.effect.effectiveAppearance performAsCurrentDrawingAppearance:^{
            color = [NSColor.labelColor colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace];
        }];
        return QColor::fromRgbF(color.redComponent, color.greenComponent, color.blueComponent, color.alphaComponent);
    }
    return Qt::black;
#endif
}
void destroy(void *value) {
    auto context = (__bridge_transfer ArbiterNativeGlassContext *)value;
    [context.foreground removeFromSuperview];
    if (context.owner && context.foreground) [context.owner addSubview:context.foreground];
    [context.effect removeFromSuperview];
    [context.containerView removeFromSuperview];
}
}

#else
namespace NativeGlass {
bool supported() { return false; }
bool reducedTransparency() { return false; }
void *create(QQuickWindow *, QQuickWindow *) { return nullptr; }
QWindow *container(void *) { return nullptr; }
void update(void *, const QRectF &, qreal) {}
void destroy(void *) {}
void setVisible(void *, bool) {}
bool geometryAligned(void *) { return false; }
QColor foregroundColor(void *) { return Qt::black; }
}
#endif
