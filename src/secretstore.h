#pragma once
#include <QByteArray>
#include <QObject>
#include <functional>

// No renderer or QML object gets access to this interface.
class SecretStore : public QObject
{
public:
    using Callback = std::function<void(const QString &error, const QByteArray &data)>;
    using QObject::QObject;
    virtual void read(const QString &key, Callback callback) = 0;
    virtual void write(const QString &key, const QByteArray &data, Callback callback) = 0;
    virtual void remove(const QString &key, Callback callback) = 0;
};

class KeychainSecretStore final : public SecretStore
{
public:
    explicit KeychainSecretStore(QObject *parent = nullptr,
        const QString &service = QStringLiteral("Arbiter Passwords")) : SecretStore(parent), m_service(service) {}
    void read(const QString &key, Callback callback) override;
    void write(const QString &key, const QByteArray &data, Callback callback) override;
    void remove(const QString &key, Callback callback) override;
private:
    QString m_service;
};
