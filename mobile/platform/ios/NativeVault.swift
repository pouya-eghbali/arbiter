import Foundation
import CryptoKit
import Security
import LocalAuthentication
import AuthenticationServices

// C ABI keeps Swift/native providers independent of the QML rendering runtime.
private func returned(_ data:Data?) -> UnsafeMutablePointer<CChar>? {
 guard let data else {return nil};return strdup(data.base64EncodedString())
}
@_cdecl("arbiter_ios_vault_directory")
func vaultDirectory() -> UnsafeMutablePointer<CChar>? {guard let url=try? AppleCredentialVault.directory() else{return nil};return strdup(url.path)}
@_cdecl("arbiter_ios_vault_key")
func vaultKey(_ create:Int32) -> UnsafeMutablePointer<CChar>? {returned(try? AppleCredentialVault.key(create:create != 0))}
@_cdecl("arbiter_ios_crypto")
func crypt(_ bytes:UnsafePointer<UInt8>?,_ count:Int32,_ key:UnsafePointer<UInt8>?,_ keyCount:Int32,_ aad:UnsafePointer<UInt8>?,_ aadCount:Int32,_ decrypt:Int32) -> UnsafeMutablePointer<CChar>? {
 guard let bytes,let key,let aad,keyCount==32,count>=0,count<=16*1024*1024+28,aadCount>=0,aadCount<4096 else{return nil}
 let data=Data(bytes:bytes,count:Int(count)),secret=SymmetricKey(data:Data(bytes:key,count:Int(keyCount))),context=Data(bytes:aad,count:Int(aadCount))
 do {if decrypt != 0{return returned(try AES.GCM.open(AES.GCM.SealedBox(combined:data),using:secret,authenticating:context))}
 return returned(try AES.GCM.seal(data,using:secret,authenticating:context).combined)}catch{return nil}
}
@_cdecl("arbiter_ios_refresh_identities")
func refreshIdentities(){DispatchQueue.main.async {try? AppleCredentialVault.refreshIdentities()}}
@_cdecl("arbiter_ios_verify")
func verify(_ reason:UnsafePointer<CChar>,_ callback:@escaping @convention(c)(Int32,UnsafeMutableRawPointer?)->Void,_ context:UnsafeMutableRawPointer?){
 let text=String(cString:reason)
 DispatchQueue.main.async {let authentication=LAContext();authentication.evaluatePolicy(.deviceOwnerAuthentication,localizedReason:text){approved,_ in callback(approved ? 1:0,context)}}
}
@_cdecl("arbiter_ios_legacy_session")
func legacySession()->UnsafeMutablePointer<CChar>? {let prefs=UserDefaults.standard;guard let value=prefs.string(forKey:"arbiter.browser.v1") ?? prefs.string(forKey:"flutter.arbiter.browser.v1") else{return nil};return strdup(value)}
@_cdecl("arbiter_ios_language")
func language(_ code:UnsafePointer<CChar>){UserDefaults(suiteName:AppleCredentialVault.group)?.set(String(cString:code),forKey:"interface-language")}
@_cdecl("arbiter_ios_enable_provider")
func enableProvider(){DispatchQueue.main.async {if #available(iOS 18.0,*){ASSettingsHelper.requestToTurnOnCredentialProviderExtension{enabled in if enabled{try? AppleCredentialVault.refreshIdentities()}}}else{ASSettingsHelper.openCredentialProviderAppSettings{_ in}}}}
