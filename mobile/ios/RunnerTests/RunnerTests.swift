import XCTest
import CryptoKit
@testable import Runner

final class RunnerTests: XCTestCase {
  // Public test scalar 1, never an application credential.
  private let fixture="308187020100301306072a8648ce3d020106082a8648ce3d030107046d306b02010104200000000000000000000000000000000000000000000000000000000000000001a144034200046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5"
  private func bytes(_ hex:String) -> Data { Data(stride(from:0,to:hex.count,by:2).map { index in
    let a=hex.index(hex.startIndex,offsetBy:index);return UInt8(hex[a..<hex.index(a,offsetBy:2)],radix:16)!
  }) }
  func testImportedPKCS8KeyAndBackupFlags() throws {
    let key=try P256.Signing.PrivateKey(derRepresentation:bytes(fixture))
    let auth=PasskeyEncoding.authData("example.com",registration:false,eligible:true,backedUp:true)
    XCTAssertEqual(auth.count,37);XCTAssertEqual(auth[32],0x1d)
    XCTAssertEqual(PasskeyEncoding.authData("example.com",registration:false,eligible:false,backedUp:false)[32],5)
    let message=auth+Data(repeating:0x63,count:32)
    let signature=try key.signature(for:message)
    XCTAssertTrue(key.publicKey.isValidSignature(signature,for:message))
    XCTAssertFalse(key.publicKey.isValidSignature(signature,for:message+Data([1])))
    XCTAssertEqual(PasskeyEncoding.cose(key).count,77)
  }
  func testAES256GCMUsesTheSameEnvelopeAsDesktopAndDart() throws {
    let sealed=try AES.GCM.seal(Data(repeating:0,count:16),using:SymmetricKey(data:Data(repeating:0,count:32)),
      nonce:AES.GCM.Nonce(data:Data(repeating:0,count:12)))
    XCTAssertEqual(sealed.combined,bytes("000000000000000000000000cea7403d4d606b6e074ec5d3baf39d18d0d1c8a799996bf0265b98b5d48ab919"))
  }
  func testForegroundAndBackgroundVaultTransactionsAreSerialized() {
    let gate=VaultTransactions();var foreground:String?;var background:String?;var closed=false
    gate.acquire("foreground"){foreground=$0 as? String}
    gate.acquire("background"){background=$0 as? String}
    XCTAssertNotNil(foreground);XCTAssertNil(background)
    gate.release("foreground",token:foreground!)
    XCTAssertNotNil(background)
    gate.closeWhenIdle("background"){closed=true}
    XCTAssertFalse(closed)
    gate.release("background",token:background!)
    XCTAssertTrue(closed)
  }
  func testCancellingAQueuedVaultTransactionDoesNotBlockOtherClients() {
    let gate=VaultTransactions();var first:String?;var third:String?;var cancellation:Any?
    gate.acquire("first"){first=$0 as? String}
    gate.acquire("cancelled"){cancellation=$0}
    gate.acquire("third"){third=$0 as? String}
    gate.closeWhenIdle("cancelled"){}
    XCTAssertNotNil(cancellation);XCTAssertNil(third)
    gate.release("first",token:first!)
    XCTAssertNotNil(third);gate.release("third",token:third!)
  }
}
