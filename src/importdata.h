#pragma once
#include <QDateTime>
#include <QList>
#include <QUrl>

struct ImportedPassword { QUrl url; QString username,password; };
struct ImportedVisit { QUrl url; QString title; QDateTime time; };
struct ImportPreview {
    QList<ImportedPassword> passwords;
    QList<ImportedVisit> visits;
    int skippedPasswords=0,skippedVisits=0;
    QString error;
};
namespace BrowserImport {
ImportPreview read(const QString &passwordFile,const QString &historyFile);
}
