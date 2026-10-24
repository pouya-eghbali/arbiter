#include "chromevault.h"
#include "passwordmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStringDecoder>
#include <QUuid>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <memory>
#ifdef Q_OS_MACOS
#include <Security/Security.h>
#elif defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#else
#include <QDBusInterface>
#include <QDBusReply>
#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")
#endif

namespace {
void wipe(QByteArray &data) { if(!data.isEmpty()) OPENSSL_cleanse(data.data(),size_t(data.size())); data.clear(); }
struct Row { QUrl url; QString username; QByteArray encrypted; };
QList<Row> readRows(const QVariantMap &source,QString &error) {
    QList<Row> rows;
    const QString profile=source.value("directory").toString();
    bool found=false;
    for(const auto &file:{QStringLiteral("Login Data"),QStringLiteral("Login Data For Account")}) {
        const auto path=QDir(profile).filePath(file); if(!QFileInfo::exists(path)) continue;
        found=true; const auto connection=QUuid::createUuid().toString();
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",connection);
            db.setDatabaseName(path); db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1500");
            if(!db.open()) error=QStringLiteral("Could not open the selected browser's password vault. Close that browser and try again.");
            else {
                QSqlQuery query(db);
                if(!query.exec("SELECT origin_url,username_value,password_value FROM logins WHERE blacklisted_by_user=0 LIMIT 10001")) error=QStringLiteral("This browser password vault uses an unsupported format or is locked. Close the source browser and try again.");
                else while(query.next()) {
                    const auto bytes=query.value(2).toByteArray(); if(bytes.isEmpty()) continue;
                    if(bytes.size()>1024*1024 || rows.size()>=10000) { error=QStringLiteral("The selected vault is too large (maximum 10,000 passwords)."); break; }
                    if(bytes.startsWith("v20")) { error=QStringLiteral("This password vault only allows its source browser to export passwords. Export passwords after authenticating in that browser, then use Import from files."); break; }
                    rows.append({QUrl(query.value(0).toString()),query.value(1).toString(),bytes});
                }
                if(query.lastError().isValid() && error.isEmpty()) error=QStringLiteral("Could not read all passwords from the selected vault.");
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connection);
        if(!error.isEmpty()) return {};
    }
    if(!found) error=QStringLiteral("No local password vault was found for this profile. Passwords available only in the source browser's cloud account must be synced there first.");
    return rows;
}
#ifdef Q_OS_MACOS
QByteArray nativeKey(const QVariantMap &source,QString &error) {
    const auto service=source.value("service").toString().toUtf8(); const auto account=source.value("account").toString().toUtf8();
    auto name=CFStringCreateWithBytes(nullptr,reinterpret_cast<const UInt8 *>(service.constData()),service.size(),kCFStringEncodingUTF8,false);
    auto user=CFStringCreateWithBytes(nullptr,reinterpret_cast<const UInt8 *>(account.constData()),account.size(),kCFStringEncodingUTF8,false);
    auto query=CFDictionaryCreateMutable(nullptr,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query,kSecClass,kSecClassGenericPassword); CFDictionarySetValue(query,kSecAttrService,name);
    CFDictionarySetValue(query,kSecAttrAccount,user); CFDictionarySetValue(query,kSecReturnData,kCFBooleanTrue);
    CFDictionarySetValue(query,kSecMatchLimit,kSecMatchLimitOne);
    CFTypeRef result=nullptr; const auto status=SecItemCopyMatching(query,&result);
    CFRelease(query); CFRelease(name); CFRelease(user);
    if(status!=errSecSuccess || !result || CFGetTypeID(result)!=CFDataGetTypeID()) {
        if(result) CFRelease(result);
        error=status==errSecUserCanceled || status==errSecAuthFailed
            ? QStringLiteral("Keychain access was cancelled or denied. Allow Arbiter to read the selected browser's Safe Storage item to import its passwords.")
            : QStringLiteral("The selected browser's Safe Storage key is unavailable. Unlock the macOS login Keychain, then try again.");
        return {};
    }
    const auto data=static_cast<CFDataRef>(result); QByteArray password(reinterpret_cast<const char *>(CFDataGetBytePtr(data)),CFDataGetLength(data));
    CFRelease(result); auto key=ChromeVault::deriveCbcKey(password,1003); wipe(password); return key;
}
#elif defined(Q_OS_WIN)
QByteArray unprotect(const QByteArray &bytes) {
    DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE *>(const_cast<char *>(bytes.constData()))},output{};
    if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,0,&output)) return {};
    QByteArray data(reinterpret_cast<char *>(output.pbData),output.cbData); SecureZeroMemory(output.pbData,output.cbData); LocalFree(output.pbData); return data;
}
QByteArray nativeKey(const QVariantMap &source,QString &error) {
    QFile file(source.value("localState").toString());
    if(!file.open(QIODevice::ReadOnly) || file.size()>16*1024*1024) { error=QStringLiteral("Could not read this browser's Local State encryption metadata."); return {}; }
    const auto root=QJsonDocument::fromJson(file.readAll()).object();
    auto protectedKey=QByteArray::fromBase64(root.value("os_crypt").toObject().value("encrypted_key").toString().toLatin1());
    if(!protectedKey.startsWith("DPAPI")) { error=QStringLiteral("This browser's protected key format requires its authenticated password export."); return {}; }
    auto key=unprotect(protectedKey.mid(5)); wipe(protectedKey);
    if(key.size()!=32) { wipe(key); error=QStringLiteral("Windows could not unlock this vault for the current user. Sign in with the account that owns the source browser profile."); }
    return key;
}
#else
QByteArray walletKey(const QVariantMap &source,QString &error) {
    for(const auto &version:{QStringLiteral("6"),QStringLiteral("5"),QString()}) {
        QDBusInterface wallet("org.kde.kwalletd"+version,"/modules/kwalletd"+version,"org.kde.KWallet");
        wallet.setTimeout(120000);
        QDBusReply<QString> name=wallet.call("networkWallet"); if(!name.isValid()) continue;
        QDBusReply<int> handle=wallet.call("open",name.value(),qlonglong(0),QStringLiteral("Arbiter"));
        if(!handle.isValid() || handle.value()<0) { error=QStringLiteral("KWallet access was cancelled or denied. Unlock the wallet and authorize Arbiter to read this browser key."); return {}; }
        const auto folder=source.value("account").toString()+" Keys",item=source.value("service").toString();
        QDBusReply<bool> has=wallet.call("hasEntry",handle.value(),folder,item,QStringLiteral("Arbiter"));
        QDBusReply<QString> password;
        if(has.isValid() && has.value()) password=wallet.call("readPassword",handle.value(),folder,item,QStringLiteral("Arbiter"));
        wallet.call("close",handle.value(),false,QStringLiteral("Arbiter"));
        if(password.isValid()) return password.value().toUtf8();
    }
    return {};
}
QByteArray nativeKey(const QVariantMap &source,QString &error) {
    const bool kde=qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE",Qt::CaseInsensitive);
    QByteArray password;
    if(kde) password=walletKey(source,error);
    if(!error.isEmpty()) return {};
    if(password.isEmpty()) {
        // Matches Chromium's read schema; never creates or replaces keyring items.
        const SecretSchema schema={"chrome_libsecret_os_crypt_password_v2",SECRET_SCHEMA_DONT_MATCH_NAME,{{"application",SECRET_SCHEMA_ATTRIBUTE_STRING},{nullptr,SECRET_SCHEMA_ATTRIBUTE_STRING}}};
        GError *failure=nullptr;
        const auto app=source.value("linuxApplication").toString().toUtf8();
        gchar *value=secret_password_lookup_sync(&schema,nullptr,&failure,"application",app.constData(),nullptr);
        if(value) { password=QByteArray(value); secret_password_free(value); }
        if(failure) {
            const bool denied=g_error_matches(failure,G_IO_ERROR,G_IO_ERROR_CANCELLED)
                || g_error_matches(failure,G_DBUS_ERROR,G_DBUS_ERROR_ACCESS_DENIED)
                || g_error_matches(failure,SECRET_ERROR,SECRET_ERROR_IS_LOCKED);
            g_error_free(failure);
            if(denied) { error=QStringLiteral("System keyring access was cancelled or denied. Unlock it or approve the system prompt, then try again."); return {}; }
        }
        if(password.isEmpty() && !kde) password=walletKey(source,error);
        if(!error.isEmpty()) return {};
    }
    if(password.isEmpty()) { error=QStringLiteral("No Safe Storage key was available for this browser. Unlock its Secret Service keyring or KWallet, then try again."); return {}; }
    auto key=ChromeVault::deriveCbcKey(password,1); wipe(password); return key;
}
#endif
ImportPreview decode(const QList<Row> &rows,const QByteArray &key,ChromeVault::Cipher cipher) {
    ImportPreview result; QSet<QString> seen;
    for(const auto &row:rows) {
        auto plain=ChromeVault::decrypt(row.encrypted,key,cipher);
        if(!plain) { result.passwords.clear(); result.error=QStringLiteral("The selected browser vault could not be decrypted with its authorized OS key. Its encryption version may not support direct import."); return result; }
        QStringDecoder decoder(QStringDecoder::Utf8); const QString password=decoder.decode(*plain); wipe(*plain);
        if(decoder.hasError()) { result.passwords.clear(); result.error=QStringLiteral("The browser vault did not unlock correctly. No passwords were imported."); return result; }
        const auto origin=PasswordManager::originFor(row.url); const auto account=origin+QChar(0)+row.username;
        if(!PasswordManager::canStore(row.url,row.username,password) || seen.contains(account)) { ++result.skippedPasswords; continue; }
        seen.insert(account); result.passwords.append({QUrl(origin),row.username,password});
    }
    return result;
}
}
QByteArray ChromeVault::deriveCbcKey(const QByteArray &password,int iterations) {
    QByteArray key(16,Qt::Uninitialized);
    if(!PKCS5_PBKDF2_HMAC_SHA1(password.constData(),password.size(),reinterpret_cast<const unsigned char *>("saltysalt"),9,iterations,16,reinterpret_cast<unsigned char *>(key.data()))) { wipe(key); return {}; }
    return key;
}
std::optional<QByteArray> ChromeVault::decrypt(const QByteArray &data,const QByteArray &key,Cipher cipher) {
    if(data.startsWith("v20")) return std::nullopt;
#ifdef Q_OS_WIN
    if(cipher==Cipher::WindowsGcm && !data.startsWith("v10")) {
        auto plain=unprotect(data); if(plain.isEmpty()) return std::nullopt; return plain;
    }
#endif
    auto actual=key;
    struct Wiper { QByteArray &bytes; ~Wiper() { wipe(bytes); } } cleanup{actual};
    if(cipher==Cipher::LinuxCbc && data.startsWith("v10")) actual=deriveCbcKey("peanuts",1);
    const bool gcm=cipher==Cipher::WindowsGcm;
    if((gcm && (actual.size()!=32 || !data.startsWith("v10") || data.size()<31))
       || (!gcm && (actual.size()!=16 || (!data.startsWith("v10") && !(cipher==Cipher::LinuxCbc && data.startsWith("v11"))) || (data.size()-3)%16!=0 || data.size()<19))) return std::nullopt;
    std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);
    if(!ctx) return std::nullopt;
    const auto encrypted=gcm ? data.mid(15,data.size()-31) : data.mid(3);
    const auto iv=gcm ? data.mid(3,12) : QByteArray(16,' ');
    QByteArray plain(encrypted.size()+16,Qt::Uninitialized); int count=0,tail=0;
    bool ok=EVP_DecryptInit_ex(ctx.get(),gcm ? EVP_aes_256_gcm() : EVP_aes_128_cbc(),nullptr,reinterpret_cast<const unsigned char *>(actual.constData()),reinterpret_cast<const unsigned char *>(iv.constData()))==1
        && EVP_DecryptUpdate(ctx.get(),reinterpret_cast<unsigned char *>(plain.data()),&count,reinterpret_cast<const unsigned char *>(encrypted.constData()),encrypted.size())==1;
    if(ok && gcm) ok=EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_SET_TAG,16,const_cast<char *>(data.constData()+data.size()-16))==1;
    if(ok) ok=EVP_DecryptFinal_ex(ctx.get(),reinterpret_cast<unsigned char *>(plain.data()+count),&tail)==1;
    if(!ok) { wipe(plain); return std::nullopt; }
    plain.resize(count+tail); return plain;
}
ImportPreview ChromeVault::readWithKey(const QVariantMap &source,const QByteArray &key,Cipher cipher) {
    QString error; const auto rows=readRows(source,error); if(!error.isEmpty()) { ImportPreview out; out.error=error; return out; }
    return decode(rows,key,cipher);
}
ImportPreview ChromeVault::read(const QVariantMap &source) {
    QString error; const auto rows=readRows(source,error);
    if(!error.isEmpty()) { ImportPreview out; out.error=error; return out; }
    if(rows.isEmpty()) return {};
#ifdef Q_OS_MACOS
    constexpr auto cipher=Cipher::MacCbc;
#elif defined(Q_OS_WIN)
    constexpr auto cipher=Cipher::WindowsGcm;
#else
    constexpr auto cipher=Cipher::LinuxCbc;
#endif
    bool needsKey=false;
    for(const auto &row:rows) {
#ifdef Q_OS_MACOS
        needsKey=true;
#elif defined(Q_OS_WIN)
        needsKey|=row.encrypted.startsWith("v10");
#else
        needsKey|=row.encrypted.startsWith("v11");
#endif
    }
    QByteArray key;
    if(needsKey) key=nativeKey(source,error);
    if(!error.isEmpty()) { ImportPreview out; out.error=error; return out; }
    auto out=decode(rows,key,cipher); wipe(key); return out;
}
