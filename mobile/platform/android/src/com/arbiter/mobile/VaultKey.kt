package com.arbiter.mobile

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.AtomicFile
import android.util.Base64
import java.io.File
import java.security.KeyStore
import java.security.SecureRandom
import java.security.PrivateKey
import java.security.spec.MGF1ParameterSpec
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.OAEPParameterSpec
import javax.crypto.spec.PSource

// Only the wrapping key lives in AndroidKeyStore. Its encrypted random vault key
// is shared by the Qt app and its OS credential provider, never by web content.
object VaultKey {
 private const val alias="ArbiterVaultWrapping.v1"
 private val aad="ArbiterVaultWrapping/v1".toByteArray()
 @Synchronized fun read(context:Context,create:Boolean):ByteArray {
  val keys=KeyStore.getInstance("AndroidKeyStore").apply{load(null)}
  val file=AtomicFile(File(context.filesDir,"arbiter-vault-key-v1.wrap"))
  if(file.baseFile.exists()){
   val bytes=file.readFully();require(bytes.size in 28..256)
   val key=keys.getKey(alias,null) as? SecretKey ?: error("Vault wrapping key is unavailable")
   val cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.DECRYPT_MODE,key,GCMParameterSpec(128,bytes.copyOfRange(0,12)));cipher.updateAAD(aad)
   return cipher.doFinal(bytes.copyOfRange(12,bytes.size)).also{require(it.size==32)}
  }
  val legacy=legacyKey(context,keys)
  check(legacy!=null || !File(context.filesDir,"credentials-v1.enc").exists()){ "The existing vault key is unavailable; the vault has been preserved" }
  check(create || legacy!=null){"No vault key"}
  val clear=legacy ?: ByteArray(32).also{SecureRandom().nextBytes(it)}
  val key=(keys.getKey(alias,null) as? SecretKey) ?: KeyGenerator.getInstance("AES","AndroidKeyStore").apply{
   init(KeyGenParameterSpec.Builder(alias,KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT).setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).setKeySize(256).setUnlockedDeviceRequired(true).build())
  }.generateKey()
  val cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.ENCRYPT_MODE,key);cipher.updateAAD(aad)
  val stream=file.startWrite();try{stream.write(cipher.iv+cipher.doFinal(clear));file.finishWrite(stream)}catch(error:Exception){file.failWrite(stream);throw error}
  return clear
 }
 // Read-only interoperability with the prototype's default RSA-OAEP/AES-GCM
 // key envelope. Keep legacy key entries until the new wrapped key is durable.
 private fun legacyKey(context:Context,keys:KeyStore):ByteArray? {
  val values=context.getSharedPreferences("FlutterSecureStorage",Context.MODE_PRIVATE)
  val prefix="VGhpcyBpcyB0aGUgcHJlZml4IGZvciBhIHNlY3VyZSBzdG9yYWdlCg"
  val value=values.getString(prefix+"_arbiter-vault-key-v1",null) ?: return null
  val stored=context.getSharedPreferences("FlutterSecureKeyStorage",Context.MODE_PRIVATE).getString("AESVGhpcyBpcyB0aGUga2V5IGZvciBhIHNlY3VyZSBzdG9yYWdlIEFFUyBLZXkK",null) ?: error("Legacy vault wrapping key missing")
  val privateKey=keys.getKey(context.packageName+".FlutterSecureStoragePluginKeyOAEP",null) as? PrivateKey ?: error("Legacy key is unavailable")
  val rsa=Cipher.getInstance("RSA/ECB/OAEPPadding");rsa.init(Cipher.UNWRAP_MODE,privateKey,OAEPParameterSpec("SHA-256","MGF1",MGF1ParameterSpec.SHA1,PSource.PSpecified.DEFAULT))
  val aes=rsa.unwrap(Base64.decode(stored,Base64.DEFAULT),"AES",Cipher.SECRET_KEY)
  val bytes=Base64.decode(value,Base64.DEFAULT);require(bytes.size>=28)
  val cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.DECRYPT_MODE,aes,GCMParameterSpec(128,bytes.copyOfRange(0,12)))
  val encoded=cipher.doFinal(bytes.copyOfRange(12,bytes.size));return Base64.decode(String(encoded),Base64.URL_SAFE or Base64.NO_WRAP).also{encoded.fill(0);require(it.size==32)}
 }
}
