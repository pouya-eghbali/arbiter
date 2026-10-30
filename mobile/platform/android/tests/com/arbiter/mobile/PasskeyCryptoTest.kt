package com.arbiter.mobile
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Test
import org.junit.Assert.*
import org.junit.runner.RunWith
import org.json.JSONObject
import org.json.JSONArray
import java.security.*
import java.security.spec.*
import javax.crypto.*
import javax.crypto.spec.*

@RunWith(AndroidJUnit4::class)
class PasskeyCryptoTest {
  // Public test scalar 1, never an application credential.
  private val fixture="308187020100301306072a8648ce3d020106082a8648ce3d030107046d306b02010104200000000000000000000000000000000000000000000000000000000000000001a144034200046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5"
  private fun bytes(hex:String)=hex.chunked(2).map {it.toInt(16).toByte()}.toByteArray()
  @Test fun importedPKCS8SignsTheBoundRelyingParty() {
    val id=PlatformCredentialVault.encode(ByteArray(32) {1})
    val data=JSONObject(mapOf("kind" to "passkey","rpId" to "example.com","credentialId" to id,"privateKey" to PlatformCredentialVault.encode(bytes(fixture)),
      "userId" to PlatformCredentialVault.encode("fixture-user".toByteArray()),"backupEligible" to true,"backedUp" to true))
    val document=JSONObject(mapOf("state" to mapOf("v" to 1,"records" to mapOf("fixture" to listOf(mapOf("clock" to mapOf("fixture" to 1),"data" to data,"deleted" to false))))))
    val hash=ByteArray(32) {0x63}
    val response=JSONObject(PlatformPasskey.assertion(document,"fixture",JSONObject(mapOf("rpId" to "example.com","allowCredentials" to emptyList<Any>())),"example.com",hash)).getJSONObject("response")
    val auth=PlatformCredentialVault.decode(response.getString("authenticatorData"));assertEquals(0x1d,auth[32].toInt())
    val params=AlgorithmParameters.getInstance("EC").apply {init(ECGenParameterSpec("secp256r1"))}.getParameterSpec(ECParameterSpec::class.java)
    val publicKey=KeyFactory.getInstance("EC").generatePublic(ECPublicKeySpec(params.generator,params))
    val verify=Signature.getInstance("SHA256withECDSA").apply {initVerify(publicKey);update(auth+hash)}
    assertTrue(verify.verify(PlatformCredentialVault.decode(response.getString("signature"))))
    assertEquals(5,PlatformPasskey.auth("example.com",false,false,false)[32].toInt())
    try { PlatformPasskey.assertion(document,"fixture",JSONObject(),"unrelated.example",hash);fail("Mismatched RP was accepted") } catch (_:IllegalArgumentException) {}
  }
  @Test fun nistAES256GCMEnvelopeMatchesDesktop() {
    val cipher=Cipher.getInstance("AES/GCM/NoPadding")
    cipher.init(Cipher.ENCRYPT_MODE,SecretKeySpec(ByteArray(32),"AES"),GCMParameterSpec(128,ByteArray(12)))
    assertArrayEquals(bytes("000000000000000000000000cea7403d4d606b6e074ec5d3baf39d18d0d1c8a799996bf0265b98b5d48ab919"),ByteArray(12)+cipher.doFinal(ByteArray(16)))
  }
}
