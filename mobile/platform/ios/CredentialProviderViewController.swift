import UIKit
import AuthenticationServices
import LocalAuthentication
import CryptoKit

final class CredentialProviderViewController: ASCredentialProviderViewController {
  private var pending: (any ASCredentialRequest)?
  private var registering = false
  private var configuring = false
  private var verifying = false
  private let stack = UIStackView()
  private let detail = UILabel()
  override func viewDidLoad() {
    super.viewDidLoad(); view.backgroundColor = UIColor(red: 0.06,green: 0.09,blue: 0.13,alpha: 1)
    stack.axis = .vertical; stack.spacing = 18; stack.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(stack)
    NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo:view.safeAreaLayoutGuide.leadingAnchor,constant:28),
      stack.trailingAnchor.constraint(equalTo:view.safeAreaLayoutGuide.trailingAnchor,constant:-28),stack.centerYAnchor.constraint(equalTo:view.centerYAnchor)])
    let title=UILabel(); title.text="Arbiter"; title.font = .systemFont(ofSize:30,weight:.semibold); title.textColor = .white; stack.addArrangedSubview(title)
    detail.text=ArbiterText("Unlock your passwords and passkeys"); detail.numberOfLines=0; detail.textColor = .lightGray; stack.addArrangedSubview(detail)
    let unlock=UIButton(type:.system); var style=UIButton.Configuration.filled(); style.title=ArbiterText("Unlock"); style.cornerStyle = .capsule
    unlock.configuration=style; unlock.addTarget(self,action:#selector(authenticate),for:.touchUpInside); stack.addArrangedSubview(unlock)
    let cancel=UIButton(type:.system); cancel.setTitle(ArbiterText("Cancel"),for:.normal); cancel.addTarget(self,action:#selector(cancelRequest),for:.touchUpInside); stack.addArrangedSubview(cancel)
  }
  override func viewWillAppear(_ animated: Bool) {
    super.viewWillAppear(animated)
    if let identity=pending?.credentialIdentity as? ASPasskeyCredentialIdentity {
      detail.text=ArbiterText(registering ? "Create a passkey for {0}" : "Sign in to {0}",identity.relyingPartyIdentifier)+"\n"+identity.userName
    } else if let identity=pending?.credentialIdentity as? ASPasswordCredentialIdentity {
      detail.text=ArbiterText("Fill a password for {0}",identity.serviceIdentifier.identifier)+"\n"+identity.user
    } else if configuring { detail.text=ArbiterText("Enable Arbiter passwords and passkeys on this device") }
  }
  override func provideCredentialWithoutUserInteraction(for credentialRequest: any ASCredentialRequest) {
    extensionContext.cancelRequest(withError:NSError(domain:ASExtensionErrorDomain,code:ASExtensionError.userInteractionRequired.rawValue))
  }
  override func prepareInterfaceToProvideCredential(for credentialRequest: any ASCredentialRequest) { pending=credentialRequest; registering=false }
  override func prepareInterface(forPasskeyRegistration registrationRequest: any ASCredentialRequest) { pending=registrationRequest; registering=true }
  override func prepareInterfaceForExtensionConfiguration() { configuring=true }
  @objc private func cancelRequest() { extensionContext.cancelRequest(withError:NSError(domain:ASExtensionErrorDomain,code:ASExtensionError.userCanceled.rawValue)) }
  @objc private func authenticate() {
    if verifying { return }; verifying=true
    let authentication=LAContext()
    authentication.evaluatePolicy(.deviceOwnerAuthentication,localizedReason:ArbiterText("Unlock your Arbiter credential vault")) { [weak self] approved,_ in
      DispatchQueue.main.async {
        guard let self else { return }; self.verifying=false
        guard approved else { self.cancelRequest(); return }
        do {
          if self.configuring {
            _ = try AppleCredentialVault.withLock { try AppleCredentialVault.key(create:true) }; try AppleCredentialVault.refreshIdentities()
            self.extensionContext.completeExtensionConfigurationRequest(); return
          }
          guard let request=self.pending else { throw VaultFailure.invalid }
          if let passkey=request as? ASPasskeyCredentialRequest {
            if self.registering { try self.create(passkey) } else { try self.assert(passkey) }
          } else if let identity=request.credentialIdentity as? ASPasswordCredentialIdentity { try self.password(identity) }
          else { throw VaultFailure.unsupported }
        } catch {
          self.extensionContext.cancelRequest(withError:NSError(domain:ASExtensionErrorDomain,code:ASExtensionError.credentialIdentityNotFound.rawValue))
        }
      }
    }
  }
  private func selected(_ id: String?) throws -> [String:Any] {
    guard let id, let record=try AppleCredentialVault.withLock({ try AppleCredentialVault.visible(AppleCredentialVault.read()).first(where:{$0.0==id})?.1 }) else { throw VaultFailure.invalid }
    return record
  }
  private func password(_ identity: ASPasswordCredentialIdentity) throws {
    let data=try selected(identity.recordIdentifier)
    guard data["kind"] as? String=="password",let origin=data["origin"] as? String,let site=URL(string:origin),site.scheme=="https",
      let username=data["username"] as? String,username==identity.user,let password=data["password"] as? String else { throw VaultFailure.invalid }
    let requested=identity.serviceIdentifier
    if requested.type == .domain {
      guard site.host?.lowercased()==requested.identifier.lowercased() else { throw VaultFailure.invalid }
    } else { guard URL(string:requested.identifier)?.host?.lowercased()==site.host?.lowercased() else { throw VaultFailure.invalid } }
    extensionContext.completeRequest(withSelectedCredential:ASPasswordCredential(user:username,password:password),completionHandler:nil)
  }
  private func assert(_ request: ASPasskeyCredentialRequest) throws {
    guard let identity=request.credentialIdentity as? ASPasskeyCredentialIdentity, request.clientDataHash.count==32 else { throw VaultFailure.invalid }
    let data=try selected(identity.recordIdentifier)
    guard data["kind"] as? String=="passkey",data["rpId"] as? String==identity.relyingPartyIdentifier,
      let id=data["credentialId"] as? String,try AppleCredentialVault.unb64(id)==identity.credentialID,
      let secret=data["privateKey"] as? String,let handle=data["userId"] as? String else { throw VaultFailure.invalid }
    let key=try P256.Signing.PrivateKey(derRepresentation:AppleCredentialVault.unb64(secret))
    let auth=PasskeyEncoding.authData(identity.relyingPartyIdentifier,registration:false,eligible:data["backupEligible"] as? Bool ?? false,backedUp:data["backedUp"] as? Bool ?? false)
    let signature=try key.signature(for:auth+request.clientDataHash).derRepresentation
    let credential=ASPasskeyAssertionCredential(userHandle:try AppleCredentialVault.unb64(handle),relyingParty:identity.relyingPartyIdentifier,
      signature:signature,clientDataHash:request.clientDataHash,authenticatorData:auth,credentialID:identity.credentialID)
    extensionContext.completeAssertionRequest(using:credential,completionHandler:nil)
  }
  private func create(_ request: ASPasskeyCredentialRequest) throws {
    guard let identity=request.credentialIdentity as? ASPasskeyCredentialIdentity, request.clientDataHash.count==32,
      request.supportedAlgorithms.contains(.ES256),identity.userHandle.count>0,identity.userHandle.count<=64,identity.userName.count<=256 else { throw VaultFailure.unsupported }
    let rp=identity.relyingPartyIdentifier
    guard !rp.isEmpty,rp.count<=253 else { throw VaultFailure.invalid }
    let document=try AppleCredentialVault.withLock { try AppleCredentialVault.read() }
    if #available(iOS 18.0, *) {
      let excluded=request.excludedCredentials?.map(\.credentialID) ?? []
      for (_,record) in AppleCredentialVault.visible(document) where record["rpId"] as? String==rp {
        if let id=record["credentialId"] as? String,excluded.contains(try AppleCredentialVault.unb64(id)) { throw VaultFailure.invalid }
      }
    }
    let key=P256.Signing.PrivateKey(); var id=Data(count:32)
    guard id.withUnsafeMutableBytes({SecRandomCopyBytes(kSecRandomDefault,32,$0.baseAddress!)})==errSecSuccess else { throw VaultFailure.invalid }
    let stable="passkey:"+SHA256.hash(data:Data(rp.utf8)+Data([0])+id).map{String(format:"%02x",$0)}.joined()
    let data:[String:Any] = ["kind":"passkey","rpId":rp,"username":identity.userName,"credentialId":AppleCredentialVault.b64(id),
      "userId":AppleCredentialVault.b64(identity.userHandle),"privateKey":AppleCredentialVault.b64(key.derRepresentation),
      "publicKey":AppleCredentialVault.b64(PasskeyEncoding.cose(key)),"backupEligible":true,"backedUp":false]
    try AppleCredentialVault.update(data,id:stable); try AppleCredentialVault.refreshIdentities()
    let auth=PasskeyEncoding.authData(rp,registration:true,eligible:true,backedUp:false)+Data(repeating:0,count:16)+Data([0,32])+id+PasskeyEncoding.cose(key)
    let credential=ASPasskeyRegistrationCredential(relyingParty:rp,clientDataHash:request.clientDataHash,credentialID:id,attestationObject:PasskeyEncoding.attestation(auth))
    extensionContext.completeRegistrationRequest(using:credential,completionHandler:nil)
  }
}
