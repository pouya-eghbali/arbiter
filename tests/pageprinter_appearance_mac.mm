#include <QPrintDialog>
#include <QPrinter>
#include <QString>
#import <AppKit/AppKit.h>

int execNativePrintDialog(QPrintDialog *dialog);
void abortNativePrintDialog();

QString checkNativePrintAppearance()
{
    @autoreleasepool {
        NSAppearance *saved = [NSApp.appearance retain];
        NSApp.appearance = nil;
        NSString *system = [[NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] copy];
        QString error;
        // Vary only this application's appearance; never change the user's OS.
        for (NSString *mode in @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]) {
            NSApp.appearance = [NSAppearance appearanceNamed:mode];
            QPrinter printer;
            QPrintDialog dialog(&printer);
            __block bool seen = false, matched = false, preserved = false;
            NSTimer *timer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *) {
                if (!NSApp.modalWindow || seen) return;
                seen = true;
                NSArray *names = @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua];
                matched = [[NSApp.modalWindow.effectiveAppearance bestMatchFromAppearancesWithNames:names] isEqualToString:system];
                preserved = [[NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:names] isEqualToString:mode];
                abortNativePrintDialog();
            }];
            [NSRunLoop.currentRunLoop addTimer:timer forMode:NSModalPanelRunLoopMode];
            [NSRunLoop.currentRunLoop addTimer:timer forMode:NSDefaultRunLoopMode];
            execNativePrintDialog(&dialog);
            [timer invalidate];
            if (!seen || !matched || !preserved || ![NSApp.appearance.name isEqualToString:mode]) {
                error = QStringLiteral("Native print appearance did not follow the system while preserving %1 (seen=%2, matched=%3, preserved=%4)")
                    .arg(QString::fromNSString(mode)).arg(seen).arg(matched).arg(preserved);
                break;
            }
        }
        NSApp.appearance = saved;
        [saved release]; [system release];
        return error;
    }
}
