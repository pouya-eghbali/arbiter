#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>

// Versioned, multi-value credential registers. Conflicts require an explicit
// user choice; wall-clock time is never used to discard a password or passkey.
namespace ArbiterSync {
constexpr qsizetype MaximumBytes = 16 * 1024 * 1024;
QByteArray seal(const QByteArray &plain, const QByteArray &key, const QByteArray &context,
                const QByteArray &nonce = {});
QByteArray open(const QByteArray &sealed, const QByteArray &key, const QByteArray &context, bool *ok);
bool dominates(const QJsonObject &a, const QJsonObject &b);
QString passwordId(const QString &origin, const QString &username);
QString passkeyId(const QString &rpId, const QByteArray &credentialId);
class State {
public:
    static bool parse(const QJsonObject &json, State *out, QString *error);
    QJsonObject json() const;
    QJsonObject records() const { return m_records; }
    bool merge(const State &other, QString *error);
    bool update(const QString &id, const QString &device, const QJsonObject &data, bool deleted, QString *error);
    int conflicts() const;
private:
    QJsonObject m_records;
};
}
