// SPDX-License-Identifier: BSD-3-Clause
#include "arbiter_webauthn.h"
#include <QCoreApplication>
#include <algorithm>
#include <QVariant>
#include "base/task/single_thread_task_runner.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_authenticator.h"

namespace QtWebEngineCore {
QObject *arbiterPasskeyProvider()
{
    auto *provider = qApp->property("arbiter.passkeys").value<QObject *>();
    return provider && provider->property("abi").toInt() == 1 ? provider : nullptr;
}
namespace {
using namespace device;
std::vector<uint8_t> bytes(const QByteArray &value) { return {value.begin(), value.end()}; }
class Authenticator final : public FidoAuthenticator {
public:
    explicit Authenticator(ArbiterWebAuthnRequest *request) : m_request(request) {
        m_options.is_platform_device = AuthenticatorSupportedOptions::PlatformDevice::kYes;
        m_options.supports_resident_key = true;
        m_options.user_verification_availability = AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured;
        m_options.always_uv = true;
        m_connection = QObject::connect(request, &ArbiterWebAuthnRequest::completed, request,
            [weak = m_weak.GetWeakPtr()](const QByteArray &response, int status) { if (weak) weak->complete(response, status); });
    }
    ~Authenticator() override { QObject::disconnect(m_connection); }
    void InitializeAuthenticator(base::OnceClosure callback) override { std::move(callback).Run(); }
    void MakeCredential(CtapMakeCredentialRequest request, MakeCredentialOptions options, MakeCredentialCallback callback) override {
        m_make = std::move(callback);
        send(AsCTAPRequestValuePair(request).second);
    }
    void GetAssertion(CtapGetAssertionRequest request, CtapGetAssertionOptions options, GetAssertionCallback callback) override {
        m_get = std::move(callback);
        send(AsCTAPRequestValuePair(request).second);
    }
    void GetPlatformCredentialInfoForRequest(const CtapGetAssertionRequest &request, const CtapGetAssertionOptions &options,
                                             GetPlatformCredentialInfoForRequestCallback callback) override {
        QVariantList entries;
        if (auto *provider = arbiterPasskeyProvider())
            QMetaObject::invokeMethod(provider, "credentials", Qt::DirectConnection,
                Q_RETURN_ARG(QVariantList, entries), Q_ARG(QString, QString::fromStdString(request.rp_id)));
        std::vector<DiscoverableCredentialMetadata> credentials;
        for (const auto &entry : entries) {
            const auto map = entry.toMap();
            auto id = bytes(map.value("id").toByteArray());
            if (!request.allow_list.empty() && std::none_of(request.allow_list.begin(), request.allow_list.end(),
                    [&id](const auto &descriptor) { return descriptor.id == id; })) continue;
            PublicKeyCredentialUserEntity user(bytes(map.value("userId").toByteArray()));
            user.name = map.value("name").toString().toStdString();
            credentials.emplace_back(AuthenticatorType::kOther, request.rp_id, std::move(id), std::move(user), "Arbiter");
        }
        auto recognized = credentials.empty() ? FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential
                                             : FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential;
        std::move(callback).Run(std::move(credentials), recognized);
    }
    void Cancel() override { m_make.Reset(); m_get.Reset(); m_weak.InvalidateWeakPtrs(); QObject::disconnect(m_connection); }
    std::string GetId() const override { return "arbiter-local-v1"; }
    const AuthenticatorSupportedOptions &Options() const override { return m_options; }
    ProtocolVersion SupportedProtocol() const override { return ProtocolVersion::kCtap2; }
    std::optional<base::span<const int32_t>> GetAlgorithms() override {
        static constexpr int32_t algorithms[] = {-7};
        return base::span<const int32_t>(algorithms);
    }
    std::optional<FidoTransportProtocol> AuthenticatorTransport() const override { return FidoTransportProtocol::kInternal; }
    base::WeakPtr<FidoAuthenticator> GetWeakPtr() override { return m_weak.GetWeakPtr(); }
private:
    void send(const std::optional<cbor::Value> &value) {
        auto encoded = value ? cbor::Writer::Write(*value) : std::nullopt;
        if (!m_request || !encoded) { complete({}, 1); return; }
        m_request->payload = QByteArray(reinterpret_cast<const char *>(encoded->data()), qsizetype(encoded->size()));
        Q_EMIT m_request->changed();
    }
    void complete(const QByteArray &encoded, int status) {
        auto decoded = encoded.size() <= 65536 ? cbor::Reader::Read(bytes(encoded)) : std::nullopt;
        if (m_request) { m_request->payload.clear(); Q_EMIT m_request->changed(); }
        if (m_make) {
            auto response = status == 0 ? ReadCTAPMakeCredentialResponse(FidoTransportProtocol::kInternal, decoded) : std::nullopt;
            if (response) {
                response->is_resident_key = true;
                response->transports = base::flat_set<FidoTransportProtocol>{FidoTransportProtocol::kInternal};
            }
            auto result = response ? MakeCredentialStatus::kSuccess : status == 0 ? MakeCredentialStatus::kAuthenticatorResponseInvalid
                : status == 2 ? MakeCredentialStatus::kUserConsentButCredentialExcluded
                : status == 3 ? MakeCredentialStatus::kNoCommonAlgorithms : MakeCredentialStatus::kUserConsentDenied;
            std::move(m_make).Run(result, std::move(response));
        } else if (m_get) {
            auto response = status == 0 ? ReadCTAPGetAssertionResponse(FidoTransportProtocol::kInternal, decoded) : std::nullopt;
            std::vector<AuthenticatorGetAssertionResponse> responses;
            if (response) { response->user_selected = true; responses.push_back(std::move(*response)); }
            auto result = !responses.empty() ? GetAssertionStatus::kSuccess : status == 0 ? GetAssertionStatus::kAuthenticatorResponseInvalid
                : status == 4 ? GetAssertionStatus::kUserConsentButCredentialNotRecognized : GetAssertionStatus::kUserConsentDenied;
            std::move(m_get).Run(result, std::move(responses));
        }
    }
    QPointer<ArbiterWebAuthnRequest> m_request;
    QMetaObject::Connection m_connection;
    AuthenticatorSupportedOptions m_options;
    MakeCredentialCallback m_make;
    GetAssertionCallback m_get;
    base::WeakPtrFactory<Authenticator> m_weak{this};
};
class Discovery final : public FidoDiscoveryBase {
public:
    explicit Discovery(ArbiterWebAuthnRequest *request) : FidoDiscoveryBase(FidoTransportProtocol::kInternal), m_authenticator(request) {}
    void Start() override {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
            base::BindOnce([](base::WeakPtr<Discovery> self) {
                if (self && self->observer()) self->observer()->DiscoveryStarted(self.get(), true, {&self->m_authenticator});
            }, m_weak.GetWeakPtr()));
    }
private:
    Authenticator m_authenticator;
    base::WeakPtrFactory<Discovery> m_weak{this};
};
}
std::unique_ptr<device::FidoDiscoveryBase> createArbiterDiscovery(ArbiterWebAuthnRequest *request) { return std::make_unique<Discovery>(request); }
}
