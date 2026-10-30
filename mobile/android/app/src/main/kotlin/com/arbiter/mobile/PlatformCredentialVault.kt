package com.arbiter.mobile

import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.util.Base64
import androidx.credentials.provider.CallingAppInfo
import com.it_nomads.fluttersecurestorage.FlutterSecureStorage
import com.it_nomads.fluttersecurestorage.FlutterSecureStorageConfig
import com.it_nomads.fluttersecurestorage.SecurePreferencesCallback
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.RandomAccessFile
import java.math.BigInteger
import java.security.*
import java.security.interfaces.ECPrivateKey
import java.security.interfaces.ECPublicKey
import java.security.spec.ECGenParameterSpec
import java.security.spec.ECParameterSpec
import java.security.spec.PKCS8EncodedKeySpec
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

object PlatformCredentialVault {
    fun encode(bytes: ByteArray): String = Base64.encodeToString(bytes,Base64.URL_SAFE or Base64.NO_WRAP or Base64.NO_PADDING)
    fun decode(value: String): ByteArray = Base64.decode(value,Base64.URL_SAFE or Base64.NO_WRAP or Base64.NO_PADDING)
    fun digest(bytes: ByteArray) = MessageDigest.getInstance("SHA-256").digest(bytes)
    private val aad = "ArbiterMobileVault/v1".toByteArray()
    fun visible(document: JSONObject): List<Pair<String,JSONObject>> {
        val records=document.getJSONObject("state").getJSONObject("records")
        return records.keys().asSequence().mapNotNull { id ->
            val versions=records.getJSONArray(id)
            if (versions.length()!=1 || versions.getJSONObject(0).getBoolean("deleted")) null
            else Pair(id,versions.getJSONObject(0).getJSONObject("data"))
        }.toList()
    }
    fun index(context: Context): List<JSONObject> = try {
        val file=File(context.filesDir,"credential-index-v1.json")
        if (!file.exists() || file.length()>8*1024*1024) emptyList()
        else JSONArray(file.readText()).let { a -> (0 until a.length()).map { a.getJSONObject(it) }.take(10000) }
    } catch (_:Exception) { emptyList() }
    fun writeIndex(context: Context, entries: List<Map<String,Any?>>) {
        val allowed=setOf("id","kind","rpId","origin","username","credentialId","userId")
        val filtered=entries.take(10000).map { entry -> JSONObject(entry.filterKeys { it in allowed }) }
        // Provider and Flutter processes can refresh public identities together.
        // Separate temporary names prevent either writer consuming the other's file.
        val temporary=File(context.filesDir,"credential-index-${java.util.UUID.randomUUID()}.tmp")
        try {
            temporary.writeText(JSONArray(filtered).toString())
            check(temporary.renameTo(File(context.filesDir,"credential-index-v1.json")))
        } finally { temporary.delete() }
    }
    fun trustedOrigin(context: Context, caller: CallingAppInfo?): String? = try {
        if (caller==null) null else {
            val allowlist=JSONObject(context.assets.open("credential-browser-allowlist.json").bufferedReader().use { it.readText() })
            val info=context.packageManager.getPackageInfo(context.packageName,PackageManager.GET_SIGNING_CERTIFICATES)
            val signatures=info.signingInfo!!.apkContentsSigners.map { signature ->
                val hash=digest(signature.toByteArray()).joinToString(":") { "%02X".format(it) }
                mapOf("build" to "release","cert_fingerprint_sha256" to hash)
            }
            allowlist.getJSONArray("apps").put(JSONObject(mapOf("type" to "android","info" to mapOf("package_name" to context.packageName,"signatures" to signatures))))
            caller.getOrigin(allowlist.toString())?.takeIf { origin ->
                val uri=Uri.parse(origin)
                uri.userInfo==null && !uri.host.isNullOrEmpty() && (uri.scheme=="https" || uri.scheme=="http" && uri.host in listOf("localhost","127.0.0.1","::1"))
            }
        }
    } catch (_:Exception) { null }
    fun relyingParty(origin: String, options: JSONObject, create: Boolean): String {
        val rp=if(create) options.getJSONObject("rp").getString("id") else options.getString("rpId")
        val host=Uri.parse(origin).host!!.lowercase()
        require(rp.isNotBlank() && rp.length<=253 && rp==rp.lowercase() && (host==rp || host.endsWith(".$rp")))
        // The verified browser performs the full public-suffix WebAuthn check.
        require(rp.contains('.') || rp=="localhost")
        return rp
    }
    fun allowed(options: JSONObject, credential: String): Boolean {
        val array=options.optJSONArray("allowCredentials") ?: return true
        return array.length()==0 || (0 until array.length()).any { array.getJSONObject(it).optString("id")==credential }
    }
    private val transactions=java.util.concurrent.Executors.newSingleThreadExecutor()
    fun afterVerification(context: Context, operation: (ByteArray,JSONObject)->Unit, failed: (Exception)->Unit) {
        transactions.execute {
            var key: ByteArray?=null
            try {
                val file=File(context.filesDir,"credentials-v1.enc")
                RandomAccessFile(File(context.filesDir,"credentials-v1.enc.lock"),"rw").use { lock ->
                    lock.channel.lock().use {
                        val storage=FlutterSecureStorage(context)
                        fun <T> waitFor(action: (SecurePreferencesCallback<T>)->Unit): T? {
                            val ready=java.util.concurrent.CountDownLatch(1)
                            var result:T?=null;var error:Exception?=null
                            action(object:SecurePreferencesCallback<T> {
                                override fun onSuccess(value:T?) {result=value;ready.countDown()}
                                override fun onError(value:Exception) {error=value;ready.countDown()}
                            })
                            ready.await();error?.let {throw it};return result
                        }
                        waitFor<Void> { storage.initialize(FlutterSecureStorageConfig(mapOf(FlutterSecureStorageConfig.PREF_OPTION_DELETE_ON_FAILURE to false)),it) }
                        val account=storage.addPrefixToKey("arbiter-vault-key-v1")
                        var encoded=waitFor<String> { storage.read(account,it) }
                        if(encoded==null) {
                            check(!file.exists()) {"The device vault key is missing"}
                            val generated=ByteArray(32).also {SecureRandom().nextBytes(it)}
                            encoded=encode(generated);generated.fill(0)
                            waitFor<Void> {storage.write(account,encoded,it)}
                        }
                        val unlocked=decode(encoded);require(unlocked.size==32);key=unlocked
                        val document=if(file.exists()) {
                            require(file.length() in 28L..(16L*1024*1024+28));val sealed=file.readBytes()
                            val cipher=Cipher.getInstance("AES/GCM/NoPadding")
                            cipher.init(Cipher.DECRYPT_MODE,SecretKeySpec(unlocked,"AES"),GCMParameterSpec(128,sealed.copyOfRange(0,12)));cipher.updateAAD(aad)
                            JSONObject(String(cipher.doFinal(sealed.copyOfRange(12,sealed.size)),Charsets.UTF_8))
                        } else JSONObject(mapOf("device" to java.util.UUID.randomUUID().toString(),"state" to mapOf("v" to 1,"records" to emptyMap<String,Any>())))
                        require(document.getJSONObject("state").getInt("v")==1);operation(unlocked,document)
                    }
                }
            } catch(error:Exception) {failed(error)}
            finally {key?.fill(0)}
        }
    }
    // The provider runs in its own process. This file lock is shared with Dart
    // and prevents an OS registration racing an encrypted sync replacement.
    fun saveCredential(context: Context,key: ByteArray,document: JSONObject,id: String,data: JSONObject) {
        val state=document.getJSONObject("state")
        val records=state.getJSONObject("records")
        val clock=JSONObject()
        val old=records.optJSONArray(id) ?: JSONArray()
        for(i in 0 until old.length()) {
            val vector=old.getJSONObject(i).getJSONObject("clock")
            vector.keys().forEach { writer -> clock.put(writer,maxOf(clock.optLong(writer,0),vector.getLong(writer))) }
        }
        val writer=document.getString("device")+"-provider"
        require(clock.length()<32 || clock.has(writer)); require(clock.optLong(writer,0)<9007199254740991L)
        clock.put(writer,clock.optLong(writer,0)+1)
        records.put(id,JSONArray().put(JSONObject(mapOf("clock" to clock,"data" to data,"deleted" to false))))
        require(records.length()<=10000)
        val clear=document.toString().toByteArray(); require(clear.size<=16*1024*1024)
        val cipher=Cipher.getInstance("AES/GCM/NoPadding"); cipher.init(Cipher.ENCRYPT_MODE,SecretKeySpec(key,"AES")); cipher.updateAAD(aad)
        val sealed=cipher.iv+cipher.doFinal(clear); clear.fill(0)
        val temporary=File(context.filesDir,"credentials-v1.enc.provider.tmp")
        temporary.outputStream().use { it.write(sealed); it.fd.sync() }
        check(temporary.renameTo(File(context.filesDir,"credentials-v1.enc")))
        writeIndex(context,visible(document).map { (id,entry) -> entry.keys().asSequence().associateWith { entry.get(it) }.toMutableMap().apply { put("id",id) } })
    }
}

object PlatformPasskey {
    private fun head(type: Int,size: Int): ByteArray = when {
        size<24 -> byteArrayOf(((type shl 5) or size).toByte())
        size<=255 -> byteArrayOf(((type shl 5) or 24).toByte(),size.toByte())
        else -> byteArrayOf(((type shl 5) or 25).toByte(),(size shr 8).toByte(),size.toByte())
    }
    private fun bytes(data: ByteArray)=head(2,data.size)+data
    private fun text(value: String)=value.toByteArray().let { head(3,it.size)+it }
    private fun coordinate(value: BigInteger): ByteArray { val bytes=value.toByteArray(); return if(bytes.size>=32) bytes.copyOfRange(bytes.size-32,bytes.size) else ByteArray(32-bytes.size)+bytes }
    fun publicCose(key: PublicKey): ByteArray {
        val point=(key as ECPublicKey).w
        return byteArrayOf(0xa5.toByte(),1,2,3,0x26,0x20,1,0x21)+bytes(coordinate(point.affineX))+byteArrayOf(0x22)+bytes(coordinate(point.affineY))
    }
    fun auth(rp: String,register: Boolean,eligible: Boolean,backed: Boolean)=PlatformCredentialVault.digest(rp.toByteArray())+
        byteArrayOf((5 or (if(register) 0x40 else 0) or (if(eligible) 8 else 0) or (if(eligible && backed) 16 else 0)).toByte(),0,0,0,0)
    fun attestation(auth: ByteArray)=byteArrayOf(0xa3.toByte())+text("fmt")+text("none")+text("attStmt")+byteArrayOf(0xa0.toByte())+text("authData")+bytes(auth)
    fun create(context: Context,key: ByteArray,document: JSONObject,options: JSONObject,rp: String,hash: ByteArray): String {
        require(hash.size==32)
        val algorithms=options.getJSONArray("pubKeyCredParams")
        require((0 until algorithms.length()).any { algorithms.getJSONObject(it).optInt("alg")== -7 })
        val user=options.getJSONObject("user"); val handle=PlatformCredentialVault.decode(user.getString("id")); require(handle.size in 1..64)
        val username=user.getString("name"); require(username.length<=256)
        val excluded=options.optJSONArray("excludeCredentials") ?: JSONArray()
        require(PlatformCredentialVault.visible(document).none { (_,data) -> data.optString("rpId")==rp && (0 until excluded.length()).any { excluded.getJSONObject(it).optString("id")==data.optString("credentialId") } })
        val generator=KeyPairGenerator.getInstance("EC");generator.initialize(ECGenParameterSpec("secp256r1")); val pair=generator.generateKeyPair()
        val id=ByteArray(32).also { SecureRandom().nextBytes(it) }; val encoded=PlatformCredentialVault.encode(id)
        val data=JSONObject(mapOf("kind" to "passkey","rpId" to rp,"username" to username,"credentialId" to encoded,
            "userId" to PlatformCredentialVault.encode(handle),"privateKey" to PlatformCredentialVault.encode(pair.private.encoded),
            "publicKey" to PlatformCredentialVault.encode(publicCose(pair.public)),"backupEligible" to true,"backedUp" to false))
        val stable="passkey:"+PlatformCredentialVault.digest(rp.toByteArray()+byteArrayOf(0)+id).joinToString("") { "%02x".format(it) }
        PlatformCredentialVault.saveCredential(context,key,document,stable,data)
        val auth=auth(rp,true,true,false)+ByteArray(16)+byteArrayOf(0,32)+id+publicCose(pair.public)
        return JSONObject(mapOf("id" to encoded,"rawId" to encoded,"type" to "public-key","authenticatorAttachment" to "platform","clientExtensionResults" to emptyMap<String,Any>(),
            "response" to mapOf("clientDataJSON" to "","attestationObject" to PlatformCredentialVault.encode(attestation(auth)),"transports" to listOf("internal")))).toString()
    }
    fun assertion(document: JSONObject,id: String,options: JSONObject,rp: String,hash: ByteArray): String {
        require(hash.size==32)
        val data=PlatformCredentialVault.visible(document).first { it.first==id }.second
        require(data.getString("kind")=="passkey" && data.getString("rpId")==rp && PlatformCredentialVault.allowed(options,data.getString("credentialId")))
        val privateKey=KeyFactory.getInstance("EC").generatePrivate(PKCS8EncodedKeySpec(PlatformCredentialVault.decode(data.getString("privateKey")))) as ECPrivateKey
        val parameters=AlgorithmParameters.getInstance("EC").apply { init(ECGenParameterSpec("secp256r1")) }.getParameterSpec(ECParameterSpec::class.java)
        require(privateKey.params.order==parameters.order && privateKey.params.generator==parameters.generator && privateKey.params.curve==parameters.curve)
        val auth=auth(rp,false,data.optBoolean("backupEligible"),data.optBoolean("backedUp"))
        val signature=Signature.getInstance("SHA256withECDSA").apply { initSign(privateKey);update(auth+hash) }.sign()
        val credential=data.getString("credentialId")
        return JSONObject(mapOf("id" to credential,"rawId" to credential,"type" to "public-key","authenticatorAttachment" to "platform","clientExtensionResults" to emptyMap<String,Any>(),
            "response" to mapOf("clientDataJSON" to "","authenticatorData" to PlatformCredentialVault.encode(auth),"signature" to PlatformCredentialVault.encode(signature),"userHandle" to data.getString("userId")))).toString()
    }
}
