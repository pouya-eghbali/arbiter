#import <AppKit/AppKit.h>
#include <QPrintDialog>

int execNativePrintDialog(QPrintDialog *dialog)
{
    @autoreleasepool {
        // Qt's explicit color-scheme preference sets NSApp.appearance. Resolve
        // the inherited system appearance synchronously, then restore the app
        // override before any dialog event loop or drawing is allowed to run.
        NSAppearance *override = [NSApp.appearance retain];
        NSApp.appearance = nil;
        NSAppearance *system = [NSApp.effectiveAppearance retain];
        NSApp.appearance = override;
        [override release];

        // NSPrintPanel creates its own native window inside QPrintDialog::exec.
        // Scope the override to that modal window (and any PDF save sheet),
        // rather than changing the appearance of all browser windows.
        NSMutableArray<NSWindow *> *windows = [NSMutableArray array];
        NSMutableArray *appearances = [NSMutableArray array];
        void (^applyAppearance)(NSNotification *) = ^(NSNotification *) {
            NSWindow *window = NSApp.modalWindow;
            if (!window || [windows containsObject:window]) return;
            [windows addObject:window];
            [appearances addObject:window.appearance ?: NSNull.null];
            window.appearance = system;
        };
        NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
        id keyObserver = [center addObserverForName:NSWindowDidBecomeKeyNotification object:nil queue:nil usingBlock:applyAppearance];
        id updateObserver = [center addObserverForName:NSApplicationDidUpdateNotification object:NSApp queue:nil usingBlock:applyAppearance];
        const int result = dialog->exec();
        [center removeObserver:keyObserver];
        [center removeObserver:updateObserver];
        for (NSUInteger i = 0; i < windows.count; ++i) {
            id appearance = appearances[i];
            windows[i].appearance = appearance == NSNull.null ? nil : appearance;
        }
        [system release];
        return result;
    }
}

// QPrintDialog's QWidget reject() does not stop NSPrintPanel's modal loop.
// Abort that loop before releasing the Qt dialog or its printer.
void abortNativePrintDialog()
{
    if (NSApp.modalWindow)
        [NSApp abortModal];
}
