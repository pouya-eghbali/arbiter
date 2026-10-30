import Foundation
import Security
import CryptoKit
import AuthenticationServices
import Darwin

enum VaultFailure: Error { case locked, invalid, unsupported }

// Shared by the app and its credential-provider extension. The secret stays in
// their explicitly shared Keychain access group; the database contains AES-GCM.
enum AppleCredentialVault {
  static let group = "group.com.arbiter.mobile"
  // Preserve the installed vault key account; this identifier is a data compatibility contract.
  static let service = "flutter_secure_storage_service"
  static let account = "arbiter-vault-key-v1"
  static var accessGroup: String? { Bundle.main.object(forInfoDictionaryKey: "ArbiterKeychainGroup") as? String }
  static func directory() throws -> URL {
    guard let root = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: group) else { throw VaultFailure.locked }
    let url = root.appendingPathComponent("Library/Application Support", isDirectory: true)
    try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
    return url
  }
  static func unb64(_ value: String) throws -> Data {
    let padded = value.replacingOccurrences(of: "-", with: "+").replacingOccurrences(of: "_", with: "/")
    guard let bytes = Data(base64Encoded: padded + String(repeating: "=", count: (4-padded.count%4)%4)) else { throw VaultFailure.invalid }
    return bytes
  }
  static func b64(_ bytes: Data) -> String { bytes.base64EncodedString().replacingOccurrences(of: "+", with: "-").replacingOccurrences(of: "/", with: "_").replacingOccurrences(of: "=", with: "") }
  static func key(create: Bool) throws -> Data {
    var query: [String: Any] = [kSecClass as String: kSecClassGenericPassword, kSecAttrAccount as String: account,
      kSecAttrService as String: service, kSecReturnData as String: true, kSecMatchLimit as String: kSecMatchLimitOne]
    if let accessGroup { query[kSecAttrAccessGroup as String] = accessGroup }
    var result: CFTypeRef?; let status = SecItemCopyMatching(query as CFDictionary, &result)
    if status == errSecSuccess, let data = result as? Data, let string = String(data: data, encoding: .utf8) {
      let key = try unb64(string); guard key.count == 32 else { throw VaultFailure.invalid }; return key
    }
    let file = try directory().appendingPathComponent("credentials-v1.enc")
    guard status == errSecItemNotFound, create, !FileManager.default.fileExists(atPath: file.path) else { throw VaultFailure.locked }
    var bytes = Data(count: 32)
    let generated = bytes.withUnsafeMutableBytes { SecRandomCopyBytes(kSecRandomDefault,32,$0.baseAddress!) }
    guard generated == errSecSuccess else { throw VaultFailure.invalid }
    query.removeValue(forKey: kSecReturnData as String); query.removeValue(forKey: kSecMatchLimit as String)
    query[kSecValueData as String] = b64(bytes).data(using: .utf8)
    query[kSecAttrAccessible as String] = kSecAttrAccessibleWhenUnlockedThisDeviceOnly
    guard SecItemAdd(query as CFDictionary,nil) == errSecSuccess else { throw VaultFailure.locked }; return bytes
  }
  static func withLock<T>(_ operation: () throws -> T) throws -> T {
    let path = try directory().appendingPathComponent("credentials-v1.enc.lock").path
    let descriptor = open(path,O_CREAT|O_RDWR,0o600); guard descriptor >= 0 else { throw VaultFailure.locked }
    defer { close(descriptor) }
    var lock = flock(); lock.l_type = Int16(F_WRLCK); lock.l_whence = Int16(SEEK_SET)
    guard fcntl(descriptor,F_SETLKW,&lock) == 0 else { throw VaultFailure.locked }
    defer { lock.l_type = Int16(F_UNLCK); _ = fcntl(descriptor,F_SETLK,&lock) }
    return try operation()
  }
  static func read() throws -> [String: Any] {
    let file = try directory().appendingPathComponent("credentials-v1.enc")
    if !FileManager.default.fileExists(atPath: file.path) { return ["device": UUID().uuidString.lowercased(),"state": ["v": 1,"records": [String: Any]()]] }
    let bytes = try Data(contentsOf: file, options: .mappedIfSafe)
    guard bytes.count >= 28, bytes.count <= 16*1024*1024+28 else { throw VaultFailure.invalid }
    let clear = try AES.GCM.open(AES.GCM.SealedBox(combined: bytes), using: SymmetricKey(data: key(create: false)), authenticating: Data("ArbiterMobileVault/v1".utf8))
    guard let document = try JSONSerialization.jsonObject(with: clear) as? [String: Any], let state = document["state"] as? [String: Any], state["v"] as? Int == 1 else { throw VaultFailure.invalid }
    return document
  }
  static func visible(_ document: [String: Any]) -> [(String,[String: Any])] {
    let records = (document["state"] as? [String: Any])?["records"] as? [String: Any] ?? [:]
    return records.compactMap { id, raw in
      guard let versions = raw as? [[String: Any]], versions.count == 1, versions[0]["deleted"] as? Bool == false,
        let data = versions[0]["data"] as? [String: Any] else { return nil }
      return (id,data)
    }
  }
  static func update(_ data: [String: Any], id: String) throws {
    try withLock {
      let encryptionKey = try key(create: true)
      var document = try read(), state = document["state"] as? [String: Any] ?? [:]
      var records = state["records"] as? [String: Any] ?? [:], clock = [String: Int]()
      for version in records[id] as? [[String: Any]] ?? [] {
        for (writer,count) in version["clock"] as? [String: Int] ?? [:] { clock[writer] = max(clock[writer] ?? 0,count) }
      }
      let writer = (document["device"] as? String ?? UUID().uuidString.lowercased())+"-provider"
      guard clock.count < 32 || clock[writer] != nil, (clock[writer] ?? 0) < 9007199254740991 else { throw VaultFailure.invalid }
      clock[writer] = (clock[writer] ?? 0)+1
      records[id] = [["clock": clock,"data": data,"deleted": false]]
      state["v"] = 1; state["records"] = records; document["state"] = state
      let clear = try JSONSerialization.data(withJSONObject: document, options: [.sortedKeys])
      guard clear.count <= 16*1024*1024 else { throw VaultFailure.invalid }
      guard let sealed = try AES.GCM.seal(clear, using: SymmetricKey(data: encryptionKey), authenticating: Data("ArbiterMobileVault/v1".utf8)).combined else { throw VaultFailure.invalid }
      try sealed.write(to: directory().appendingPathComponent("credentials-v1.enc"),options: [.atomic,.completeFileProtection])
    }
  }
  static func refreshIdentities() throws {
    let document = try withLock { try read() }
    var identities = [any ASCredentialIdentity]()
    for (id,data) in visible(document) {
      if data["kind"] as? String == "password", let origin = data["origin"] as? String, let user = data["username"] as? String {
        identities.append(ASPasswordCredentialIdentity(serviceIdentifier: ASCredentialServiceIdentifier(identifier: origin,type: .URL), user: user, recordIdentifier: id))
      } else if data["kind"] as? String == "passkey", let rp = data["rpId"] as? String, let user = data["username"] as? String,
        let credential = data["credentialId"] as? String, let handle = data["userId"] as? String {
        identities.append(ASPasskeyCredentialIdentity(relyingPartyIdentifier: rp,userName: user,credentialID: try unb64(credential),userHandle: try unb64(handle),recordIdentifier: id))
      }
    }
    ASCredentialIdentityStore.shared.replaceCredentialIdentities(identities,completion: nil)
  }
}

enum PasskeyEncoding {
  static func head(_ major: UInt8,_ count: Int) -> Data {
    if count < 24 { return Data([major<<5|UInt8(count)]) }
    if count <= 255 { return Data([major<<5|24,UInt8(count)]) }
    return Data([major<<5|25,UInt8(count>>8),UInt8(count&255)])
  }
  static func bytes(_ value: Data) -> Data { head(2,value.count)+value }
  static func text(_ value: String) -> Data { let data=Data(value.utf8); return head(3,data.count)+data }
  static func cose(_ key: P256.Signing.PrivateKey) -> Data {
    let point=key.publicKey.x963Representation
    return Data([0xa5,0x01,0x02,0x03,0x26,0x20,0x01,0x21])+bytes(Data(point[1..<33]))+Data([0x22])+bytes(Data(point[33..<65]))
  }
  static func authData(_ rp: String,registration: Bool,eligible: Bool,backedUp: Bool) -> Data {
    Data(SHA256.hash(data: Data(rp.utf8)))+Data([0x05 | (registration ? 0x40 : 0) | (eligible ? 0x08 : 0) | (eligible && backedUp ? 0x10 : 0),0,0,0,0])
  }
  static func attestation(_ data: Data) -> Data {
    Data([0xa3])+text("fmt")+text("none")+text("attStmt")+Data([0xa0])+text("authData")+bytes(data)
  }
}

private var arbiterCatalogs:[String:[String:String]]=[:]
func ArbiterText(_ source:String,_ values:String...)->String {
  let selected=UserDefaults(suiteName:AppleCredentialVault.group)?.string(forKey:"interface-language") ?? "system"
  let code=selected=="system" ? (Locale.preferredLanguages.first.flatMap{Locale(identifier:$0).language.languageCode?.identifier} ?? "en") : selected
  let language=["en","de","fr","it","es","pt","ja","zh"].contains(code) ? code : "en"
  if arbiterCatalogs[language]==nil {
    let url=Bundle.main.url(forResource:language,withExtension:"json",subdirectory:"Translations") ?? Bundle.main.url(forResource:language,withExtension:"json",subdirectory:"i18n")
    if let url,let data=try? Data(contentsOf:url),let parsed=try? JSONSerialization.jsonObject(with:data) as? [String:String] {arbiterCatalogs[language]=parsed}
    else {arbiterCatalogs[language]=[:]}
  }
  var result=arbiterCatalogs[language]?[source] ?? source
  for (i,value) in values.enumerated(){result=result.replacingOccurrences(of:"{\(i)}",with:value)}
  return result
}
