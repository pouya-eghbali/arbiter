package com.arbiter.mobile

import android.content.Intent
import android.graphics.RectF
import android.os.SystemClock
import android.view.InputDevice
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.net.InetAddress
import java.net.ServerSocket
import kotlin.concurrent.thread
import kotlin.math.abs

// Exercises the actual Qt window + foreign WebView hierarchy with system touch
// injection and screenshots. A synthetic local page avoids internet dependencies.
@RunWith(AndroidJUnit4::class)
class PillInteractionTest {
 @Test(timeout=60000) fun realPageScrollKeepsPillFixedAndTabsButtonWorks() {
  assumeTrue("Disposable emulator only",android.os.Build.MODEL.contains("sdk_gphone")||android.os.Build.FINGERPRINT.contains("generic")||android.os.Build.PRODUCT.startsWith("sdk_"))
  val instrumentation=InstrumentationRegistry.getInstrumentation()
  val context=instrumentation.targetContext
  val server=ServerSocket(0,10,InetAddress.getByName("127.0.0.1"))
  val worker=thread(isDaemon=true){
   try{while(!server.isClosed)server.accept().use{client->
    val reader=client.getInputStream().bufferedReader();while(!reader.readLine().isNullOrEmpty()){}
    val body="<meta name='viewport' content='width=device-width,initial-scale=1'><title>Pill scroll regression</title><body style='margin:0;background:#cc0088;min-height:12000px'></body>".toByteArray()
    client.getOutputStream().apply{write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ${body.size}\r\nConnection: close\r\n\r\n".toByteArray());write(body);flush()}
   }}catch(_:java.net.SocketException){}
  }
  fun waitFor(message:String,condition:()->Boolean){
   val end=SystemClock.uptimeMillis()+15000
   while(SystemClock.uptimeMillis()<end){if(condition())return;Thread.sleep(100)}
   fail(message)
  }
  fun <T> ui(block:()->T):T {
   val result=java.util.concurrent.CompletableFuture<T>()
   instrumentation.runOnMainSync{try{result.complete(block())}catch(error:Throwable){result.completeExceptionally(error)}}
   return result.get(5,java.util.concurrent.TimeUnit.SECONDS)
  }
  fun touch(action:Int,x:Float,y:Float,down:Long){
   val event=MotionEvent.obtain(down,SystemClock.uptimeMillis(),action,x,y,0)
   event.source=InputDevice.SOURCE_TOUCHSCREEN
   try{assertTrue(instrumentation.uiAutomation.injectInputEvent(event,true))}finally{event.recycle()}
  }
  fun tap(x:Float,y:Float){val down=SystemClock.uptimeMillis();touch(MotionEvent.ACTION_DOWN,x,y,down);Thread.sleep(50);touch(MotionEvent.ACTION_UP,x,y,down)}
  fun find(view:View):ArbiterPageView? {
   if(view is ArbiterPageView&&view.isShown)return view
   if(view is ViewGroup)for(index in 0 until view.childCount)find(view.getChildAt(index))?.let{return it}
   return null
  }
  fun rect(web:ArbiterPageView):RectF=ui {
   val location=IntArray(2);web.getLocationOnScreen(location)
   RectF(location[0]+web.overlay.left*web.width,location[1]+web.overlay.top*web.height,
    location[0]+web.overlay.right*web.width,location[1]+web.overlay.bottom*web.height)
  }
  fun phase(value:String){android.util.Log.i("PillE2E",value)}
  try {
   phase("launch")
   // This Activity is singleTop and may already exist after another test.
   // startActivitySync waits for a new instance and hangs on that reuse path.
   instrumentation.runOnMainSync{context.startActivity(Intent(context,MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT))}
   phase("wait native")
   waitFor("Qt did not initialize"){try{NativeBridge.blocked("https://googleads.g.doubleclick.net/pagead/ads.js","https://fixture.example","script","GET")}catch(_:UnsatisfiedLinkError){false}}
   phase("native ready")
   // A fresh emulator must complete onboarding through the same UI as a user.
   val session=java.io.File(context.filesDir,"browser-session-v1.json")
   val setupDone=try{org.json.JSONObject(session.readText()).getJSONObject("settings").optBoolean("onboardingDone")}catch(_:Exception){false}
   if(!setupDone){
    // Wait for the actual first frame, not just JNI initialization. Qt's
    // accessibility tree may not yet expose the first onboarding delegates.
    val density=context.resources.displayMetrics.density
    waitFor("Onboarding's first frame did not render") {
     val image=instrumentation.uiAutomation.takeScreenshot()
     val pixel=image.getPixel(image.width/5,(image.height-120*density).toInt());image.recycle()
     android.graphics.Color.blue(pixel)>android.graphics.Color.red(pixel)+40
    }
    val image=instrumentation.uiAutomation.takeScreenshot()
    tap(image.width/2f,image.height-65*density);image.recycle()
    waitFor("Onboarding did not finish") {try{org.json.JSONObject(session.readText()).getJSONObject("settings").optBoolean("onboardingDone")}catch(_:Exception){false}}
   }
   phase("navigate")
   NativeBridge.openUrl("http://127.0.0.1:${server.localPort}/")
   phase("find page")
   var web:ArbiterPageView?=null
   waitFor("Real browser page did not load"){web=ui{find(MainActivity.current!!.window.decorView)};ui{web?.title=="Pill scroll regression"&&web!!.overlay.width()>0}}
   phase("page ready")
   val page=web!!;Thread.sleep(700)
   val before=rect(page)
   val initial=instrumentation.uiAutomation.takeScreenshot()
   val sampleX=before.centerX().toInt();val sampleY=(before.top+before.height()*.12f).toInt()
   val initialPixel=initial.getPixel(sampleX,sampleY);initial.recycle()
   val down=SystemClock.uptimeMillis();val centerX=before.centerX()
   touch(MotionEvent.ACTION_DOWN,centerX,before.top-250,down)
   for(index in 1..10){Thread.sleep(25);touch(MotionEvent.ACTION_MOVE,centerX,before.top-250-index*45,down)}
   touch(MotionEvent.ACTION_UP,centerX,before.top-700,down)
   waitFor("Injected swipe did not scroll the real WebView"){ui{page.scrollY>200}}
   Thread.sleep(500)
   val after=rect(page)
   assertEquals("Pill viewport bounds moved after scrolling",before,after)
   val scrolled=instrumentation.uiAutomation.takeScreenshot();val actualPixel=scrolled.getPixel(sampleX,sampleY);scrolled.recycle()
   // Glass is allowed to pick up a newly captured backdrop. It must still
   // reveal Qt's tinted pill rather than the fixture's opaque magenta content.
   val pageColor=android.graphics.Color.rgb(204,0,136)
   fun distance(a:Int,b:Int)=listOf(0,8,16).sumOf{abs(((a shr it)and 255)-((b shr it)and 255))}
   assertTrue("Pill did not render before scrolling",distance(initialPixel,pageColor)>40)
   assertTrue("Web content covered the fixed pill after scrolling: ${Integer.toHexString(actualPixel)}",distance(actualPixel,pageColor)>40)
   ui {
    val preview=android.graphics.Bitmap.createBitmap(page.width,page.height,android.graphics.Bitmap.Config.ARGB_8888)
    page.drawPreview(android.graphics.Canvas(preview))
    val pixel=preview.getPixel(40,40);preview.recycle()
    assertTrue("Scrolled tab preview did not capture the visible viewport",android.graphics.Color.red(pixel)>180&&android.graphics.Color.green(pixel)<30&&android.graphics.Color.blue(pixel) in 100..165)
   }
   val density=context.resources.displayMetrics.density
   tap(after.right-73*density,after.centerY())
   waitFor("Pill's Tabs button did not open the real QML panel"){ui{!page.isShown}}
   // System Back returns to the same page; no profile/session data is cleared.
   instrumentation.sendKeyDownUpSync(android.view.KeyEvent.KEYCODE_BACK)
   waitFor("Tabs dismissal did not restore the page"){ui{page.isShown}}
  }finally{server.close();worker.join(1000)}
 }
}
