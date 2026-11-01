package com.arbiter.mobile

import android.Manifest.permission.*
import android.content.pm.PackageManager
import android.net.Uri
import android.webkit.GeolocationPermissions
import android.webkit.PermissionRequest

// UI-thread coordinator. Busy requests are denied rather than overwriting another
// tab's consent. Each OS request has its own code, so late results cannot approve
// a different website after navigation, cancellation or destruction.
internal class WebPermissions(
 private val current:(String,String)->Boolean,
 private val granted:(String)->Boolean,
 private val request:(Array<String>,Int)->Unit,
 private val prompt:(String,Boolean,(Boolean)->Unit)->()->Unit
) {
 private class Pending(val page:String,val origin:String,val media:PermissionRequest?,
  val geo:GeolocationPermissions.Callback?,val resources:List<String>) {
  var dismiss:(()->Unit)?=null
  var code=0
 }
 private var pending:Pending?=null
 private var nextCode=20000
 companion object {
  fun sameOrigin(a:String,b:String):Boolean {
   fun origin(value:String):Triple<String?,String?,Int> {
    val uri=Uri.parse(value)
    return Triple(uri.scheme?.lowercase(),uri.host?.lowercase(),
     if(uri.port!=-1)uri.port else if(uri.scheme=="https")443 else 80)
   }
   val left=origin(a);return left.first in listOf("http","https")&&left.second!=null&&left==origin(b)
  }
  private fun osPermission(resource:String)=when(resource){
   PermissionRequest.RESOURCE_VIDEO_CAPTURE->CAMERA
   PermissionRequest.RESOURCE_AUDIO_CAPTURE->RECORD_AUDIO
   else->null
  }
 }
 fun media(page:String,value:PermissionRequest) {
  val resources=value.resources.filter{osPermission(it)!=null}.distinct()
  if(resources.isEmpty()||pending!=null||!current(page,value.origin.toString())){value.deny();return}
  begin(Pending(page,value.origin.toString(),value,null,resources))
 }
 fun location(page:String,origin:String,callback:GeolocationPermissions.Callback) {
  if(pending!=null||!current(page,origin)){callback.invoke(origin,false,false);return}
  begin(Pending(page,origin,null,callback,emptyList()))
 }
 private fun begin(value:Pending) {
  pending=value
  value.dismiss=prompt(value.origin,value.geo!=null){allowed->
   if(pending!==value)return@prompt
   if(!allowed||!current(value.page,value.origin)){finish(value,emptySet());return@prompt}
   val names=if(value.geo!=null)listOf(ACCESS_COARSE_LOCATION,ACCESS_FINE_LOCATION)
    else value.resources.mapNotNull{osPermission(it)}
   if(names.all{granted(it)}){finish(value,names.toSet());return@prompt}
   // Do not recycle codes within a process; deny safely on exhaustion.
   if(nextCode>60000){finish(value,emptySet());return@prompt}
   value.code=nextCode++
   try{request(names.toTypedArray(),value.code)}catch(_:Exception){finish(value,emptySet())}
  }
 }
 fun result(code:Int,names:Array<out String>,results:IntArray) {
  val value=pending ?: return
  if(value.code==0||value.code!=code)return
  val approved=names.indices.filter{it<results.size&&results[it]==PackageManager.PERMISSION_GRANTED}
   .map{names[it]}.filter{granted(it)}.toSet()
  finish(value,approved)
 }
 private fun finish(value:Pending,approved:Set<String>,respond:Boolean=true) {
  if(pending!==value)return
  pending=null
  value.dismiss?.invoke()
  if(!respond)return // WebView already canceled this request; only dismiss its UI.
  val valid=current(value.page,value.origin)
  if(value.geo!=null)value.geo.invoke(value.origin,valid&&(ACCESS_COARSE_LOCATION in approved||ACCESS_FINE_LOCATION in approved),false)
  else {
   val resources=if(valid)value.resources.filter{osPermission(it) in approved}.toTypedArray()else emptyArray()
   if(resources.isEmpty())value.media?.deny()else value.media?.grant(resources)
  }
 }
 fun cancel(page:String){pending?.takeIf{it.page==page}?.let{finish(it,emptySet())}}
 fun cancel(value:PermissionRequest){pending?.takeIf{it.media===value}?.let{finish(it,emptySet(),false)}}
}
