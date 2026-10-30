package com.arbiter.mobile

import io.flutter.plugin.common.MethodChannel
import android.os.Handler
import android.os.Looper
import org.json.JSONObject
import org.mozilla.geckoview.WebExtension

class AdBlockBridge(private val owner: MobileBrowserBridge) {
    var ready=false; private set
    var working=false;private set
    private var revision=0
    private val pending=mutableMapOf<Int,MethodChannel.Result>()
    var enabled=true
    var disabledHosts: List<String> = emptyList()
    private var port: WebExtension.Port?=null
    private val ui=Handler(Looper.getMainLooper())
    init {
        owner.runtime.webExtensionController.ensureBuiltIn("resource://android/assets/arbiter-adblock/","adblock@arbiter.internal").accept({ ext ->
            if(ext==null) { finish(false);return@accept }
            ext.setMessageDelegate(object:WebExtension.MessageDelegate {
                override fun onConnect(p:WebExtension.Port) {
                    if(p.sender.session!=null) { p.disconnect();return }
                    port=p
                    p.setDelegate(object:WebExtension.PortDelegate {
                        override fun onPortMessage(message:Any,p:WebExtension.Port) {
                            val data=message as? JSONObject ?: return
                            if(data.optString("type")=="configure") configure()
                            if(data.optString("type")=="ready") {
                                finish(true)
                                val applied=data.optInt("revision")
                                pending.keys.filter{it<=applied}.toList().forEach{pending.remove(it)?.success(null)}
                            }
                        }
                        override fun onDisconnect(p:WebExtension.Port) { if(port===p) port=null }
                    })
                    configure()
                }
            },"arbiterAdblock")
            owner.runtime.webExtensionController.setAllowedInPrivateBrowsing(ext,true)
        },{finish(false)})
        ui.postDelayed({if(!ready) finish(false)},15000)
    }
    fun configure(result:MethodChannel.Result?=null) {
        val version=++revision
        if(result!=null) {
            pending[version]=result
            ui.postDelayed({pending.remove(version)?.error("adblock_unavailable","Ad blocking could not apply the setting.",null)},15000)
        }
        port?.postMessage(JSONObject(mapOf("action" to "configure","enabled" to enabled,"disabledHosts" to disabledHosts,"revision" to version)))
    }
    private fun finish(working:Boolean) {
        this.working=working
        if(ready) return
        ready=true
        owner.event("","adblockReady",mapOf("working" to working))
        owner.pagesSnapshot().forEach { it.startPending() }
    }
}
