package com.arbiter.mobile
import android.app.job.JobScheduler
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Test
import org.junit.Assert.*
import org.junit.runner.RunWith
import java.io.File
import org.json.JSONObject
@RunWith(AndroidJUnit4::class)
class BackgroundSyncTest {
  @Test fun headlessJobCompletesWithoutOpeningUserCredentials() {
    val instrumentation=InstrumentationRegistry.getInstrumentation()
    val context=instrumentation.targetContext
    val scheduler=context.getSystemService(JobScheduler::class.java)
    val previous=scheduler.getPendingJob(BackgroundSync.ID)
    val started=System.currentTimeMillis()
    try {
      BackgroundSync.configure(context,true,smoke=true)
      instrumentation.uiAutomation.executeShellCommand("cmd jobscheduler run -f ${context.packageName} ${BackgroundSync.ID}").use {it.close()}
      val file=File(context.filesDir,"background-last-run.json")
      val deadline=System.currentTimeMillis()+60000
      var status:JSONObject?=null
      while(System.currentTimeMillis()<deadline) {
        status=try{JSONObject(file.readText())}catch(_:Exception){null}
        if((status?.optLong("time") ?: 0)>=started)break
        Thread.sleep(200)
      }
      assertTrue("The headless job did not complete",(status?.optLong("time") ?: 0)>=started)
      assertTrue("The headless job reported failure",status?.optBoolean("success")==true)
    } finally {scheduler.cancel(BackgroundSync.ID);if(previous!=null)scheduler.schedule(previous)}
  }
}
