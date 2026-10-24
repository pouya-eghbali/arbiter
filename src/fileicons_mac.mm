#include "fileicons.h"
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

QImage nativeFileTypeIcon(const QString &extension)
{
    @autoreleasepool {
        UTType *type = extension.isEmpty() ? UTTypeData : [UTType typeWithFilenameExtension:extension.toNSString()];
        NSImage *icon = [NSWorkspace.sharedWorkspace iconForContentType:type ?: UTTypeData];
        NSRect bounds = NSMakeRect(0, 0, 96, 96);
        CGImageRef pixels = [icon CGImageForProposedRect:&bounds context:nil hints:nil];
        if (!pixels) return {};
        NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithCGImage:pixels];
        NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        const QImage result = QImage::fromData(static_cast<const uchar *>(png.bytes), int(png.length), "PNG");
        [bitmap release];
        return result;
    }
}
