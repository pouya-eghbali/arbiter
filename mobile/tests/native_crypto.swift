import Foundation
import CryptoKit
@main struct NativeCryptoTest {
 static func bytes(_ hex:String)->Data {Data(stride(from:0,to:hex.count,by:2).map{index in let start=hex.index(hex.startIndex,offsetBy:index);return UInt8(hex[start..<hex.index(start,offsetBy:2)],radix:16)!})}
 static func main() throws {
  // Public P-256 test scalar 1; never an application credential.
  let fixture="308187020100301306072a8648ce3d020106082a8648ce3d030107046d306b02010104200000000000000000000000000000000000000000000000000000000000000001a144034200046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5"
  let key=try P256.Signing.PrivateKey(derRepresentation:bytes(fixture))
  let auth=PasskeyEncoding.authData("example.com",registration:false,eligible:true,backedUp:true)
  precondition(auth.count==37 && auth[32]==0x1d)
  precondition(PasskeyEncoding.authData("example.com",registration:false,eligible:false,backedUp:false)[32]==5)
  let message=auth+Data(repeating:0x63,count:32),signature=try key.signature(for:message)
  precondition(key.publicKey.isValidSignature(signature,for:message))
  precondition(!key.publicKey.isValidSignature(signature,for:message+Data([1])))
  precondition(PasskeyEncoding.cose(key).count==77)
  let sealed=try AES.GCM.seal(Data(repeating:0,count:16),using:SymmetricKey(data:Data(repeating:0,count:32)),nonce:AES.GCM.Nonce(data:Data(repeating:0,count:12)))
  precondition(sealed.combined==bytes("000000000000000000000000cea7403d4d606b6e074ec5d3baf39d18d0d1c8a799996bf0265b98b5d48ab919"))
  print("Native Swift PKCS#8, backup flags, signatures and AES-GCM vectors passed")
 }
}
