#import <AppKit/AppKit.h>

// QPrintDialog's QWidget reject() does not stop NSPrintPanel's modal loop.
// Abort that loop before releasing the Qt dialog or its printer.
void abortNativePrintDialog()
{
    if (NSApp.modalWindow)
        [NSApp abortModal];
}
