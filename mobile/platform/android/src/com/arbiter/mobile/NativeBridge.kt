package com.arbiter.mobile

import android.app.Activity
import android.app.AlertDialog
import android.app.KeyguardManager
import android.app.DownloadManager
import android.app.role.RoleManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.hardware.biometrics.BiometricPrompt
import android.net.Uri
import android.os.Build
import android.os.CancellationSignal
import android.os.Environment
import android.view.WindowManager
import android.webkit.*
import android.media.AudioManager
import android.util.Base64
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.File
import java.util.concurrent.ConcurrentHashMap
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

// Clip only the floating QML controls, not a whole bottom toolbar strip.
class ArbiterPageView(context:android.content.Context):WebView(context){
 var overlay=android.graphics.RectF()
 var settled:(()->Unit)?=null
 private var forwarding=false
 private val settledTask=Runnable{settled?.invoke()}
 private fun frame()=android.graphics.RectF(overlay.left*width,overlay.top*height,overlay.right*width,overlay.bottom*height)
 override fun draw(canvas:android.graphics.Canvas){
  val rect=frame()
  if(rect.isEmpty){super.draw(canvas);return}
  // ViewGroup draws a scrolled child in its content coordinate system. The
  // cutout belongs to the viewport, so compensate before clipping the canvas.
  rect.offset(scrollX.toFloat(),scrollY.toFloat())
  val saved=canvas.save()
  val path=android.graphics.Path().apply{addRoundRect(rect,minOf(32*resources.displayMetrics.density,rect.height()/2),minOf(32*resources.displayMetrics.density,rect.height()/2),android.graphics.Path.Direction.CW)}
  canvas.clipOutPath(path);super.draw(canvas);canvas.restoreToCount(saved)
 }
 fun drawPreview(canvas:android.graphics.Canvas){
  // Match ViewGroup.drawChild's viewport transform when rendering offscreen.
  val saved=canvas.save();canvas.translate(-scrollX.toFloat(),-scrollY.toFloat())
  super.draw(canvas);canvas.restoreToCount(saved)
 }
 override fun dispatchTouchEvent(event:android.view.MotionEvent):Boolean {
  if(event.actionMasked==android.view.MotionEvent.ACTION_DOWN)forwarding=frame().contains(event.x,event.y)
  if(!forwarding)return super.dispatchTouchEvent(event)
  // Returning false sends the event to the foreign QtWindow wrapper, which has
  // no QML scene. Deliver it to the owning Qt window instead, in its coordinates.
  // Keep the whole gesture there even if the pointer leaves the rounded pill.
  val owner=(parent as? android.view.View)?.parent as? android.view.View
  if(owner!=null){
   val sourceLocation=IntArray(2);val ownerLocation=IntArray(2)
   getLocationOnScreen(sourceLocation);owner.getLocationOnScreen(ownerLocation)
   val translated=android.view.MotionEvent.obtain(event)
   translated.offsetLocation((sourceLocation[0]-ownerLocation[0]).toFloat(),(sourceLocation[1]-ownerLocation[1]).toFloat())
   try{owner.onTouchEvent(translated)}finally{translated.recycle()}
  }
  if(event.actionMasked==android.view.MotionEvent.ACTION_UP||event.actionMasked==android.view.MotionEvent.ACTION_CANCEL)forwarding=false
  return true
 }
 override fun onScrollChanged(l:Int,t:Int,oldl:Int,oldt:Int){super.onScrollChanged(l,t,oldl,oldt);removeCallbacks(settledTask);postDelayed(settledTask,250)}
 override fun destroy(){removeCallbacks(settledTask);settled=null;super.destroy()}
}
object NativeBridge {
 private val sources=ConcurrentHashMap<String,String>()
 private val scripts=ConcurrentHashMap<String,String>()
 private val privateProfiles=ConcurrentHashMap<String,String>()
 private val views=ConcurrentHashMap<String,WebView>()
 private var fileCallback:ValueCallback<Array<Uri>>?=null
 private val permissions by lazy { WebPermissions(
  current = { id, origin -> views[id]?.url?.let { WebPermissions.sameOrigin(it, origin) } == true },
  granted = { activity().checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED },
  request = { names, code -> activity().requestPermissions(names, code) },
  prompt = { origin, location, reply ->
   val dialog=AlertDialog.Builder(activity()).setTitle(Uri.parse(origin).host)
    .setMessage(MobileStrings.text(activity(),if(location)"Allow location access?" else "Allow camera or microphone access?"))
    .setNegativeButton(MobileStrings.text(activity(),"Deny")){_,_->reply(false)}
    .setPositiveButton(MobileStrings.text(activity(),"Allow")){_,_->reply(true)}
    .setOnCancelListener{reply(false)}.create()
   dialog.show()
   val close:()->Unit = { dialog.dismiss() }
   close
  }) }
 @JvmStatic external fun event(id:String,type:String,json:String)
 @JvmStatic external fun verified(approved:Boolean)
 @JvmStatic external fun openUrl(url:String)
 @JvmStatic external fun blocked(url:String,source:String,kind:String,method:String):Boolean
 private fun activity():Activity=MainActivity.current!!
 private val downloadExecutor=java.util.concurrent.Executors.newSingleThreadScheduledExecutor()
 private val downloadMonitorStarted=java.util.concurrent.atomic.AtomicBoolean(false)
 private val downloadSamples=mutableMapOf<Long,Pair<Long,Long>>()
 private val finishedDownloads=mutableSetOf<Long>()
 private fun pollDownloads(){try{
  val manager=activity().getSystemService(DownloadManager::class.java)
  manager.query(DownloadManager.Query()).use{cursor->while(cursor.moveToNext()){
   fun number(column:String)=cursor.getLong(cursor.getColumnIndexOrThrow(column))
   fun text(column:String)=cursor.getString(cursor.getColumnIndexOrThrow(column)) ?: ""
   val id=number(DownloadManager.COLUMN_ID);if(id in finishedDownloads)continue
   val bytes=number(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR);val total=number(DownloadManager.COLUMN_TOTAL_SIZE_BYTES);val stamp=android.os.SystemClock.elapsedRealtime()
   val previous=downloadSamples.put(id,Pair(stamp,bytes));val speed=if(previous!=null&&stamp>previous.first)maxOf(0,(bytes-previous.second)*1000/(stamp-previous.first))else 0
   val status=number(DownloadManager.COLUMN_STATUS).toInt();val complete=status==DownloadManager.STATUS_SUCCESSFUL;val failed=status==DownloadManager.STATUS_FAILED
   event("","download",JSONObject(mapOf("id" to id.toString(),"name" to text(DownloadManager.COLUMN_TITLE),"url" to text(DownloadManager.COLUMN_URI))).toString())
   event("","downloadProgress",JSONObject(mapOf("id" to id.toString(),"received" to bytes,"total" to total,"speed" to speed,"complete" to complete,"error" to failed,"path" to (if(complete)manager.getUriForDownloadedFile(id)?.toString() ?: "" else ""))).toString())
   if(complete||failed){finishedDownloads.add(id);downloadSamples.remove(id)}
  }}
 }catch(_:Exception){}}
 @JvmStatic fun initialize(){
  if(downloadMonitorStarted.compareAndSet(false,true))downloadExecutor.scheduleWithFixedDelay({pollDownloads()},1,1,java.util.concurrent.TimeUnit.SECONDS)
  activity().runOnUiThread{
  activity().intent?.dataString?.let{if(Uri.parse(it).scheme in listOf("http","https"))openUrl(it)}
  if(WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE)){
   val store=androidx.webkit.ProfileStore.getInstance()
   for(name in store.allProfileNames)if(name.startsWith("private-"))try{store.deleteProfile(name)}catch(_:IllegalStateException){}
  }
 }}
 @JvmStatic fun shutdown(){downloadExecutor.shutdownNow();activity().runOnUiThread{for(id in views.keys.toList())destroy(id)}}
 private fun clearPrivateProfile(name:String){
  val store=androidx.webkit.ProfileStore.getInstance()
  val profile=store.getProfile(name) ?: return
  profile.cookieManager.removeAllCookies(null);profile.geolocationPermissions.clearAll()
  androidx.webkit.WebStorageCompat.deleteBrowsingData(profile.webStorage){
   // WebView may keep an in-memory Profile reference until process exit.
   // Its storage is cleared now; remove the profile shell at next startup.
   try{store.deleteProfile(name)}catch(_:IllegalStateException){}
  }
 }
 @JvmStatic fun directory():String=activity().filesDir.absolutePath
 @JvmStatic fun vaultKey(create:Boolean):ByteArray=try{VaultKey.read(activity(),create)}catch(_:Exception){ByteArray(0)}
 @JvmStatic fun crypt(bytes:ByteArray,key:ByteArray,aad:ByteArray,decrypt:Boolean):ByteArray=try{
  require(key.size==32&&bytes.size<=16*1024*1024+28)
  val cipher=Cipher.getInstance("AES/GCM/NoPadding")
  if(decrypt){require(bytes.size>=28);cipher.init(Cipher.DECRYPT_MODE,SecretKeySpec(key,"AES"),GCMParameterSpec(128,bytes.copyOfRange(0,12)));cipher.updateAAD(aad);cipher.doFinal(bytes.copyOfRange(12,bytes.size))}
  else{cipher.init(Cipher.ENCRYPT_MODE,SecretKeySpec(key,"AES"));cipher.updateAAD(aad);cipher.iv+cipher.doFinal(bytes)}
 }catch(_:Exception){ByteArray(0)}
 @JvmStatic fun background(enabled:Boolean){BackgroundSync.configure(activity(),enabled)}
 @JvmStatic fun refreshIdentities(){PlatformCredentialVault.afterVerification(activity(),{_,document->val records=PlatformCredentialVault.visible(document).map{(id,data)->val value=mutableMapOf<String,Any?>("id" to id);for(key in listOf("kind","rpId","origin","username","credentialId","userId"))if(data.has(key))value[key]=data.get(key);value};PlatformCredentialVault.writeIndex(activity(),records)},{})}
 @JvmStatic fun language(code:String){MobileStrings.language(activity(),code)}
 @JvmStatic fun share(url:String){activity().runOnUiThread{activity().startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).apply{type="text/plain";putExtra(Intent.EXTRA_TEXT,url)},null))}}
 @JvmStatic fun secure(value:Boolean){activity().runOnUiThread{if(value)activity().window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)else activity().window.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)}}
 @JvmStatic fun appSettings(){activity().runOnUiThread{activity().startActivity(Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:"+activity().packageName)))}}
 @JvmStatic fun defaultBrowser(){activity().runOnUiThread{if(Build.VERSION.SDK_INT>=29){val manager=activity().getSystemService(RoleManager::class.java);activity().startActivity(manager.createRequestRoleIntent(RoleManager.ROLE_BROWSER))}else activity().startActivity(Intent(android.provider.Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS))}}
 @JvmStatic fun credentialProvider(){activity().runOnUiThread{if(Build.VERSION.SDK_INT>=34)androidx.credentials.CredentialManager.create(activity()).createSettingsPendingIntent().send()}}
 @JvmStatic fun verify(reason:String){activity().runOnUiThread{
  val builder=BiometricPrompt.Builder(activity()).setTitle("Arbiter").setDescription(reason)
  if(Build.VERSION.SDK_INT>=29)builder.setDeviceCredentialAllowed(true)else builder.setNegativeButton(MobileStrings.text(activity(),"Use device passcode"),activity().mainExecutor){_,_->passcode(reason)}
  builder.build().authenticate(CancellationSignal(),activity().mainExecutor,object:BiometricPrompt.AuthenticationCallback(){
   override fun onAuthenticationSucceeded(result:BiometricPrompt.AuthenticationResult){verified(true)}
   override fun onAuthenticationError(code:Int,message:CharSequence){if(code==BiometricPrompt.BIOMETRIC_ERROR_NO_BIOMETRICS||code==BiometricPrompt.BIOMETRIC_ERROR_HW_NOT_PRESENT)passcode(reason)else verified(false)}
  })
 }}
 private fun passcode(reason:String){val intent=activity().getSystemService(KeyguardManager::class.java).createConfirmDeviceCredentialIntent("Arbiter",reason);if(intent==null)verified(false)else activity().startActivityForResult(intent,71001)}
 @JvmStatic fun create(id:String,profile:String,privateMode:Boolean,script:String):WebView {
  // Qt creates native windows on its application thread; Android UI creation
  // must complete before QWindow wraps the returned Java View.
  var view:WebView?=null;var failure:Exception?=null;val ready=java.util.concurrent.CountDownLatch(1)
  activity().runOnUiThread{try{
   val web=ArbiterPageView(activity());web.settled={capture(id)};view=web;scripts[id]=script;if(privateMode)privateProfiles[id]=profile
   web.settings.javaScriptEnabled=true;web.settings.allowFileAccess=false;web.settings.allowContentAccess=false;web.settings.mixedContentMode=WebSettings.MIXED_CONTENT_NEVER_ALLOW;web.settings.mediaPlaybackRequiresUserGesture=true
   if(WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE))WebViewCompat.setProfile(web,profile)
   else check(profile=="shared"&&!privateMode){"This Android WebView does not support isolated profiles"}
   web.settings.domStorageEnabled=!privateMode
   if(privateMode){check(WebViewFeature.isFeatureSupported(WebViewFeature.DELETE_BROWSING_DATA)){"Update Android System WebView to use private browsing"};web.settings.cacheMode=WebSettings.LOAD_NO_CACHE;WebViewCompat.getProfile(web).cookieManager.setAcceptCookie(false)}
   if(WebViewFeature.isFeatureSupported(WebViewFeature.WEB_AUTHENTICATION))androidx.webkit.WebSettingsCompat.setWebAuthenticationSupport(web.settings,androidx.webkit.WebSettingsCompat.WEB_AUTHENTICATION_SUPPORT_FOR_BROWSER)
   check(WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_LISTENER)){"Update Android System WebView to enable credential suggestions"}
   WebViewCompat.addWebMessageListener(web,"arbiterForms",setOf("*")){_,message,origin,mainFrame,_->
    if(mainFrame&&origin.scheme in listOf("https","http"))try{val data=JSONObject(message.data!!);val type=data.optString("type");if(type in listOf("credentialFocus","loginSubmitted","loginSucceeded","activityState")&&(!data.has("origin")||data.getString("origin")==origin.toString()))event(id,type,data.toString())}catch(_:Exception){}
   }
   web.webViewClient=object:WebViewClient(){
    override fun shouldOverrideUrlLoading(view:WebView,request:WebResourceRequest):Boolean {val uri=request.url;if(uri.scheme in listOf("https","http"))return false;if(request.hasGesture()&&uri.scheme in listOf("mailto","tel","sms"))try{activity().startActivity(Intent(Intent.ACTION_VIEW,uri))}catch(_:Exception){};return true}
    override fun shouldInterceptRequest(view:WebView,request:WebResourceRequest):WebResourceResponse? {
     if(request.isForMainFrame)return null
     val url=request.url.toString();val path=request.url.path?.lowercase() ?: ""
     val kind=when{path.endsWith(".js")->"script";path.endsWith(".css")->"stylesheet";path.matches(Regex(".*\\.(png|jpg|jpeg|gif|webp|svg|ico)$"))->"image";else->"other"}
     return if(blocked(url,sources[id] ?: "",kind,request.method))WebResourceResponse("text/plain","UTF-8",ByteArrayInputStream(ByteArray(0)))else null
    }
    override fun onPageStarted(view:WebView,url:String,icon:android.graphics.Bitmap?){permissions.cancel(id);event(id,"start","{}");location(id,view)}
    override fun onPageFinished(view:WebView,url:String){location(id,view);view.evaluateJavascript(scripts[id] ?: script,null);event(id,"finish","{\"success\":true}")}
    override fun onReceivedError(view:WebView,request:WebResourceRequest,error:WebResourceError){if(request.isForMainFrame)event(id,"error",JSONObject(mapOf("message" to error.description.toString())).toString())}
    override fun onReceivedSslError(view:WebView,handler:SslErrorHandler,error:android.net.http.SslError){handler.cancel()}
    override fun onRenderProcessGone(view:WebView,detail:RenderProcessGoneDetail):Boolean {if(views[id]===view){permissions.cancel(id);views.remove(id);view.destroy();event(id,"error",JSONObject(mapOf("message" to MobileStrings.text(activity(),"The page stopped responding. Reload to try again."),"crashed" to true)).toString())};return true}
   }
   web.webChromeClient=object:WebChromeClient(){
    override fun onProgressChanged(view:WebView,value:Int){event(id,"progress","{\"progress\":$value}")}
    override fun onReceivedTitle(view:WebView,title:String){event(id,"title",JSONObject(mapOf("title" to title)).toString())}
    override fun onPermissionRequest(request:PermissionRequest){permissions.media(id,request)}
    override fun onPermissionRequestCanceled(request:PermissionRequest){permissions.cancel(request)}
    override fun onGeolocationPermissionsShowPrompt(origin:String,callback:GeolocationPermissions.Callback){permissions.location(id,origin,callback)}
    override fun onGeolocationPermissionsHidePrompt(){permissions.cancel(id)}
    override fun onShowFileChooser(view:WebView,callback:ValueCallback<Array<Uri>>,params:FileChooserParams):Boolean {fileCallback?.onReceiveValue(null);fileCallback=callback;return try{activity().startActivityForResult(params.createIntent(),71002);true}catch(_:Exception){fileCallback=null;false}}
   }
   web.setDownloadListener{url,userAgent,disposition,mime,_->if(!privateMode){val name=URLUtil.guessFileName(url,disposition,mime).replace(Regex("[^A-Za-z0-9._ -]"),"_");val request=DownloadManager.Request(Uri.parse(url)).setTitle(name).setMimeType(mime).setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED).setDestinationInExternalFilesDir(activity(),Environment.DIRECTORY_DOWNLOADS,name).addRequestHeader("User-Agent",userAgent)
    val cookies=if(WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE))WebViewCompat.getProfile(web).cookieManager.getCookie(url)else CookieManager.getInstance().getCookie(url);if(!cookies.isNullOrEmpty())request.addRequestHeader("Cookie",cookies)
    val download=activity().getSystemService(DownloadManager::class.java).enqueue(request);event(id,"download",JSONObject(mapOf("id" to download.toString(),"url" to url,"name" to name)).toString())}}
   web.setOnTouchListener{_,touch->if(touch.actionMasked==android.view.MotionEvent.ACTION_DOWN)event(id,"activity","{}");false}
   views[id]=web
  }catch(error:Exception){failure=error;view?.destroy();view=null}finally{ready.countDown()}}
  ready.await();return view ?: throw (failure ?: IllegalStateException("Native WebView creation failed"))
 }
 private fun location(id:String,view:WebView){sources[id]=view.url ?: "";event(id,"location",JSONObject(mapOf("url" to (view.url ?: ""),"back" to view.canGoBack(),"forward" to view.canGoForward())).toString())}
 @JvmStatic fun navigate(id:String,url:String){activity().runOnUiThread{views[id]?.loadUrl(url)}}
 @JvmStatic fun action(id:String,name:String){activity().runOnUiThread{views[id]?.let{when(name){"back"->it.goBack();"forward"->it.goForward();"reload"->it.reload()}}}}
 @JvmStatic fun evaluate(id:String,script:String){activity().runOnUiThread{views[id]?.evaluateJavascript(script,null)}}
 @JvmStatic fun inspect(id:String){activity().runOnUiThread{views[id]?.evaluateJavascript("globalThis.__arbiterHasPassword?.()??true"){value->event(id,"loginInspection",JSONObject(mapOf("hasPassword" to (value!="false"))).toString())}}}
 @JvmStatic fun discard(id:String,expected:WebView?){activity().runOnUiThread{if(expected!=null&&views[id]===expected)destroy(id)}}
 @JvmStatic fun destroy(id:String){activity().runOnUiThread{permissions.cancel(id);views.remove(id)?.let{it.stopLoading();it.destroy()};sources.remove(id);scripts.remove(id);findQueries.remove(id);privateProfiles.remove(id)?.let{profile->if(!privateProfiles.containsValue(profile))clearPrivateProfile(profile)}}}
 @JvmStatic fun configure(id:String,script:String,prefs:String){scripts[id]=script;evaluate(id,"globalThis.__arbiterUpdatePreferences?.($prefs)")}
 @JvmStatic fun overlay(id:String,x:Float,y:Float,width:Float,height:Float){activity().runOnUiThread{(views[id] as? ArbiterPageView)?.let{it.overlay=android.graphics.RectF(x,y,x+width,y+height);it.invalidate()}}}
 @JvmStatic fun capture(id:String){activity().runOnUiThread{val view=views[id] as? ArbiterPageView ?: return@runOnUiThread;if(view.width<=0||view.height<=0)return@runOnUiThread;try{val scale=minOf(1f,640f/view.width);val bitmap=android.graphics.Bitmap.createBitmap(maxOf(1,(view.width*scale).toInt()),maxOf(1,(view.height*scale).toInt()),android.graphics.Bitmap.Config.ARGB_8888);val canvas=android.graphics.Canvas(bitmap);canvas.scale(scale,scale);view.drawPreview(canvas);val stream=java.io.ByteArrayOutputStream();bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,90,stream);bitmap.recycle();event(id,"preview",JSONObject(mapOf("image" to "data:image/png;base64,"+android.util.Base64.encodeToString(stream.toByteArray(),android.util.Base64.NO_WRAP),"sourceUrl" to (view.url ?: ""))).toString())}catch(_:Exception){}}}
 private val findQueries=ConcurrentHashMap<String,String>()
 @JvmStatic fun find(id:String,query:String,backwards:Boolean){activity().runOnUiThread{views[id]?.let{if(query.isEmpty()){it.clearMatches();findQueries.remove(id)}else if(findQueries[id]==query)it.findNext(!backwards)else{findQueries[id]=query;it.findAllAsync(query)}}}}
 @JvmStatic fun canSleep():Boolean = !activity().getSystemService(AudioManager::class.java).isMusicActive
 fun activityResult(request:Int,result:Int,data:Intent?){when(request){71001->verified(result==Activity.RESULT_OK);71002->{fileCallback?.onReceiveValue(WebChromeClient.FileChooserParams.parseResult(result,data));fileCallback=null}}}
 fun permissionsResult(request:Int,names:Array<out String>,results:IntArray){permissions.result(request,names,results)}
}
