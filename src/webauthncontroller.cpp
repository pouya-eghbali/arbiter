#include "webauthncontroller.h"
#include <QSet>

using State = QWebEngineWebAuthUxRequest::WebAuthUxState;
using Failure = QWebEngineWebAuthUxRequest::RequestFailureReason;

WebAuthnController::~WebAuthnController() { cancel(); }
int WebAuthnController::state() const { return m_request ? int(m_request->state()) : int(State::Completed); }
QString WebAuthnController::relyingParty() const { return m_request ? m_request->relyingPartyId() : QString(); }
QStringList WebAuthnController::accounts() const
{
    if (!m_request || m_request->state() != State::SelectAccount) return {};
    const auto names = m_request->userNames();
    // Qt selects by username, not credential ID. Don't silently select the
    // wrong credential if the authenticator supplies indistinguishable names.
    if (QSet<QString>(names.begin(), names.end()).size() != names.size()) return {};
    return names;
}

bool WebAuthnController::confirmPin() const
{
    return m_request && m_request->state() == State::CollectPin
        && m_request->pinRequest().reason != QWebEngineWebAuthUxRequest::PinEntryReason::Challenge;
}

bool WebAuthnController::canRetry() const
{
    if (!m_request || m_request->state() != State::RequestFailed) return false;
    const auto failure = m_request->requestFailureReason();
    return failure == Failure::KeyAlreadyRegistered || failure == Failure::SoftPinBlock
        || failure == Failure::AuthenticatorRemovedDuringPinEntry;
}

QString WebAuthnController::description() const
{
    if (!m_request) return {};
    switch (m_request->state()) {
    case State::SelectAccount:
        return accounts().isEmpty() ? tr("The authenticator did not provide distinguishable account names. Cancel and try another authentication method.")
                                    : tr("Choose the account to use with this website.");
    case State::CollectPin:
        return confirmPin() ? tr("Choose and confirm a PIN for your security key. This PIN is managed by the security key.")
                            : tr("Enter your security key's PIN to continue.");
    case State::FinishTokenCollection:
        return tr("Follow the instructions on your authenticator. You may need to touch your security key again.");
    case State::RequestFailed:
        switch (m_request->requestFailureReason()) {
        case Failure::Timeout: return tr("The authentication request timed out. Start sign-in again on the website.");
        case Failure::KeyNotRegistered: return tr("This security key is not registered with this website.");
        case Failure::KeyAlreadyRegistered: return tr("This security key is already registered. Try another key.");
        case Failure::SoftPinBlock: return tr("Too many incorrect PIN attempts. Remove and reinsert your security key before retrying.");
        case Failure::HardPinBlock: return tr("This security key's PIN is blocked. Use another authentication method.");
        case Failure::AuthenticatorRemovedDuringPinEntry: return tr("The security key was removed. Reinsert it before retrying.");
        case Failure::AuthenticatorMissingResidentKeys: return tr("This authenticator cannot store discoverable credentials.");
        case Failure::AuthenticatorMissingUserVerification: return tr("This authenticator cannot provide the user verification this website requires.");
        case Failure::AuthenticatorMissingLargeBlob: return tr("This authenticator does not support the requested large-blob extension.");
        case Failure::NoCommonAlgorithms: return tr("The website and authenticator do not share a supported signing algorithm.");
        case Failure::StorageFull: return tr("The authenticator's credential storage is full.");
        case Failure::UserConsentDenied: return tr("Authentication was declined.");
        case Failure::WinUserCancelled: return tr("The Windows authentication dialog was cancelled.");
        }
        break;
    default:
        return tr("Connect a compatible security key, or follow any system authentication prompt.");
    }
    return tr("Authentication could not be completed.");
}

QString WebAuthnController::error() const
{
    if (!m_error.isEmpty()) return m_error;
    if (!m_request || m_request->state() != State::CollectPin) return {};
    const auto pin = m_request->pinRequest();
    switch (pin.error) {
    case QWebEngineWebAuthUxRequest::PinEntryError::NoError: return {};
    case QWebEngineWebAuthUxRequest::PinEntryError::WrongPin:
        return pin.remainingAttempts >= 0 ? tr("Incorrect PIN. %1 attempts remaining.").arg(pin.remainingAttempts) : tr("Incorrect PIN.");
    case QWebEngineWebAuthUxRequest::PinEntryError::TooShort: return tr("The PIN is too short.");
    case QWebEngineWebAuthUxRequest::PinEntryError::InvalidCharacters: return tr("The PIN contains unsupported characters.");
    case QWebEngineWebAuthUxRequest::PinEntryError::SameAsCurrentPin: return tr("Choose a different PIN from the existing one.");
    case QWebEngineWebAuthUxRequest::PinEntryError::InternalUvLocked: return tr("Built-in verification is locked. Use your security key's PIN.");
    }
    return {};
}

QString WebAuthnController::validatePin(const QString &pin, const QString &confirmation,
                                       int minimumLength, bool confirmationRequired)
{
    if (pin.contains(QChar(0)) || pin.toUtf8().size() > 63)
        return tr("The PIN must not contain a null character or exceed 63 UTF-8 bytes.");
    if (pin.toUcs4().size() < qMax(4, minimumLength))
        return tr("Use at least %1 characters for the PIN.").arg(qMax(4, minimumLength));
    if (confirmationRequired && pin != confirmation) return tr("The PINs do not match.");
    return {};
}

void WebAuthnController::handle(QWebEngineWebAuthUxRequest *request)
{
    if (m_request == request) return;
    cancel();
    m_request = request;
    if (!request) return;
    connect(request, &QWebEngineWebAuthUxRequest::stateChanged, this, &WebAuthnController::update);
    connect(request, &QObject::destroyed, this, [this] { m_request.clear(); m_error.clear(); emit changed(); });
    update();
}

void WebAuthnController::update()
{
    m_error.clear();
    if (m_request && (m_request->state() == State::Cancelled || m_request->state() == State::Completed)) {
        disconnect(m_request, nullptr, this, nullptr);
        m_request.clear();
    }
    emit changed();
}

void WebAuthnController::cancel()
{
    const auto request = m_request;
    m_request.clear();
    m_error.clear();
    if (request) {
        disconnect(request, nullptr, this, nullptr);
        request->cancel();
    }
    emit changed();
}

void WebAuthnController::retry()
{
    if (canRetry()) { m_error.clear(); m_request->retry(); }
}

void WebAuthnController::selectAccount(const QString &account)
{
    if (m_request && m_request->state() == State::SelectAccount && accounts().contains(account))
        m_request->setSelectedAccount(account);
}

void WebAuthnController::submitPin(const QString &pin, const QString &confirmation)
{
    if (!m_request || m_request->state() != State::CollectPin) return;
    m_error = validatePin(pin, confirmation, m_request->pinRequest().minPinLength, confirmPin());
    if (!m_error.isEmpty()) { emit changed(); return; }
    m_request->setPin(pin);
    emit pinSubmitted();
}
