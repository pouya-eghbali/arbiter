#include "browserimporter.h"
#include <QGuiApplication>
#include <QWindow>
#include <QPointer>
#include <QMap>
#import <AppKit/AppKit.h>

QStringList installedMacBrowsers() {
    const QMap<QString,QString> identifiers{{"Chrome","com.google.Chrome"},{"Firefox","org.mozilla.firefox"},
        {"Edge","com.microsoft.edgemac"},{"Brave","com.brave.Browser"},{"Chromium","org.chromium.Chromium"},
        {"Vivaldi","com.vivaldi.Vivaldi"},{"Arc","company.thebrowser.Browser"},{"Safari","com.apple.Safari"}};
    QStringList result;
    for(auto it=identifiers.cbegin();it!=identifiers.cend();++it) {
        NSURL *url=[NSWorkspace.sharedWorkspace URLForApplicationWithBundleIdentifier:it.value().toNSString()];
        if(url && [NSFileManager.defaultManager fileExistsAtPath:url.path]) result.append(it.key());
    }
    return result;
}
bool macRequiresBrowserDataConsent() { return NSProcessInfo.processInfo.operatingSystemVersion.majorVersion>=27; }

void requestMacBrowserAccess(QObject *lifetime,QWindow *window,const QUrl &directory,BrowserImporter::AccessCompletion completion) {
    if(QGuiApplication::platformName()!=QStringLiteral("cocoa") || !window || !window->isVisible()) {
        completion(false,QObject::tr("Browser access could not open because its window is unavailable. Try again from the import screen."));
        return;
    }
    NSView *view=reinterpret_cast<NSView *>(window->winId());
    NSWindow *parent=view.window;
    if(!parent || parent.attachedSheet) {
        completion(false,QObject::tr("Close any open system dialog, then try Allow browser access again."));
        return;
    }
    NSOpenPanel *panel=[NSOpenPanel openPanel];
    panel.canChooseDirectories=YES;
    panel.canChooseFiles=NO;
    panel.allowsMultipleSelection=NO;
    panel.directoryURL=[NSURL fileURLWithPath:directory.toLocalFile().toNSString() isDirectory:YES];
    panel.title=QObject::tr("Allow access to browser data").toNSString();
    panel.message=QObject::tr("Allow access to this browser’s data folder. Arbiter will find its profiles automatically; passwords and history are imported only when you continue.").toNSString();
    panel.prompt=QObject::tr("Allow access").toNSString();
    QPointer<QObject> owner(lifetime);
    const auto expected=directory;
    [panel retain];
    [panel beginSheetModalForWindow:parent completionHandler:^(NSModalResponse result) {
        if(result!=NSModalResponseOK || !owner) { completion(false,{}); [panel release]; return; }
        NSURL *selected=panel.URL;
        NSURL *required=[NSURL fileURLWithPath:expected.toLocalFile().toNSString() isDirectory:YES];
        if(!selected || ![selected.URLByResolvingSymlinksInPath.URLByStandardizingPath isEqual:required.URLByResolvingSymlinksInPath.URLByStandardizingPath]) {
            completion(false,QObject::tr("Select the browser data folder shown when the access dialog opens. Access to a parent folder does not authorize its protected browser data."));
            [panel release]; return;
        }
        // Keep the actual URL returned by the consent panel alive for the importer.
        // A non-sandboxed app can receive a TCC grant even when this returns NO.
        const BOOL scoped=[selected startAccessingSecurityScopedResource];
        [selected retain];
        QObject::connect(owner,&QObject::destroyed,[selected,scoped] {
            if(scoped) [selected stopAccessingSecurityScopedResource];
            [selected release];
        });
        completion(true,{});
        [panel release];
    }];
}
