#include "passkeycontroller.h"
#include "qrcodegen.hpp"
#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QStandardPaths>
#include <QCborArray>
#include <QCborValue>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

#ifdef ARBITER_PATCHED_WEBENGINE
#if !defined(QT_ARBITER_WEBAUTHN_ABI) || QT_ARBITER_WEBAUTHN_ABI != 1
#error "This build requires Arbiter's ABI 1 patched Qt WebEngine SDK."
#endif
#endif

PasskeyController::PasskeyController(QObject *parent)
    : QObject(parent), m_store(this, QStringLiteral("Arbiter Passkeys")), m_vault(&m_store,
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/passkeys/Default"), this)
{
    connect(&m_vault, &PasskeyVault::changed, this, &PasskeyController::changed);
    if (enabled()) qApp->setProperty("arbiter.passkeys", QVariant::fromValue(static_cast<QObject *>(this)));
}
bool PasskeyController::enabled() const {
#ifdef ARBITER_PATCHED_WEBENGINE
    // The exported symbol also prevents loading a stock runtime with patched
    // headers: the dynamic loader must resolve the matching engine ABI.
    return qWebEngineArbiterWebAuthnAbi() == abi();
#else
    return false;
#endif
}
PasskeyController::~PasskeyController()
{
    clear();
    if (qApp->property("arbiter.passkeys").value<QObject *>() == this)
        qApp->setProperty("arbiter.passkeys", QVariant());
}
bool PasskeyController::localAvailable() const { return m_bridge && m_bridge->property("localAvailable").toBool(); }
bool PasskeyController::phoneAvailable() const { return m_bridge && m_bridge->property("phoneAvailable").toBool(); }
bool PasskeyController::systemAvailable() const { return m_bridge && m_bridge->property("systemAvailable").toBool(); }
bool PasskeyController::verifying() const { return m_bridge && !m_bridge->property("payload").toByteArray().isEmpty(); }
bool PasskeyController::registration() const { return m_bridge && m_bridge->property("registration").toBool(); }
int PasskeyController::bluetooth() const { return m_bridge ? m_bridge->property("bluetooth").toInt() : 1; }
QString PasskeyController::origin() const { return m_bridge ? m_bridge->property("origin").toString() : QString(); }
QVariantList PasskeyController::accounts() const
{
    if (!m_bridge || registration()) return {};
    auto result = credentials(m_bridge->property("rp").toString());
    const auto request = QCborValue::fromCbor(m_bridge->property("payload").toByteArray()).toMap();
    const auto allow = request.value(3).toArray();
    if (!allow.isEmpty()) {
        result.removeIf([&allow](const QVariant &entry) {
            auto id = entry.toMap().value(QStringLiteral("id")).toByteArray();
            for (const auto &descriptor : allow) if (descriptor.toMap().value(QStringLiteral("id")).toByteArray() == id) return false;
            return true;
        });
    }
    return result;
}
void PasskeyController::handle(QWebEngineWebAuthUxRequest *request)
{
    clear();
    if (!enabled() || !request) return;
    auto *bridge = request->property("arbiterRequest").value<QObject *>();
    if (!bridge || bridge->property("abi").toInt() != abi()) return;
    m_bridge = bridge; m_request = request;
    connect(bridge, SIGNAL(changed()), this, SLOT(update()));
    connect(bridge, &QObject::destroyed, this, &PasskeyController::clear);
    connect(request, &QWebEngineWebAuthUxRequest::stateChanged, this, [this] {
        if (m_request && (m_request->state() == QWebEngineWebAuthUxRequest::WebAuthUxState::Cancelled
            || m_request->state() == QWebEngineWebAuthUxRequest::WebAuthUxState::Completed)) clear();
    });
    update();
}
void PasskeyController::update() { emit changed(); }
void PasskeyController::clear()
{
    if (m_bridge) disconnect(m_bridge, nullptr, this, nullptr);
    if (m_request) disconnect(m_request, nullptr, this, nullptr);
    m_bridge.clear(); m_request.clear(); m_qr.clear(); emit changed();
}
void PasskeyController::setup(const QString &phrase, const QString &confirmation) { if (enabled()) m_vault.setup(phrase, confirmation); }
void PasskeyController::useLocal() { if (localAvailable() && !busy()) { m_qr.clear(); QMetaObject::invokeMethod(m_bridge, "useLocal"); } }
void PasskeyController::useSystem() { if (systemAvailable() && !busy()) { m_qr.clear(); QMetaObject::invokeMethod(m_bridge, "useSystem"); emit changed(); } }
void PasskeyController::powerOnBluetooth() { if (m_bridge) QMetaObject::invokeMethod(m_bridge, "powerOnBluetooth"); }
void PasskeyController::requestBluetoothPermission() { if (m_bridge) QMetaObject::invokeMethod(m_bridge, "requestBluetoothPermission"); }
void PasskeyController::usePhone()
{
    if (!phoneAvailable() || verifying() || busy()) return;
    const auto payload = m_bridge->property("qrPayload").toString();
    if (!payload.startsWith(QStringLiteral("FIDO:/")) || payload.size() > 2200) return;
    try {
        auto qr = qrcodegen::QrCode::encodeText(payload.toUtf8().constData(), qrcodegen::QrCode::Ecc::LOW);
        constexpr int quiet = 4, scale = 6;
        QImage image((qr.getSize() + 2 * quiet) * scale, (qr.getSize() + 2 * quiet) * scale, QImage::Format_RGB32);
        image.fill(Qt::white);
        for (int y = 0; y < qr.getSize(); ++y) for (int x = 0; x < qr.getSize(); ++x)
            if (qr.getModule(x, y)) for (int yy = 0; yy < scale; ++yy) for (int xx = 0; xx < scale; ++xx)
                image.setPixel((x + quiet) * scale + xx, (y + quiet) * scale + yy, qRgb(0, 0, 0));
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly);
        if (image.save(&buffer, "PNG")) m_qr = QUrl(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64()));
    } catch (const std::exception &) { return; }
    emit changed();
}
void PasskeyController::verify(int accountIndex, const QString &passphrase)
{
    if (!verifying() || busy()) return;
    auto entries = accounts();
    if (!registration() && (accountIndex < 0 || accountIndex >= entries.size())) return;
    QByteArray id = registration() ? QByteArray() : entries.at(accountIndex).toMap().value(QStringLiteral("id")).toByteArray();
    QPointer<QObject> bridge = m_bridge;
    QPointer<QWebEngineWebAuthUxRequest> request = m_request;
    m_vault.perform(registration(), bridge->property("payload").toByteArray(), id, passphrase,
        [bridge, request](int status, const QByteArray &response) {
            // No response is delivered after cancellation, navigation or timeout.
            if (bridge && request && request->state() != QWebEngineWebAuthUxRequest::WebAuthUxState::Cancelled
                && request->state() != QWebEngineWebAuthUxRequest::WebAuthUxState::Completed)
                QMetaObject::invokeMethod(bridge, "complete", Q_ARG(QByteArray, response), Q_ARG(int, status));
        }, [bridge, request] {
            return bridge && request && request->state() != QWebEngineWebAuthUxRequest::WebAuthUxState::Cancelled
                && request->state() != QWebEngineWebAuthUxRequest::WebAuthUxState::Completed;
        });
    emit verificationSubmitted();
}
