#pragma once
#include <QQuickImageProvider>
QQuickImageProvider *createBrowserIconProvider();

#ifdef Q_OS_MACOS
#include <QIcon>
QIcon nativeBrowserAppIcon(const QString &path);
#endif
