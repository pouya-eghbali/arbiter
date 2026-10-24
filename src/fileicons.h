#pragma once
#include <QString>
#include <QUrl>
QUrl fileTypeIcon(const QString &fileName);
#ifdef Q_OS_MACOS
#include <QImage>
QImage nativeFileTypeIcon(const QString &extension);
#endif
