#include "browsericons.h"
#include <QIcon>
#include <QImage>
#include <QPixmap>
#import <AppKit/AppKit.h>

QIcon nativeBrowserAppIcon(const QString &path) {
    @autoreleasepool {
        NSImage *image=[NSWorkspace.sharedWorkspace iconForFile:path.toNSString()];
        NSRect bounds=NSMakeRect(0,0,128,128);
        CGImageRef pixels=[image CGImageForProposedRect:&bounds context:nil hints:nil];
        if(!pixels) return {};
        NSBitmapImageRep *bitmap=[[NSBitmapImageRep alloc] initWithCGImage:pixels];
        NSData *png=[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        const auto result=QImage::fromData(static_cast<const uchar *>(png.bytes),int(png.length),"PNG");
        [bitmap release];
        return result.isNull() ? QIcon() : QIcon(QPixmap::fromImage(result));
    }
}
