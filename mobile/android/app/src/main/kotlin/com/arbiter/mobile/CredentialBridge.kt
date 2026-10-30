package com.arbiter.mobile

import android.os.Handler
import android.os.Looper
import org.json.JSONObject
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.WebExtension
import io.flutter.plugin.common.MethodChannel
import java.util.UUID

// Native-only, first-party extension. Both the committed top-level URL and the
// sending frame are checked before any credential enters the content process.
class CredentialBridge(private val owner: MobileBrowserBridge) {
    private var extension: WebExtension? = null
    var ready = false; private set
    private val ui = Handler(Looper.getMainLooper())
    private val ports = mutableMapOf<String, WebExtension.Port>()
    private val pending = mutableMapOf<String, Pair<String, MethodChannel.Result>>()
    init {
        owner.runtime.webExtensionController.ensureBuiltIn("resource://android/assets/arbiter-credentials/", "credentials@arbiter.internal").accept({ value ->
            if (value == null) { finish(null); return@accept }
            owner.runtime.webExtensionController.setAllowedInPrivateBrowsing(value, true).accept({ finish(it) }, { finish(null) })
        }, { finish(null) })
        ui.postDelayed({ if (!ready) finish(null) }, 10000)
    }
    private fun finish(value: WebExtension?) {
        if (ready) return
        extension = value; ready = true
        owner.pagesSnapshot().forEach { attach(it); it.startPending() }
    }
    fun attach(page: BrowserPage) {
        val ext = extension ?: return
        page.session.webExtensionController.setMessageDelegate(ext, object : WebExtension.MessageDelegate {
            override fun onMessage(nativeApp:String,message:Any,sender:WebExtension.MessageSender):GeckoResult<Any>? {
                val data=message as? JSONObject ?: return null
                if(!owner.savePasswords || page.isPrivate || data.optString("type")!="loginSubmitted" || !sender.isTopLevel || sender.session!==page.session || !MobileBrowserBridge.webURL(sender.url))return null
                if(data.optString("origin")!=MobileBrowserBridge.origin(sender.url))return null
                owner.event(page.id,"loginSubmitted",mapOf("origin" to data.optString("origin"),"username" to data.optString("username").take(512),"password" to data.optString("password").take(4096)))
                return GeckoResult.fromValue(null)
            }
            override fun onConnect(port: WebExtension.Port) {
                if (!port.sender.isTopLevel || port.sender.session !== page.session || !MobileBrowserBridge.webURL(port.sender.url)) {
                    port.disconnect(); return
                }
                ports[page.id] = port
                preferences(page,port)
                port.setDelegate(object : WebExtension.PortDelegate {
                    override fun onDisconnect(port: WebExtension.Port) {
                        if (ports[page.id] === port) ports.remove(page.id)
                        pending.filterValues { it.first == page.id }.keys.toList().forEach { pending.remove(it)?.second?.success(false) }
                    }
                    override fun onPortMessage(message: Any, port: WebExtension.Port) {
                        val data = message as? JSONObject ?: return
                        if (ports[page.id] !== port || MobileBrowserBridge.origin(page.committedUrl)!=MobileBrowserBridge.origin(port.sender.url)) return
                        if(data.optString("type") in listOf("credentialFocus","loginSubmitted","loginSucceeded","activityState")) {
                            if(data.optString("type")=="loginSubmitted" && (!owner.savePasswords || page.isPrivate))return
                            if(data.optString("origin").isNotEmpty() && data.optString("origin")!=MobileBrowserBridge.origin(page.committedUrl)) return
                            val values=mutableMapOf<String,Any?>()
                            for(key in listOf("origin","username","password","newPassword","hasPassword","audio","dirty")) if(data.has(key)) values[key]=data.get(key)
                            owner.event(page.id,data.getString("type"),values);return
                        }
                        if(data.optString("type")=="preferences"){preferences(page,port);return}
                        if (data.optString("type") != "filled") return
                        val id = data.optString("requestId")
                        if (pending[id]?.first != page.id || ports[page.id] !== port) return
                        pending.remove(id)?.second?.success(data.optBoolean("filled"))
                    }
                })
            }
        }, "arbiterCredentials")
    }
    private fun preferences(page:BrowserPage,port:WebExtension.Port) {
        port.postMessage(JSONObject(mapOf("action" to "preferences","savePasswords" to (owner.savePasswords && !page.isPrivate),"suggestPasswords" to owner.suggestPasswords)))
    }
    fun updatePreferences() {owner.pagesSnapshot().forEach {page->ports[page.id]?.let {preferences(page,it)}}}
    fun fill(page: BrowserPage?, data: Map<*, *>, result: MethodChannel.Result) {
        val origin = data["origin"] as? String
        val port = page?.let { ports[it.id] }
        if (page == null || origin == null || port == null || !port.sender.isTopLevel ||
            port.sender.session !== page.session || !MobileBrowserBridge.webURL(page.committedUrl) ||
            MobileBrowserBridge.origin(page.committedUrl) != origin || MobileBrowserBridge.origin(port.sender.url) != origin) {
            result.success(false); return
        }
        val id = UUID.randomUUID().toString()
        pending[id] = Pair(page.id, result)
        port.postMessage(JSONObject(mapOf("action" to (if(data["inspect"]==true) "inspect" else "fill"), "requestId" to id, "origin" to origin,
            "generate" to (data["generate"]==true), "username" to (data["username"] as? String ?: ""), "password" to (data["password"] as? String ?: ""))))
        ui.postDelayed({ pending.remove(id)?.second?.success(false) }, 3000)
    }
    fun close(id: String) { ports.remove(id)?.disconnect() }
}
