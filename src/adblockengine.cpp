#include "adblockengine.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
extern "C" {
void *arbiter_adblock_new(const char *);
void arbiter_adblock_free(void *);
bool arbiter_adblock_check(const void *, const char *, const char *, const char *, const char *);
char *arbiter_adblock_cosmetic(const void *, const char *, const char *, const char *);
void arbiter_adblock_string_free(char *);
}
AdBlockEngine::AdBlockEngine(const QByteArray &rules) : m_engine(arbiter_adblock_new(rules.constData())) {}
AdBlockEngine::~AdBlockEngine() {
    arbiter_adblock_free(m_engine);
}
bool AdBlockEngine::blocks(const QUrl &url, const QUrl &source, const QByteArray &type,
                           const QByteArray &method) const {
    if (!m_engine)
        return false;
    QMutexLocker lock(&m_mutex);
    return arbiter_adblock_check(m_engine, url.toEncoded().constData(), source.toEncoded().constData(),
                                 type.constData(), method.constData());
}
QStringList AdBlockEngine::cosmetic(const QUrl &url, const QStringList &classes,
                                    const QStringList &ids) const {
    if (!m_engine)
        return {};
    QMutexLocker lock(&m_mutex);
    char *result = arbiter_adblock_cosmetic(
        m_engine, url.toEncoded().constData(),
        QJsonDocument(QJsonArray::fromStringList(classes)).toJson(QJsonDocument::Compact).constData(),
        QJsonDocument(QJsonArray::fromStringList(ids)).toJson(QJsonDocument::Compact).constData());
    if (!result)
        return {};
    const auto entries = QJsonDocument::fromJson(QByteArray(result)).array();
    arbiter_adblock_string_free(result);
    QStringList selectors;
    for (const auto &entry : entries)
        selectors.append(entry.toString());
    return selectors;
}
