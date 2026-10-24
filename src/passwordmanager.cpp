#include "passwordmanager.h"
#include <QCborMap>
#include <QCborValue>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <chrono>
#include <algorithm>
#include "settings.h"
#include <QStandardPaths>
#include <QUuid>

namespace {
QString quoted(const QString &value)
{
    const auto json = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}
QByteArray record(const QString &origin, const QString &username, const QString &password)
{
    QCborMap map;
    map.insert(QStringLiteral("version"), 1);
    map.insert(QStringLiteral("origin"), origin);
    map.insert(QStringLiteral("username"), username);
    map.insert(QStringLiteral("password"), password);
    return QCborValue(map).toCbor();
}
}

PasswordManager::PasswordManager(QObject *parent)
    : PasswordManager(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                          + QStringLiteral("/passwords/Default"), QStringLiteral("Default"), nullptr, parent)
{
    m_enabled = Settings().value(QStringLiteral("Passwords/enabled"), true).toBool();
}

PasswordManager::PasswordManager(const QString &directory, const QString &profile,
                                 SecretStore *store, QObject *parent, VerificationClock clock)
    : QObject(parent), m_verificationClock(clock ? std::move(clock) : VerificationClock([] {
          return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
      })), m_directory(directory), m_profile(profile), m_store(store)
{
    bool valid=false;
    const int cooldown=Settings().value(QStringLiteral("Passwords/verificationCooldown"),300).toInt(&valid);
    m_verificationCooldown=valid ? std::clamp(cooldown,0,3600) : 300;
    m_biometricEnabled=Settings().value(QStringLiteral("Passwords/biometricEnabled"),true).toBool();
    if (!m_store) m_store = new KeychainSecretStore(this);
    m_pendingTimer.setSingleShot(true);
    m_pendingTimer.setInterval(120000);
    connect(&m_pendingTimer, &QTimer::timeout, this, &PasswordManager::dismissPending);
    initialize();
}

QString PasswordManager::originFor(const QUrl &url)
{
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()
        || !url.userInfo().isEmpty()) return {};
    QUrl origin;
    origin.setScheme(QStringLiteral("https"));
    origin.setHost(url.host().toLower());
    if (url.port(-1) != 443) origin.setPort(url.port(-1));
    return origin.toString(QUrl::FullyEncoded);
}

void PasswordManager::initialize()
{
    if (!QDir().mkpath(m_directory)) {
        m_writable = false;
        m_error = tr("Could not create the password index directory.");
        return;
    }
    QFile::setPermissions(m_directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    m_lock = std::make_unique<QLockFile>(m_directory + QStringLiteral("/index.lock"));
    if (!m_lock->tryLock(0)) {
        m_writable = false;
        m_error = tr("Password storage is in use by another Arbiter process. Close it and restart Arbiter.");
        return;
    }
    QFile file(m_directory + QStringLiteral("/index.json"));
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) {
        m_writable = false;
        m_error = tr("Could not read the password index. The existing data has been preserved.");
        return;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto root = doc.object();
    bool valid = parseError.error == QJsonParseError::NoError && root.value("version").toInt() == 1
                 && root.value("entries").isArray();
    QSet<QString> ids, accounts;
    QVariantList entries;
    for (const auto &value : root.value("entries").toArray()) {
        const auto entry = value.toObject();
        const QString id = entry.value("id").toString();
        const QString origin = entry.value("origin").toString();
        const QString username = entry.value("username").toString();
        const QString account = origin + QChar(0) + username;
        valid &= !QUuid(id).isNull() && id == QUuid(id).toString(QUuid::WithoutBraces)
                 && originFor(QUrl(origin)) == origin && !origin.isEmpty()
                 && username.size() <= 512 && !ids.contains(id) && !accounts.contains(account);
        ids.insert(id);
        accounts.insert(account);
        entries.append(QVariantMap{{"id", id}, {"origin", origin}, {"username", username}});
    }
    if (!valid) {
        m_writable = false;
        m_error = tr("The password index is invalid or uses an unsupported version. It has not been overwritten.");
        return;
    }
    m_entries = entries;
}

bool PasswordManager::persist(const QVariantList &entries)
{
    QSaveFile file(m_directory + QStringLiteral("/index.json"));
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"entries", QJsonArray::fromVariantList(entries)}})
                          .toJson(QJsonDocument::Compact);
    if (data.size() > 4 * 1024 * 1024) return false;
    return file.write(data) == data.size() && file.commit();
}

QString PasswordManager::secretKey(const QString &id) const
{
    const auto scope = QCryptographicHash::hash((QDir(m_directory).absolutePath() + QChar(0) + m_profile).toUtf8(),
                                               QCryptographicHash::Sha256).toHex();
    return QStringLiteral("Arbiter/Passwords/v1/") + QString::fromLatin1(scope) + QLatin1Char('/') + id;
}

void PasswordManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    clearUserVerification();
    Settings().setValue(QStringLiteral("Passwords/enabled"), enabled);
    if (!enabled) dismissPending();
    emit stateChanged();
}

void PasswordManager::finish(const QString &error, const QString &message)
{
    m_busy = false;
    m_error = error;
    m_message = error.isEmpty() ? message : QString();
    emit stateChanged();
}

void PasswordManager::clearMessage()
{
    if (m_writable) m_error.clear();
    m_message.clear();
    emit stateChanged();
}

void PasswordManager::reportFillResult(bool filled)
{
    m_error = filled ? QString() : tr("The login form changed or cannot be filled safely. Open Passwords again.");
    m_message = filled ? tr("Login filled. Review it before signing in.") : QString();
    emit stateChanged();
}
void PasswordManager::reportSuggestionResult(bool filled)
{
    m_error=filled ? QString() : tr("The password field changed or cannot be filled safely. Right-click the field and try again.");
    m_message=filled ? tr("Suggested password inserted. Arbiter will offer to save it when you submit.") : QString();
    emit stateChanged();
}

int PasswordManager::find(const QString &origin, const QString &username) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        const auto entry = m_entries.at(i).toMap();
        if (entry.value("origin") == origin && entry.value("username") == username) return i;
    }
    return -1;
}

QVariantList PasswordManager::accountsFor(const QUrl &url) const
{
    QVariantList result;
    const auto origin = originFor(url);
    if (!m_enabled || origin.isEmpty()) return result;
    for (const auto &entry : m_entries)
        if (entry.toMap().value("origin") == origin) result.append(entry);
    return result;
}

bool PasswordManager::canStore(const QUrl &url,const QString &username,const QString &password) {
    const auto origin=originFor(url);
    return !origin.isEmpty() && !password.isEmpty() && username.size()<=512 && record(origin,username,password).size()<=2560;
}
void PasswordManager::importAccount(const QUrl &url,const QString &username,const QString &password,ImportCallback callback) {
    if(m_busy || !m_writable) { callback(false,m_error.isEmpty() ? tr("Password storage is busy or unavailable.") : m_error); return; }
    if(find(originFor(url),username)>=0) { callback(false,{}); return; }
    saveLogin(url,username,password,std::move(callback));
}
void PasswordManager::save(const QUrl &url, const QString &username, const QString &password) {
    if(m_busy || !m_writable) return;
    saveLogin(url,username,password,{});
}
void PasswordManager::saveLogin(const QUrl &url,const QString &username,const QString &password,ImportCallback callback)
{
    const auto origin = originFor(url);
    if ((!callback && !m_enabled) || origin.isEmpty() || password.isEmpty() || username.size() > 512) {
        finish(tr("Enter an HTTPS website and a non-empty password (username: at most 512 characters)."));
        if(callback) callback(false,m_error);
        return;
    }
    const auto data = record(origin, username, password);
    // Windows Credential Manager has a 2560-byte blob limit. Enforce the same
    // limit everywhere, including the account-binding envelope.
    if (data.size() > 2560) {
        finish(tr("This login is too large for cross-platform secure storage (maximum 2560 encoded bytes)."));
        if(callback) callback(false,m_error);
        return;
    }
    const int existing = find(origin, username);
    const QString id = existing < 0 ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                    : m_entries.at(existing).toMap().value("id").toString();
    auto next = m_entries;
    if (existing < 0) {
        next.append(QVariantMap{{"id", id}, {"origin", origin}, {"username", username}});
        // Reserve the index before writing the secret. A crash can leave a
        // missing secret that can be re-saved, never an undiscoverable password.
        if (!persist(next)) {
            finish(tr("Could not update the password index. No password was written."));
            if(callback) callback(false,m_error);
            return;
        }
    }
    m_busy = true;
    m_error.clear();
    m_message.clear();
    emit stateChanged();
    const QPointer<PasswordManager> guard(this);
    m_store->write(secretKey(id), data, [guard, next, existing, callback=std::move(callback)](const QString &error, const QByteArray &) {
        if (!guard) return;
        if (!error.isEmpty()) {
            if (existing < 0 && !guard->persist(guard->m_entries)) {
                // Keep the reserved entry visible if rollback cannot be saved.
                guard->m_entries = next;
                emit guard->entriesChanged();
            }
            guard->finish(error);
            if(callback) callback(false,error);
            return;
        }
        guard->m_entries = next;
        emit guard->entriesChanged();
        guard->finish({}, callback ? QString() : existing < 0 ? tr("Password saved securely.") : tr("Password updated securely."));
        if(callback) callback(true,{});
        else { emit guard->credentialSaved(); guard->dismissPending(); }
    });
}

void PasswordManager::remove(const QString &id)
{
    if (m_busy || !m_writable) return;
    int index = -1;
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries.at(i).toMap().value("id") == id) index = i;
    if (index < 0) return;
    m_busy = true;
    m_error.clear();
    m_message.clear();
    emit stateChanged();
    const QPointer<PasswordManager> guard(this);
    m_store->remove(secretKey(id), [guard, index](const QString &error, const QByteArray &) {
        if (!guard) return;
        if (!error.isEmpty()) { guard->finish(error); return; }
        auto next = guard->m_entries;
        next.removeAt(index);
        if (!guard->persist(next)) {
            guard->finish(tr("The secret was deleted, but its index entry could not be removed. Retry deletion."));
            return;
        }
        guard->m_entries = next;
        emit guard->entriesChanged();
        guard->finish({}, tr("Password deleted."));
    });
}

void PasswordManager::setUserVerification(const QString &name,bool available,VerifyUser verify) {
    if(m_biometricName!=name || m_biometricAvailable!=available) clearUserVerification();
    m_biometricName=name; m_biometricAvailable=available; m_verifyUser=std::move(verify); emit stateChanged();
}
void PasswordManager::clearUserVerification() {
    m_verifiedAt=-1;
    ++m_verificationEpoch;
}
void PasswordManager::setBiometricEnabled(bool enabled) {
    if(m_biometricEnabled==enabled) return;
    m_biometricEnabled=enabled;
    clearUserVerification();
    Settings().setValue(QStringLiteral("Passwords/biometricEnabled"),enabled);
    emit stateChanged();
}
void PasswordManager::setVerificationCooldown(int seconds) {
    seconds=std::clamp(seconds,0,3600);
    if(m_verificationCooldown==seconds) return;
    m_verificationCooldown=seconds;
    clearUserVerification();
    Settings().setValue(QStringLiteral("Passwords/verificationCooldown"),seconds);
    emit stateChanged();
}
void PasswordManager::readForFill(const QString &id, const QString &origin, SecretStore::Callback callback,bool biometric,std::function<bool()> current)
{
    if (m_busy || !m_writable || !m_enabled || origin.isEmpty()) {
        callback(tr("Password storage is unavailable or busy."), {});
        return;
    }
    QVariantMap match;
    for (const auto &value : m_entries) {
        const auto entry = value.toMap();
        if (entry.value("id") == id && entry.value("origin") == origin) match = entry;
    }
    if (match.isEmpty()) { callback(tr("This account does not belong to the current website."), {}); return; }
    m_busy = true;
    m_error.clear();
    m_message.clear();
    emit stateChanged();
    const QPointer<PasswordManager> guard(this);
    const auto epoch=m_verificationEpoch;
    const auto read=[guard,match,id,callback,current,epoch] {
        if(!guard) return;
        if(epoch!=guard->m_verificationEpoch || (current && !current())) { guard->finish({}); callback(tr("Password request cancelled."),{}); return; }
        if(!guard->m_enabled) { const auto error=tr("Password filling is disabled."); guard->finish(error); callback(error,{}); return; }
        const auto completed=[guard,match,callback,current,epoch](const QString &error,const QByteArray &data) {
            if(!guard) return;
            QString resultError=error;
            const auto map=QCborValue::fromCbor(data).toMap();
            if(resultError.isEmpty() && (map.value(QStringLiteral("version")).toInteger()!=1
                || map.value(QStringLiteral("origin")).toString()!=match.value("origin").toString()
                || map.value(QStringLiteral("username")).toString()!=match.value("username").toString()
                || !map.value(QStringLiteral("password")).isString() || map.value(QStringLiteral("password")).toString().isEmpty()))
                resultError=tr("The stored password does not match its index entry. It was not filled.");
            if(!guard->m_enabled) resultError=tr("Password filling is disabled.");
            if(epoch!=guard->m_verificationEpoch || (current && !current())) resultError=tr("Password request cancelled.");
            guard->finish(resultError);
            callback(resultError,resultError.isEmpty() ? data : QByteArray());
        };
        guard->m_store->read(guard->secretKey(id),completed);
    };
    if(!biometric) { read(); return; }
    if(!m_biometricAvailable || !m_verifyUser) {
        const auto error=tr("Fingerprint verification is unavailable. Check verification preferences in Settings → Passwords.");
        finish(error); callback(error,{}); return;
    }
    const qint64 elapsed=m_verifiedAt<0 ? -1 : m_verificationClock()-m_verifiedAt;
    if(m_verificationCooldown>0 && elapsed>=0 && elapsed<qint64(m_verificationCooldown)*1000) { read(); return; }
    m_verifiedAt=-1;
    m_verifyUser(tr("Fill your saved password for %1").arg(origin),[guard,read,callback,current,epoch](bool approved,const QString &) {
        if(!guard) return;
        if(!approved) { const auto message=tr("%1 verification was cancelled or unsuccessful. Try again, or review verification preferences in Settings → Passwords.").arg(guard->m_biometricName); guard->finish(message); callback(message,{}); return; }
        if(epoch==guard->m_verificationEpoch && guard->m_enabled && (!current || current()) && guard->m_verificationCooldown>0)
            guard->m_verifiedAt=guard->m_verificationClock();
        read();
    });
}

bool PasswordManager::pendingUpdate() const { return find(m_pendingOrigin, m_pendingUsername) >= 0; }

void PasswordManager::offer(const QUrl &committedUrl, const QString &reportedOrigin,
                            const QString &username, const QString &password, bool privateBrowsing)
{
    const auto origin = originFor(committedUrl);
    if (!m_enabled || !m_writable || privateBrowsing || m_busy || pending()
        || origin.isEmpty() || origin != reportedOrigin || password.isEmpty()
        || username.size() > 512 || password.size() > 2560) return;
    const QPointer<PasswordManager> guard(this);
    const auto stage = [guard, origin, username, password] {
        if (!guard || !guard->m_enabled || guard->pending() || guard->m_busy) return;
        guard->m_pendingOrigin = origin;
        guard->m_pendingUsername = username;
        guard->m_pendingPassword = password;
        guard->m_pendingTimer.start();
        guard->clearMessage();
        emit guard->pendingChanged();
    };
    const int existing = find(origin, username);
    if (existing < 0) { stage(); return; }
    // Compare only this submitted account, never fetch the whole vault. The
    // stored value stays native and is never returned through the capture bridge.
    readForFill(m_entries.at(existing).toMap().value("id").toString(), origin,
        [stage, password](const QString &error, const QByteArray &data) {
            if (error.isEmpty()
                && QCborValue::fromCbor(data).toMap().value(QStringLiteral("password")).toString() == password) return;
            stage();
        });
}

void PasswordManager::acceptPending(const QString &username)
{
    if (pending()) save(QUrl(m_pendingOrigin), username, m_pendingPassword);
}

void PasswordManager::dismissPending()
{
    m_pendingTimer.stop();
    m_pendingOrigin.clear();
    m_pendingUsername.clear();
    m_pendingPassword.fill(QChar(0));
    m_pendingPassword.clear();
    emit pendingChanged();
}

QString PasswordManager::generatePassword() const
{
    const QString alphabet = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%*-_=+");
    QString password;
    // Rejection sampling via bounded() avoids modulo bias. Require all four
    // character classes for common website password policies.
    do {
        password.clear();
        for (int i = 0; i < 24; ++i)
            password += alphabet.at(QRandomGenerator::system()->bounded(quint32(alphabet.size())));
    } while (!password.contains(QRegularExpression(QStringLiteral("[A-Z]")))
             || !password.contains(QRegularExpression(QStringLiteral("[a-z]")))
             || !password.contains(QRegularExpression(QStringLiteral("[0-9]")))
             || !password.contains(QRegularExpression(QStringLiteral("[!@#$%*_=+\\-]"))));
    return password;
}

QString PasswordManager::captureScript() const
{
    QFile channel(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
    QFile script(QStringLiteral(":/passwords/passwords.js"));
    if (!channel.open(QIODevice::ReadOnly) || !script.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(channel.readAll()) + QLatin1Char('\n') + QString::fromUtf8(script.readAll());
}

QString PasswordManager::fillScript(const QString &origin, const QString &document,
                                    const QString &form, const QString &username, const QString &password)
{
    return QStringLiteral("globalThis.__arbiterPasswords?.fill(%1,%2,%3,%4,%5) === true")
        .arg(quoted(origin), quoted(document), quoted(form), quoted(username), quoted(password));
}
QString PasswordManager::suggestionScript(const QString &origin, const QString &document,
                                          const QString &form, const QString &field, const QString &password)
{
    return QStringLiteral("globalThis.__arbiterPasswords?.suggestPassword(%1,%2,%3,%4,%5) === true")
        .arg(quoted(origin),quoted(document),quoted(form),quoted(field),quoted(password));
}

void PasswordCapture::submit(const QString &origin, const QString &username, const QString &password)
{
    if (receiver && origin.size() <= 2048 && username.size() <= 512 && password.size() <= 2560)
        receiver(origin, username, password);
}

PasswordSession::PasswordSession(QObject *parent) : QObject(parent), m_capture(this)
{
    m_capture.focusReceiver=[this](const QVariantMap &document) { m_focusDocument=document; emit focusNeedsValidation(); m_focusDocument.clear(); };
    m_capture.receiver = [this](const QString &origin, const QString &username, const QString &password) {
        m_captureOrigin = origin;
        m_captureUsername = username;
        m_capturePassword = password;
        emit captureNeedsValidation();
        m_captureOrigin.clear();
        m_captureUsername.clear();
        m_capturePassword.fill(QChar(0));
        m_capturePassword.clear();
    };
}

void PasswordCapture::focus(const QVariantMap &document) {
    if(focusReceiver && document.size()<=8 && document.value("origin").toString().size()<=2048
        && document.value("document").toString().size()==36 && document.value("form").toString().size()<=16) focusReceiver(document);
}
void PasswordSession::validateFocus(const QUrl &committedUrl) {
    if(m_focusDocument.isEmpty()) return;
    prepare(committedUrl,m_focusDocument);
    if(m_origin.isEmpty()) return;
    const QRectF rect(m_focusDocument.value("x").toDouble(),m_focusDocument.value("y").toDouble(),
        m_focusDocument.value("width").toDouble(),m_focusDocument.value("height").toDouble());
    if(!std::isfinite(rect.x()) || !std::isfinite(rect.y()) || !std::isfinite(rect.width()) || !std::isfinite(rect.height())
        || rect.width()<=0 || rect.height()<=0 || rect.width()>10000 || rect.height()>10000) { invalidate(); return; }
    emit focused(m_origin,rect);
}
void PasswordSession::validateCapture(const QUrl &committedUrl, bool privateBrowsing)
{
    if (m_manager)
        m_manager->offer(committedUrl, m_captureOrigin, m_captureUsername, m_capturePassword, privateBrowsing);
}

void PasswordSession::invalidate()
{
    ++m_generation;
    m_origin.clear();
    m_document.clear();
    m_form.clear();
}

void PasswordSession::prepare(const QUrl &committedUrl, const QVariantMap &document)
{
    invalidate();
    const auto origin = PasswordManager::originFor(committedUrl);
    if (!m_manager || !m_manager->enabled() || origin.isEmpty()
        || document.value("origin").toString() != origin
        || document.value("document").toString().size() != 36
        || document.value("form").toString().isEmpty()) {
        emit prepared({});
        return;
    }
    m_origin = origin;
    m_document = document.value("document").toString();
    m_form = document.value("form").toString();
    emit prepared(origin);
}

void PasswordSession::fill(const QString &id, const QUrl &committedUrl)
{
    if (!m_manager || m_document.isEmpty() || m_origin != PasswordManager::originFor(committedUrl)) return;
    const auto generation = ++m_generation;
    const QPointer<PasswordSession> guard(this);
    m_manager->readForFill(id, m_origin, [guard, generation](const QString &error, const QByteArray &data) {
        if (!guard || guard->m_generation != generation || !error.isEmpty()) return;
        const auto map = QCborValue::fromCbor(data).toMap();
        emit guard->runFillScript(PasswordManager::fillScript(guard->m_origin, guard->m_document, guard->m_form,
            map.value(QStringLiteral("username")).toString(), map.value(QStringLiteral("password")).toString()));
    },m_manager->biometricEnabled() && m_manager->biometricAvailable(),[guard,generation] { return guard && guard->m_generation==generation; });
}
void PasswordSession::suggestPassword(const QUrl &committedUrl, const QVariantMap &field)
{
    if(!m_manager || !m_manager->enabled() || m_manager->busy() || field.size()!=4
        || field.value("form").toString().size()>16 || field.value("field").toString().isEmpty()
        || field.value("field").toString().size()>16 || QUuid(field.value("document").toString()).isNull()) return;
    prepare(committedUrl,field);
    if(m_origin.isEmpty()) return;
    emit runPasswordSuggestionScript(PasswordManager::suggestionScript(m_origin,m_document,m_form,
        field.value("field").toString(),m_manager->generatePassword()));
}
