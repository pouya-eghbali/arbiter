package com.arbiter.mobile

import android.app.job.JobInfo
import android.app.job.JobParameters
import android.app.job.JobScheduler
import android.app.job.JobService
import android.content.ComponentName
import android.content.Context
import android.os.Handler
import android.os.Looper
import io.flutter.FlutterInjector
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.embedding.engine.dart.DartExecutor
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugins.GeneratedPluginRegistrant

object BackgroundSync {
    const val ID=71024
    fun configure(context:Context,enabled:Boolean,smoke:Boolean=false) {
        val scheduler=context.getSystemService(JobScheduler::class.java)
        if(!enabled){scheduler.cancel(ID);return}
        val job=JobInfo.Builder(ID,ComponentName(context,SyncJobService::class.java))
            .setRequiredNetworkType(JobInfo.NETWORK_TYPE_UNMETERED).setPeriodic(15*60*1000L)
            .setPersisted(!smoke).setExtras(android.os.PersistableBundle().apply{putBoolean("smoke",smoke)}).build()
        check(scheduler.schedule(job)==JobScheduler.RESULT_SUCCESS)
    }
}
class SyncJobService:JobService() {
    private var engine:FlutterEngine?=null
    private var parameters:JobParameters?=null
    private var completion:MethodChannel?=null
    private val ui=Handler(Looper.getMainLooper())
    private val timeout=Runnable {stop()}
    override fun onStartJob(params:JobParameters):Boolean {
        if(engine!=null)return false
        parameters=params
        try {
            val loader=FlutterInjector.instance().flutterLoader()
            loader.startInitialization(applicationContext);loader.ensureInitializationComplete(applicationContext,null)
            val worker=FlutterEngine(applicationContext,null,false);engine=worker
            GeneratedPluginRegistrant.registerWith(worker)
            MethodChannel(worker.dartExecutor.binaryMessenger,"com.arbiter.mobile/browser").setMethodCallHandler {call,result ->
                if(call.method=="credentialIndex") {
                    PlatformCredentialVault.writeIndex(this,call.argument<List<Map<String,Any?>>>("entries") ?: emptyList());result.success(null)
                } else result.notImplemented()
            }
            completion=MethodChannel(worker.dartExecutor.binaryMessenger,"com.arbiter.mobile/background")
            completion?.setMethodCallHandler {call,result ->
                if(call.method=="complete") {
                    val success=call.argument<Boolean>("success")==true
                    result.success(null);ui.post{finish(success)}
                }else result.notImplemented()
            }
            worker.dartExecutor.executeDartEntrypoint(DartExecutor.DartEntrypoint(loader.findAppBundlePath(),"package:arbiter_mobile/main.dart",if(params.extras.getBoolean("smoke")) "backgroundSyncSmokeMain" else "backgroundSyncMain"))
            ui.postDelayed(timeout,90000);return true
        } catch (_:Exception) {finish(false);return false}
    }
    private fun finish(completed:Boolean) {
        ui.removeCallbacks(timeout)
        val params=parameters;parameters=null
        completion?.setMethodCallHandler(null);completion=null
        engine?.destroy();engine=null
        java.io.File(filesDir,"background-last-run.json").writeText(org.json.JSONObject(mapOf("time" to System.currentTimeMillis(),"success" to completed)).toString())
        if(params!=null)jobFinished(params,false)
    }
    private fun stop() {
        ui.removeCallbacks(timeout)
        completion?.invokeMethod("cancel",null)
        // This service has its own :sync process. If a native plugin hangs,
        // terminating only that worker releases its OS file locks safely.
        ui.postDelayed({if(engine!=null)android.os.Process.killProcess(android.os.Process.myPid())},5000)
    }
    override fun onStopJob(params:JobParameters):Boolean {parameters=null;stop();return true}
}
