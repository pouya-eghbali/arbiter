package com.arbiter.mobile

import android.app.KeyguardManager
import android.app.job.JobInfo
import android.app.job.JobParameters
import android.app.job.JobScheduler
import android.app.job.JobService
import android.content.ComponentName
import android.content.Context
import android.net.Uri
import android.os.PersistableBundle
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest
import java.security.cert.X509Certificate
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit
import javax.net.ssl.*

object BackgroundSync {
 const val ID=71024
 @JvmStatic external fun merge(local:String,remote:String):String?
 fun configure(context:Context,enabled:Boolean,smoke:Boolean=false){val scheduler=context.getSystemService(JobScheduler::class.java);if(!enabled){scheduler.cancel(ID);return};scheduler.schedule(JobInfo.Builder(ID,ComponentName(context,SyncJobService::class.java)).setRequiredNetworkType(JobInfo.NETWORK_TYPE_UNMETERED).setPeriodic(15*60*1000L).setPersisted(!smoke).setExtras(PersistableBundle().apply{putBoolean("smoke",smoke)}).build())}
 @Synchronized fun loadCore(context:Context){
  val directory=File(context.applicationInfo.nativeLibraryDir)
  for(prefix in listOf("libc++_shared","libQt6Core_","libarbiter_sync_core")){
   val file=directory.listFiles()?.firstOrNull{it.name.startsWith(prefix)&&it.name.endsWith(".so")} ?: error("Native sync dependency unavailable")
   System.load(file.absolutePath)
  }
 }
 fun read(context:Context):Pair<ByteArray,JSONObject>{val result=CompletableFuture<Pair<ByteArray,JSONObject>>();PlatformCredentialVault.afterVerification(context,{key,document->result.complete(Pair(key.copyOf(),JSONObject(document.toString())))},{result.completeExceptionally(it)});return result.get(10,TimeUnit.SECONDS)}
 fun local(uri:Uri):Boolean {if(uri.scheme!="https"||uri.userInfo!=null||uri.query!=null||uri.fragment!=null||uri.path !in listOf("","/"))return false;val host=uri.host?.lowercase() ?: return false;if(host=="localhost"||host.endsWith(".local")||host=="::1"||host.startsWith("fe80:")||host.contains(':')&&(host.startsWith("fc")||host.startsWith("fd")))return true;val parts=host.split('.').map{it.toIntOrNull() ?: return false};return parts.size==4&&parts.all{it in 0..255}&&(parts[0] in listOf(10,127)||parts[0]==192&&parts[1]==168||parts[0]==172&&parts[1] in 16..31)}
}
class SyncJobService:JobService(){
 private var worker:Thread?=null
 @Volatile private var connection:HttpsURLConnection?=null
 @Volatile private var stopped=false
 override fun onStartJob(parameters:JobParameters):Boolean {
  stopped=false;worker=Thread{
   var success=false;var localKey:ByteArray?=null;var peerKey:ByteArray?=null
   try{
    BackgroundSync.loadCore(this)
    if(parameters.extras.getBoolean("smoke")){check(BackgroundSync.merge("{\"v\":1,\"records\":{}}","{\"v\":1,\"records\":{}}")!=null);success=true}
    else if(!getSystemService(KeyguardManager::class.java).isDeviceLocked){
     val (key,document)=BackgroundSync.read(this);localKey=key
     val peer=document.optJSONObject("peer")
     if(peer==null)success=true else {
      val endpoint=Uri.parse(peer.getString("endpoint"));check(BackgroundSync.local(endpoint));check(peer.getString("pin").matches(Regex("[0-9a-f]{64}")))
      val secret=PlatformCredentialVault.decode(peer.getString("key"));require(secret.size==32);peerKey=secret
      val pin=peer.getString("pin")
      val trust=object:X509TrustManager{
       override fun getAcceptedIssuers()=emptyArray<X509Certificate>()
       override fun checkClientTrusted(chain:Array<X509Certificate>,auth:String){error("Not a server")}
       override fun checkServerTrusted(chain:Array<X509Certificate>,auth:String){check(chain.isNotEmpty()&&MessageDigest.getInstance("SHA-256").digest(chain[0].encoded).joinToString(""){"%02x".format(it)}==pin){"Desktop certificate mismatch"}}
      }
      val tls=SSLContext.getInstance("TLS").apply{init(null,arrayOf(trust),null)}
      val url=java.net.URL(endpoint.buildUpon().path("/v1/sync").build().toString());val request=url.openConnection() as HttpsURLConnection;connection=request
      request.sslSocketFactory=tls.socketFactory;request.hostnameVerifier=HostnameVerifier{host,session->host==endpoint.host&&session.peerCertificates.firstOrNull()?.let{MessageDigest.getInstance("SHA-256").digest(it.encoded).joinToString(""){b->"%02x".format(b)}==pin}==true}
      request.instanceFollowRedirects=false;request.connectTimeout=6000;request.readTimeout=15000;request.requestMethod="POST";request.doOutput=true
      request.setRequestProperty("Authorization","Bearer "+peer.getString("token"));request.setRequestProperty("Content-Type","application/octet-stream")
      val aad=("ArbiterSync/v1/"+peer.getString("vaultId")).toByteArray()
      val cipher=javax.crypto.Cipher.getInstance("AES/GCM/NoPadding");cipher.init(javax.crypto.Cipher.ENCRYPT_MODE,javax.crypto.spec.SecretKeySpec(secret,"AES"));cipher.updateAAD(aad)
      val body=cipher.iv+cipher.doFinal(document.getJSONObject("state").toString().toByteArray());request.setFixedLengthStreamingMode(body.size)
      check(!stopped&&!getSystemService(KeyguardManager::class.java).isDeviceLocked)
      request.outputStream.use{it.write(body)};check(request.responseCode==200)
      val output=java.io.ByteArrayOutputStream();request.inputStream.use{input->val part=ByteArray(16384);while(true){val count=input.read(part);if(count<0)break;check(output.size()+count<=16*1024*1024+28);output.write(part,0,count)}}
      val bytes=output.toByteArray();require(bytes.size>=28);cipher.init(javax.crypto.Cipher.DECRYPT_MODE,javax.crypto.spec.SecretKeySpec(secret,"AES"),javax.crypto.spec.GCMParameterSpec(128,bytes.copyOfRange(0,12)));cipher.updateAAD(aad)
      val clear=cipher.doFinal(bytes.copyOfRange(12,bytes.size));val remote=String(clear,Charsets.UTF_8);clear.fill(0)
      check(!stopped&&!getSystemService(KeyguardManager::class.java).isDeviceLocked)
      val saved=CompletableFuture<Boolean>()
      PlatformCredentialVault.afterVerification(this,{vaultKey,current->
       check(!stopped&&current.optJSONObject("peer")?.optString("token")==peer.getString("token")){"Pairing changed"}
       val merged=BackgroundSync.merge(current.getJSONObject("state").toString(),remote) ?: error("Invalid sync state")
       current.put("state",JSONObject(merged));PlatformCredentialVault.writeDocument(this,vaultKey,current);saved.complete(true)
      },{saved.completeExceptionally(it)})
      success=saved.get(15,TimeUnit.SECONDS)
     }
    }
   }catch(_:Exception){}finally{localKey?.fill(0);peerKey?.fill(0);connection?.disconnect();connection=null;File(filesDir,"background-last-run.json").writeText(JSONObject(mapOf("time" to System.currentTimeMillis(),"success" to success)).toString());if(!stopped)jobFinished(parameters,false);worker=null}
  }.apply{start()};return true
 }
 override fun onStopJob(parameters:JobParameters):Boolean{stopped=true;connection?.disconnect();worker?.interrupt();return true}
}
