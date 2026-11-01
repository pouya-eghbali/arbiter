package com.arbiter.mobile

import android.Manifest.permission.*
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.content.pm.PackageManager.PERMISSION_DENIED
import android.net.Uri
import android.webkit.PermissionRequest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Test
import org.junit.Assert.*
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebPermissionsTest {
 private class Request(private val names:Array<String>):PermissionRequest(){
  var approved:Array<String>?=null
  var denied=false
  override fun getOrigin()=Uri.parse("https://example.com")
  override fun getResources()=names
  override fun grant(resources:Array<String>){approved=resources}
  override fun deny(){denied=true}
 }
 private class Fixture {
  var url="https://example.com/page"
  var answer:((Boolean)->Unit)?=null
  var code=0
  var names=emptyArray<String>()
  val allowed=mutableSetOf<String>()
  val controller=WebPermissions(
   current={page,origin->page=="tab"&&WebPermissions.sameOrigin(url,origin)},
   granted={it in allowed},request={n,c->names=n;code=c},
   prompt={_,_,reply->answer=reply;{}})
  fun result(vararg permissions:Pair<String,Int>){
   for((name,result)in permissions)if(result==PERMISSION_GRANTED)allowed.add(name)
   controller.result(code,permissions.map{it.first}.toTypedArray(),permissions.map{it.second}.toIntArray())
  }
 }
 @Test fun mixedResourcesOnlyGrantApprovedCamera(){
  val f=Fixture();val request=Request(arrayOf(PermissionRequest.RESOURCE_VIDEO_CAPTURE,"future.webview.resource"))
  f.controller.media("tab",request);f.answer!!(true)
  assertArrayEquals(arrayOf(CAMERA),f.names)
  f.result(CAMERA to PERMISSION_GRANTED)
  assertArrayEquals(arrayOf(PermissionRequest.RESOURCE_VIDEO_CAPTURE),request.approved)
 }
 @Test fun partialDenialStillAllowsExplicitlyApprovedMicrophone(){
  val f=Fixture();val request=Request(arrayOf(PermissionRequest.RESOURCE_VIDEO_CAPTURE,PermissionRequest.RESOURCE_AUDIO_CAPTURE))
  f.controller.media("tab",request);f.answer!!(true)
  f.result(CAMERA to PERMISSION_DENIED,RECORD_AUDIO to PERMISSION_GRANTED)
  assertArrayEquals(arrayOf(PermissionRequest.RESOURCE_AUDIO_CAPTURE),request.approved)
 }
 @Test fun unsupportedAndDeniedRequestsDoNotGetAccess(){
  val f=Fixture();val unsupported=Request(arrayOf("unknown"));f.controller.media("tab",unsupported)
  assertTrue(unsupported.denied);assertNull(f.answer)
  val request=Request(arrayOf(PermissionRequest.RESOURCE_VIDEO_CAPTURE));f.controller.media("tab",request);f.answer!!(false)
  assertTrue(request.denied);assertEquals(0,f.code)
 }
 @Test fun overlappingRequestCannotReplaceOriginal(){
  val f=Fixture();val first=Request(arrayOf(PermissionRequest.RESOURCE_VIDEO_CAPTURE));val other=Request(arrayOf(PermissionRequest.RESOURCE_AUDIO_CAPTURE))
  f.controller.media("tab",first);f.controller.media("tab",other)
  assertTrue(other.denied);f.answer!!(true);f.result(CAMERA to PERMISSION_GRANTED)
  assertNotNull(first.approved);assertNull(other.approved)
 }
 @Test fun cancelledRequestAndLateResultCannotApproveNewRequest(){
  val f=Fixture();val first=Request(arrayOf(PermissionRequest.RESOURCE_VIDEO_CAPTURE))
  f.controller.media("tab",first);f.answer!!(true);val oldCode=f.code;f.controller.cancel(first)
  val next=Request(arrayOf(PermissionRequest.RESOURCE_VIDEO_CAPTURE));f.controller.media("tab",next);f.answer!!(true)
  f.allowed.add(CAMERA);f.controller.result(oldCode,arrayOf(CAMERA),intArrayOf(PERMISSION_GRANTED))
  assertNull(first.approved);assertFalse(first.denied);assertNull(next.approved)
  f.result(CAMERA to PERMISSION_GRANTED);assertNotNull(next.approved)
 }
 @Test fun navigationInvalidatesConsentEvenBeforeOsResult(){
  val f=Fixture();val request=Request(arrayOf(PermissionRequest.RESOURCE_VIDEO_CAPTURE))
  f.controller.media("tab",request);f.answer!!(true);f.url="https://other.example/"
  f.result(CAMERA to PERMISSION_GRANTED);assertTrue(request.denied)
 }
 @Test fun approximateLocationIsEnoughAndNotRemembered(){
  val f=Fixture();var granted=false;var remembered=true
  f.controller.location("tab","https://example.com"){_,allow,remember->granted=allow;remembered=remember}
  f.answer!!(true);assertEquals(setOf(ACCESS_COARSE_LOCATION,ACCESS_FINE_LOCATION),f.names.toSet())
  f.result(ACCESS_COARSE_LOCATION to PERMISSION_GRANTED,ACCESS_FINE_LOCATION to PERMISSION_DENIED)
  assertTrue(granted);assertFalse(remembered)
 }
 @Test fun cancelledLocationDoesNotLeakToNextTab(){
  val f=Fixture();var granted=true
  f.controller.location("tab","https://example.com"){_,allow,_->granted=allow}
  f.answer!!(true);f.controller.cancel("tab");f.result(ACCESS_FINE_LOCATION to PERMISSION_GRANTED)
  assertFalse(granted)
 }
}
