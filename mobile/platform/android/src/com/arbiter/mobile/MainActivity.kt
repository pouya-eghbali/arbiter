package com.arbiter.mobile
import android.content.Intent
import org.qtproject.qt.android.bindings.QtActivity
class MainActivity:QtActivity(){
 companion object {@Volatile var current:MainActivity?=null}
 override fun onCreate(state:android.os.Bundle?){current=this;super.onCreate(state)}
 override fun onNewIntent(intent:Intent){super.onNewIntent(intent);setIntent(intent);intent.dataString?.let{try{NativeBridge.openUrl(it)}catch(_:UnsatisfiedLinkError){}}}
 override fun onActivityResult(request:Int,result:Int,data:Intent?){super.onActivityResult(request,result,data);NativeBridge.activityResult(request,result,data)}
 override fun onRequestPermissionsResult(request:Int,permissions:Array<out String>,results:IntArray){super.onRequestPermissionsResult(request,permissions,results);NativeBridge.permissionsResult(request,results)}
}
