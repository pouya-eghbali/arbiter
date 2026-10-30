package com.arbiter.mobile

import android.os.Handler
import android.os.Looper
import org.json.JSONObject
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
            override fun onConnect(port: WebExtension.Port) {
                if (!port.sender.isTopLevel || port.sender.session !== page.session || !MobileBrowserBridge.webURL(port.sender.url)) {
                    port.disconnect(); return
                }
                ports[page.id] = port
                port.setDelegate(object : WebExtension.PortDelegate {
                    override fun onDisconnect(port: WebExtension.Port) {
                        if (ports[page.id] === port) ports.remove(page.id)
                        pending.filterValues { it.first == page.id }.keys.toList().forEach { pending.remove(it)?.second?.success(false) }
                    }
                    override fun onPortMessage(message: Any, port: WebExtension.Port) {
                        val data = message as? JSONObject ?: return
                        if (data.optString("type") != "filled") return
                        val id = data.optString("requestId")
                        if (pending[id]?.first != page.id || ports[page.id] !== port) return
                        pending.remove(id)?.second?.success(data.optBoolean("filled"))
                    }
                })
            }
        }, "arbiterCredentials")
    }
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
        port.postMessage(JSONObject(mapOf("action" to "fill", "requestId" to id, "origin" to origin,
            "username" to (data["username"] as? String ?: ""), "password" to (data["password"] as? String ?: ""))))
        ui.postDelayed({ pending.remove(id)?.second?.success(false) }, 3000)
    }
    fun close(id: String) { ports.remove(id)?.disconnect() }
}
