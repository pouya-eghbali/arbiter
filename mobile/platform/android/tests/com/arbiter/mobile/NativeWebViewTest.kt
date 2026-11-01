package com.arbiter.mobile
import android.content.Intent
import android.webkit.WebView
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.webkit.WebViewFeature
import org.junit.Test
import org.junit.Assert.*
import org.junit.runner.RunWith
import java.util.UUID
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit
@RunWith(AndroidJUnit4::class)
class NativeWebViewTest {
 @Test fun qtHostLoadsNativeEngineAndIsolatesWorkspaceStorage() {
  org.junit.Assume.assumeTrue("Use a disposable Android emulator for the browser UI test",android.os.Build.MODEL.contains("sdk_gphone")||android.os.Build.FINGERPRINT.contains("generic")||android.os.Build.PRODUCT.startsWith("sdk_"))
  val instrumentation=InstrumentationRegistry.getInstrumentation()
  val context=instrumentation.targetContext
  instrumentation.startActivitySync(Intent(context,MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
  val deadline=System.currentTimeMillis()+20000
  var ready=false
  while(System.currentTimeMillis()<deadline){
   ready=try{NativeBridge.blocked("https://googleads.g.doubleclick.net/pagead/ads.js","https://fixture.example","script","GET")}catch(_:UnsatisfiedLinkError){false}
   if(ready)break
   Thread.sleep(100)
  }
  assertTrue("The Qt host and native Rust blocker did not initialize",ready)
  assertFalse(NativeBridge.blocked("https://fixture.example/ordinary.js","https://fixture.example","script","GET"))
  assertTrue("Current Android WebView must support isolated profiles",WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE))
  val id="native-test-"+UUID.randomUUID().toString()
  val other=id+"-other"
  val script=context.assets.open("browser/forms.js").bufferedReader().use{it.readText()}
  var first:WebView?=null;var second:WebView?=null
  fun js(view:WebView,source:String):String {val result=CompletableFuture<String>();instrumentation.runOnMainSync{view.evaluateJavascript(source){result.complete(it)}};return result.get(5,TimeUnit.SECONDS)}
  try {
   first=NativeBridge.create(id,id,false,script)
   instrumentation.runOnMainSync{(MainActivity.current!!.window.decorView as android.view.ViewGroup).addView(first,android.view.ViewGroup.LayoutParams(400,800));first!!.loadDataWithBaseURL("https://fixture.example/","<title>Native fixture</title><body style='margin:0;background:#ff0000;min-height:800px'><input type=password></body>","text/html","UTF-8",null)}
   val loaded=System.currentTimeMillis()+10000
   while(System.currentTimeMillis()<loaded&&js(first!!,"typeof globalThis.__arbiterFill")!="\"function\"")Thread.sleep(100)
   assertEquals("\"function\"",js(first!!,"typeof globalThis.__arbiterFill"))
   assertEquals("\"fixture\"",js(first!!,"localStorage.setItem('arbiter-test','fixture');localStorage.getItem('arbiter-test')"))
   val painted=CompletableFuture<Boolean>()
   instrumentation.runOnMainSync{first!!.postVisualStateCallback(1,object:WebView.VisualStateCallback(){override fun onComplete(id:Long){painted.complete(true)}})}
   assertTrue(painted.get(10,TimeUnit.SECONDS))
   NativeBridge.overlay(id,.05f,.9f,.9f,.075f)
   instrumentation.runOnMainSync{
    val image=android.graphics.Bitmap.createBitmap(400,800,android.graphics.Bitmap.Config.ARGB_8888)
    image.eraseColor(android.graphics.Color.GREEN);first!!.draw(android.graphics.Canvas(image))
    assertEquals("The floating-pill cutout must reveal Qt Quick",android.graphics.Color.GREEN,image.getPixel(200,750))
    assertTrue("The page must still render outside the cutout",android.graphics.Color.red(image.getPixel(200,200))>240)
    (first as ArbiterPageView).drawPreview(android.graphics.Canvas(image))
    assertTrue("Tab previews must include the page behind the pill",android.graphics.Color.red(image.getPixel(200,750))>240)
    val press=android.view.MotionEvent.obtain(0,0,android.view.MotionEvent.ACTION_DOWN,200f,750f,0)
    assertTrue("Pill gestures must be consumed after routing to the owning Qt window",first!!.dispatchTouchEvent(press));press.recycle();image.recycle()
   }
   second=NativeBridge.create(other,other,false,script)
   instrumentation.runOnMainSync{second!!.loadDataWithBaseURL("https://fixture.example/","<title>Other fixture</title>","text/html","UTF-8",null)}
   val otherLoaded=System.currentTimeMillis()+10000
   while(System.currentTimeMillis()<otherLoaded&&js(second!!,"document.title")!="\"Other fixture\"")Thread.sleep(100)
   assertEquals("null",js(second!!,"localStorage.getItem('arbiter-test')"))
  }finally{
   instrumentation.runOnMainSync{(first?.parent as? android.view.ViewGroup)?.removeView(first)}
   NativeBridge.destroy(id);NativeBridge.destroy(other)
   instrumentation.runOnMainSync{}
   val done=CompletableFuture<Boolean>()
   instrumentation.runOnMainSync{
    val store=androidx.webkit.ProfileStore.getInstance()
    if(!WebViewFeature.isFeatureSupported(WebViewFeature.DELETE_BROWSING_DATA)){store.getProfile(id)!!.webStorage.deleteAllData();store.getProfile(other)!!.webStorage.deleteAllData();done.complete(true)}else androidx.webkit.WebStorageCompat.deleteBrowsingData(store.getProfile(id)!!.webStorage){
     androidx.webkit.WebStorageCompat.deleteBrowsingData(store.getProfile(other)!!.webStorage){done.complete(true)}
    }
   }
   assertTrue(done.get(10,TimeUnit.SECONDS))
   val stopped=CompletableFuture<Boolean>()
   val monitor=androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry.getInstance()
   val callback=androidx.test.runner.lifecycle.ActivityLifecycleCallback{activity,stage->if(activity===MainActivity.current&&stage==androidx.test.runner.lifecycle.Stage.STOPPED)stopped.complete(true)}
   instrumentation.runOnMainSync{monitor.addLifecycleCallback(callback);MainActivity.current!!.moveTaskToBack(true)}
   assertTrue(stopped.get(5,TimeUnit.SECONDS))
   instrumentation.runOnMainSync{monitor.removeLifecycleCallback(callback)}
  }
 }
}
