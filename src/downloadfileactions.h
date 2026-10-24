#pragma once
#include <QString>
#include <QUrl>
QString markDownloadedFile(const QString &path, const QUrl &source);
bool revealDownloadedFile(const QString &path);
