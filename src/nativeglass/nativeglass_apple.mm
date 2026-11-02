#include "nativeglasshost.h"
#include "surfacepalette.h"
#include <TargetConditionals.h>
#include <QGuiApplication>
#include <QAbstractNativeEventFilter>
namespace NativeGlass { class PopupEventFilter; }
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

namespace NativeGlass {
bool nativeKeyboardAvailable() {
#if TARGET_OS_IPHONE
    return qGuiApp && qGuiApp->platformName() == "ios";
#else
    return false;
#endif
}
void watchKeyboard(SurfacePalette *palette) {
#if TARGET_OS_IPHONE
    if (!nativeKeyboardAvailable()) return;
    QPointer<SurfacePalette> guarded(palette);
    auto token = [NSNotificationCenter.defaultCenter addObserverForName:UIKeyboardWillChangeFrameNotification
        object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) {
            if (!guarded) return;
            const auto frame = [note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
            const int duration = qRound([note.userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue] * 1000);
            const int curve = [note.userInfo[UIKeyboardAnimationCurveUserInfoKey] intValue];
            guarded->setKeyboardFrame(QRectF(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height), duration, curve);
        }];
    QObject::connect(palette, &QObject::destroyed, [token] { [NSNotificationCenter.defaultCenter removeObserver:token]; });
#else
    Q_UNUSED(palette);
#endif
}
qreal keyboardInset(QQuickItem *item, const QRectF &frame) {
#if TARGET_OS_IPHONE
    if (!nativeKeyboardAvailable() || !item || !item->window() || frame.isEmpty()) return 0;
    UIView *view = (__bridge UIView *)(void *)item->window()->winId();
    if (!view.window) return 0;
    const auto screen = CGRectMake(frame.x(), frame.y(), frame.width(), frame.height());
    const auto windowFrame = [view.window convertRect:screen fromCoordinateSpace:view.window.screen.coordinateSpace];
    const auto local = [view convertRect:windowFrame fromView:view.window];
    // A floating iPad keyboard should not push the entire browser upward.
    if (!CGRectIntersectsRect(view.bounds, local) || CGRectGetMaxY(local) < CGRectGetMaxY(view.bounds) - 1) return 0;
    const auto point = item->mapFromScene(QPointF(local.origin.x, local.origin.y));
    return qMax(0., item->height() - point.y());
#else
    Q_UNUSED(item); Q_UNUSED(frame); return 0;
#endif
}
}

#if (TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000) || (!TARGET_OS_IPHONE && __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000)
@interface ArbiterNativeGlassContext : NSObject
#if TARGET_OS_IPHONE
@property(nonatomic, weak) UIView *owner;
@property(nonatomic, weak) UIView *foreground;
@property(nonatomic, strong) UIVisualEffectView *effect;
@property(nonatomic, strong) UIView *containerView;
#else
@property(nonatomic) BOOL windowSurface;
@property(nonatomic) NativeGlass::PopupEventFilter *outsideFilter;
@property(nonatomic, weak) NSWindow *hostWindow;
@property(nonatomic, weak) NSView *owner;
@property(nonatomic, weak) NSView *foreground;
@property(nonatomic, strong) NSView *containerView;
@property(nonatomic, strong) NSGlassEffectView *effect API_AVAILABLE(macos(26.0));
#endif
@property(nonatomic) QWindow *container;
@property(nonatomic) BOOL fixedRenderTarget;
@end
@implementation ArbiterNativeGlassContext
@end

namespace NativeGlass {
#if !TARGET_OS_IPHONE
class PopupEventFilter final : public QAbstractNativeEventFilter {
public:
    PopupEventFilter(NSWindow *nativeWindow, NSView *view, QWindow *window, NativeGlassHost *host)
        : m_nativeWindow(nativeWindow), m_view(view), m_window(window), m_host(host) {}
    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *) override {
        if (type != "mac_generic_NSEvent" || !m_host || !m_window || !m_nativeWindow || !m_view) return false;
        NSEvent *event = (__bridge NSEvent *)message;
        const bool pressed = event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown || event.type == NSEventTypeOtherMouseDown;
        const bool released = event.type == NSEventTypeLeftMouseUp || event.type == NSEventTypeRightMouseUp || event.type == NSEventTypeOtherMouseUp;
        if ((!pressed && !released) || !event.window) return false;
        // The backing popup is modeless at the Cocoa level, so native events
        // aimed at its parent reach this public filter before view delivery.
        const auto screen = [event.window convertPointToScreen:event.locationInWindow];
        const auto native = [m_nativeWindow convertPointFromScreen:screen];
        const auto point = [m_view convertPoint:native fromView:nil];
        const auto y = m_view.isFlipped ? point.y : m_view.bounds.size.height - point.y;
        return m_host->handleOutsidePointer(m_window->mapToGlobal(QPointF(point.x, y)), pressed);
    }
private:
    __weak NSWindow *m_nativeWindow;
    __weak NSView *m_view;
    QPointer<QWindow> m_window;
    QPointer<NativeGlassHost> m_host;
};
#endif
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
void *createWindow(QQuickWindow *owner, NativeGlassHost *host) {
#if !TARGET_OS_IPHONE
    if (@available(macOS 26.0, *)) {
        if (!supported() || (owner->type() != Qt::Popup && owner->type() != Qt::ToolTip)) return nullptr;
        auto context = [ArbiterNativeGlassContext new];
        context.windowSurface = YES;
        context.foreground = (__bridge NSView *)(void *)owner->winId();
        context.owner = context.foreground.superview;
        context.hostWindow = context.foreground.window;
        if (!context.hostWindow) return nullptr;
        context.effect = [NSGlassEffectView new];
        context.effect.style = NSGlassEffectViewStyleRegular;
        context.effect.tintColor = nil;
        context.hostWindow.opaque = NO;
        context.hostWindow.backgroundColor = NSColor.clearColor;
        context.hostWindow.hasShadow = YES;
        [context.owner addSubview:context.effect positioned:NSWindowBelow relativeTo:context.foreground];
        context.effect.frame = context.foreground.frame;
        context.outsideFilter = new PopupEventFilter(context.hostWindow, context.foreground, owner, host);
        qGuiApp->installNativeEventFilter(context.outsideFilter);
        return (__bridge_retained void *)context;
    }
#else
    Q_UNUSED(owner);
    Q_UNUSED(host);
#endif
    return nullptr;
}
QWindow *container(void *value) {
    return ((__bridge ArbiterNativeGlassContext *)value).container;
}
void update(void *value, const QRectF &rect, qreal radius, const QSizeF &minimumRenderSize) {
    auto context = (__bridge ArbiterNativeGlassContext *)value;
#if TARGET_OS_IPHONE
    if (@available(iOS 26.0, *)) {
        context.fixedRenderTarget = minimumRenderSize.isValid();
        [UIView performWithoutAnimation:^{
            context.containerView.frame = CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
            context.effect.frame = context.containerView.bounds;
            context.effect.layer.cornerRadius = radius;
            context.effect.layer.cornerCurve = kCACornerCurveContinuous;
            context.effect.cornerConfiguration = [UICornerConfiguration configurationWithRadius:[UICornerRadius fixedRadius:radius]];
            context.foreground.frame = CGRectMake(0, 0, MAX(rect.width(), minimumRenderSize.width()),
                                                 MAX(rect.height(), minimumRenderSize.height()));
        }];
        [context.owner bringSubviewToFront:context.containerView];
    }
#else
    if (@available(macOS 26.0, *)) {
        if (context.windowSurface) {
            // Leave Qt's content view at the NSWindow root. Reparenting it
            // changes Cocoa geometry notifications and breaks nested popups.
            if (context.effect.superview != context.foreground.superview) {
                context.owner = context.foreground.superview;
                [context.owner addSubview:context.effect positioned:NSWindowBelow relativeTo:context.foreground];
            }
            context.effect.frame = context.foreground.frame;
            context.effect.cornerRadius = radius;
            return;
        }
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
void setOpacity(void *value, qreal opacity) {
    auto context = (__bridge ArbiterNativeGlassContext *)value;
#if TARGET_OS_IPHONE
    context.containerView.alpha = opacity;
#else
    if (context.windowSurface) context.effect.alphaValue = opacity;
    else context.containerView.alphaValue = opacity;
#endif
}
void activateForTest(QQuickWindow *window) {
    if (!supported()) { window->requestActivate(); return; }
#if !TARGET_OS_IPHONE
    NSView *view = (__bridge NSView *)(void *)window->winId();
    [NSApp activateIgnoringOtherApps:YES];
    [view.window makeKeyAndOrderFront:nil];
#endif
    window->requestActivate();
}
bool clickForTest(QWindow *target, const QPointF &global) {
#if !TARGET_OS_IPHONE
    if (!supported()) return false;
    NSView *view = (__bridge NSView *)(void *)target->winId();
    const auto local = target->mapFromGlobal(global);
    const auto y = view.isFlipped ? local.y() : view.bounds.size.height - local.y();
    const auto location = [view convertPoint:NSMakePoint(local.x(), y) toView:nil];
    for (auto type : {NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp}) {
        auto event = [NSEvent mouseEventWithType:type location:location modifierFlags:0
            timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:view.window.windowNumber
            context:nil eventNumber:0 clickCount:1 pressure:type == NSEventTypeLeftMouseDown ? 1 : 0];
        [NSApp postEvent:event atStart:NO];
    }
    return true;
#else
    Q_UNUSED(target); Q_UNUSED(global); return false;
#endif
}
bool geometryAligned(void *value) {
    auto context = (__bridge ArbiterNativeGlassContext *)value;
    const auto glass = [context.effect convertRect:context.effect.bounds toView:context.owner];
    const auto controls = [context.foreground convertRect:context.foreground.bounds toView:context.owner];
    const bool aligned = qAbs(glass.origin.x - controls.origin.x) < 1 && qAbs(glass.origin.y - controls.origin.y) < 1
        && (context.fixedRenderTarget
            ? controls.size.width + 1 >= glass.size.width && controls.size.height + 1 >= glass.size.height
            : qAbs(glass.size.width - controls.size.width) < 1 && qAbs(glass.size.height - controls.size.height) < 1);
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
#if !TARGET_OS_IPHONE
    if (context.windowSurface) {
        delete context.outsideFilter;
        context.outsideFilter = nullptr;
        [context.effect removeFromSuperview];
        return;
    }
#endif
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
void *createWindow(QQuickWindow *, NativeGlassHost *) { return nullptr; }
QWindow *container(void *) { return nullptr; }
void update(void *, const QRectF &, qreal, const QSizeF &) {}
void destroy(void *) {}
void setVisible(void *, bool) {}
void setOpacity(void *, qreal) {}
bool geometryAligned(void *) { return false; }
void activateForTest(QQuickWindow *window) { window->requestActivate(); }
bool clickForTest(QWindow *, const QPointF &) { return false; }
QColor foregroundColor(void *) { return Qt::black; }
}
#endif
