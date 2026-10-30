package com.arbiter.mobile

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.StandardMessageCodec
import io.flutter.plugin.platform.PlatformView
import io.flutter.plugin.platform.PlatformViewFactory
import org.mozilla.geckoview.*
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.UUID
import java.util.concurrent.Executors

class MobileBrowserBridge(val activity: MainActivity, messenger: BinaryMessenger) : PlatformViewFactory(StandardMessageCodec.INSTANCE) {
    val channel = MethodChannel(messenger, "com.arbiter.mobile/browser")
    val runtime: GeckoRuntime = GeckoRuntime.create(activity.applicationContext,
        GeckoRuntimeSettings.Builder().aboutConfigEnabled(false).build())
    private val pages = mutableMapOf<String, BrowserPage>()
    private val worker = Executors.newCachedThreadPool()
    private val ui = Handler(Looper.getMainLooper())
    val credentials = CredentialBridge(this)
    fun pagesSnapshot() = pages.values.toList()
    private var privateVisible = false
    private var secureScreen = false
    private var androidPermission: GeckoSession.PermissionDelegate.Callback? = null
    private var filePrompt: GeckoSession.PromptDelegate.FilePrompt? = null
    private var fileReply: GeckoResult<GeckoSession.PromptDelegate.PromptResponse>? = null
    init { channel.setMethodCallHandler(::command) }
    override fun create(context: Context, viewId: Int, args: Any?): PlatformView {
        val data = args as? Map<*, *> ?: emptyMap<Any, Any>()
        val id = data["id"] as? String ?: UUID.randomUUID().toString()
        val page = pages.getOrPut(id) {
            BrowserPage(this, id, data["private"] == true, data["context"] as? String).also {
                (data["url"] as? String)?.let(it::load)
            }
        }
        return object : PlatformView {
            val view = GeckoView(context).apply {
                setBackgroundColor(android.graphics.Color.WHITE)
                page.view?.releaseSession()
                page.view = this
                setSession(page.session)
                page.session.setActive(true)
                setOnTouchListener { _, event ->
                    if (event.actionMasked == MotionEvent.ACTION_DOWN) this@MobileBrowserBridge.event(page.id, "activity")
                    false
                }
            }
            override fun getView(): View = view
            override fun dispose() { view.releaseSession(); if (page.view === view) page.view = null; page.session.setActive(false) }
        }
    }
    fun event(id: String, type: String, data: Map<String, Any?> = emptyMap()) {
        ui.post { channel.invokeMethod("event", mapOf("id" to id, "type" to type) + data) }
    }
    fun ask(type: String, data: Map<String, Any?>, reply: (Any?) -> Unit) {
        ui.post { channel.invokeMethod(type, data, object : MethodChannel.Result {
            override fun success(result: Any?) = reply(result)
            override fun error(code: String, message: String?, details: Any?) = reply(null)
            override fun notImplemented() = reply(null)
        }) }
    }
    private fun command(call: MethodCall, result: MethodChannel.Result) {
        val page = pages[call.argument<String>("id")]
        when (call.method) {
            "credentialIndex" -> {
                val entries=call.argument<List<Map<String,Any?>>>("entries") ?: emptyList()
                PlatformCredentialVault.writeIndex(activity,entries);result.success(null)
            }
            "enableCredentialProvider" -> {
                if (android.os.Build.VERSION.SDK_INT>=34) {
                    try {
                        val manager=androidx.credentials.CredentialManager.create(activity)
                        manager.createSettingsPendingIntent().send();result.success(true)
                    } catch (_: Exception) { result.success(false) }
                } else result.success(false)
            }
            "requestDefaultBrowser" -> {
                if (android.os.Build.VERSION.SDK_INT>=29) {
                    val roles=activity.getSystemService(android.app.role.RoleManager::class.java)
                    if (!roles.isRoleHeld(android.app.role.RoleManager.ROLE_BROWSER)) activity.startActivity(roles.createRequestRoleIntent(android.app.role.RoleManager.ROLE_BROWSER))
                } else activity.startActivity(Intent(android.provider.Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS))
                result.success(null)
            }
            "ready" -> result.success(activity.intent.dataString)
            "privateVisible", "secureScreen" -> {
                if (call.method == "privateVisible") privateVisible = call.argument<Boolean>("value") == true
                else secureScreen = call.argument<Boolean>("value") == true
                if (privateVisible || secureScreen) activity.window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
                else activity.window.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
                result.success(null)
            }
            "close" -> { page?.let { it.session.close(); pages.remove(it.id) }; result.success(null) }
            "pausePrivate" -> { pages.values.filter { it.isPrivate }.forEach { it.media?.pause() }; result.success(null) }
            "load" -> { page?.load(call.argument<String>("url") ?: ""); result.success(null) }
            "back" -> { page?.session?.goBack(); result.success(null) }
            "forward" -> { page?.session?.goForward(); result.success(null) }
            "reload" -> { page?.session?.reload(); result.success(null) }
            "stop" -> { page?.session?.stop(); result.success(null) }
            "find" -> {
                val query = call.argument<String>("query") ?: ""
                if (query.isEmpty()) page?.session?.finder?.clear() else page?.session?.finder?.find(query, 0)
                result.success(null)
            }
            "fill" -> credentials.fill(page, call.arguments as? Map<*, *> ?: emptyMap<Any, Any>(), result)
            "snapshot" -> {
                val view = page?.view
                if (view == null || view.width <= 0 || view.height <= 0) { result.success(null); return }
                view.capturePixels().accept({ bitmap ->
                    if (bitmap == null) { result.success(null); return@accept }
                    val width = minOf(bitmap.width, 360)
                    val scaled = Bitmap.createScaledBitmap(bitmap, width, (bitmap.height.toLong() * width / bitmap.width).toInt().coerceAtLeast(1), true)
                    val bytes = ByteArrayOutputStream()
                    scaled.compress(Bitmap.CompressFormat.JPEG, 65, bytes)
                    result.success(bytes.toByteArray())
                }, { result.success(null) })
            }
            else -> result.notImplemented()
        }
    }
    fun requestAndroidPermissions(permissions: Array<String>?, callback: GeckoSession.PermissionDelegate.Callback) {
        val needed = permissions.orEmpty().filter { ContextCompat.checkSelfPermission(activity, it) != PackageManager.PERMISSION_GRANTED }
        if (needed.isEmpty()) { callback.grant(); return }
        if (androidPermission != null) { callback.reject(); return }
        androidPermission = callback
        ActivityCompat.requestPermissions(activity, needed.toTypedArray(), 7801)
    }
    fun permissionResult(requestCode: Int, allowed: Boolean) {
        if (requestCode != 7801) return
        androidPermission?.let { if (allowed) it.grant() else it.reject() }
        androidPermission = null
    }
    fun chooseFiles(prompt: GeckoSession.PromptDelegate.FilePrompt): GeckoResult<GeckoSession.PromptDelegate.PromptResponse> {
        if (filePrompt != null) return GeckoResult.fromValue(prompt.dismiss())
        filePrompt = prompt
        val reply = GeckoResult<GeckoSession.PromptDelegate.PromptResponse>(); fileReply = reply
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE); type = "*/*"
            putExtra(Intent.EXTRA_ALLOW_MULTIPLE, prompt.type == GeckoSession.PromptDelegate.FilePrompt.Type.MULTIPLE)
            prompt.mimeTypes?.filter { it.contains('/') }?.takeIf { it.isNotEmpty() }?.let { putExtra(Intent.EXTRA_MIME_TYPES, it.toTypedArray()) }
        }
        activity.startActivityForResult(intent, 7802)
        return reply
    }
    fun fileResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode != 7802) return
        val prompt = filePrompt ?: return
        val uris = if (data?.clipData != null) (0 until data.clipData!!.itemCount).map { data.clipData!!.getItemAt(it).uri }
            else listOfNotNull(data?.data)
        fileReply?.complete(if (resultCode == Activity.RESULT_OK && uris.isNotEmpty()) prompt.confirm(activity, uris.toTypedArray()) else prompt.dismiss())
        filePrompt = null; fileReply = null
    }
    fun download(page: BrowserPage, response: WebResponse) {
        val disposition = response.headers.entries.firstOrNull { it.key.equals("content-disposition", true) }?.value ?: ""
        val filename = Regex("filename\\*?=(?:UTF-8''|\\\")?([^\\\";]+)", RegexOption.IGNORE_CASE).find(disposition)?.groupValues?.get(1)
            ?: Uri.parse(response.uri).lastPathSegment ?: "download"
        val safeName = filename.substringAfterLast('/').substringAfterLast('\\').replace(Regex("[\\x00-\\x1f:*?\"<>|]"), "_").take(160).takeIf { it.isNotBlank() && it != "." && it != ".." } ?: "download"
        ask("download", mapOf("id" to page.id, "name" to safeName)) { allowed ->
            if (allowed != true) { worker.execute { response.body?.close() }; return@ask }
            val id = UUID.randomUUID().toString()
            val file = File(activity.filesDir, "Downloads/$id/$safeName")
            val total = response.headers.entries.firstOrNull { it.key.equals("content-length", true) }?.value?.toLongOrNull() ?: -1L
            worker.execute {
                var received = 0L; var last = 0L
                fun update(complete: Boolean = false, error: String? = null) = event(page.id, "download", mapOf(
                    "downloadId" to id, "name" to safeName, "path" to file.absolutePath,
                    "private" to page.isPrivate, "total" to total, "received" to received, "complete" to complete, "error" to error))
                try {
                    file.parentFile!!.mkdirs()
                    val body = response.body ?: throw IllegalStateException("No download data received")
                    update()
                    body.use { input -> file.outputStream().use { output ->
                        val buffer = ByteArray(32768)
                        while (true) {
                            val n = input.read(buffer); if (n < 0) break
                            output.write(buffer, 0, n); received += n
                            val now = android.os.SystemClock.elapsedRealtime()
                            if (now - last > 500) { last = now; update() }
                        }
                    } }
                    if (total >= 0 && received != total) throw IllegalStateException("The download ended before the file was complete")
                    update(complete = true)
                } catch (error: Exception) { file.delete(); update(error = error.message ?: "Download failed") }
            }
        }
    }
    fun dispose() { pages.values.forEach { it.session.close() }; pages.clear(); runtime.shutdown(); worker.shutdown(); channel.setMethodCallHandler(null) }
    companion object {
        fun webURL(value: String): Boolean = try {
            val uri = Uri.parse(value)
            uri.scheme in listOf("http", "https") && !uri.host.isNullOrEmpty() && uri.userInfo == null
        } catch (_: Exception) { false }
        fun origin(value: String): String = try { val uri = Uri.parse(value); "${uri.scheme}://${uri.host}${if (uri.port >= 0) ":${uri.port}" else ""}" } catch (_: Exception) { "this website" }
    }
}

class BrowserPage(val owner: MobileBrowserBridge, val id: String, val isPrivate: Boolean, context: String?) {
    val session = GeckoSession(GeckoSessionSettings.Builder().usePrivateMode(isPrivate).contextId(context).useTrackingProtection(true).build())
    var view: GeckoView? = null
    var media: MediaSession? = null
    private var url = ""
    val committedUrl: String get() = url
    private var pendingUrl: String? = null
    private var back = false
    private var forward = false
    init {
        session.navigationDelegate = object : GeckoSession.NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, value: String?, perms: MutableList<GeckoSession.PermissionDelegate.ContentPermission>, hasUserGesture: Boolean) {
                url = value ?: ""; location()
            }
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) { back = canGoBack; location() }
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) { forward = canGoForward; location() }
            override fun onLoadRequest(session: GeckoSession, request: GeckoSession.NavigationDelegate.LoadRequest): GeckoResult<AllowOrDeny> {
                if (MobileBrowserBridge.webURL(request.uri) || request.uri == "about:blank") {
                    if (request.target == GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW) {
                        if (request.hasUserGesture) owner.event(id, "newTab", mapOf("url" to request.uri))
                        return GeckoResult.fromValue(AllowOrDeny.DENY)
                    }
                    return GeckoResult.fromValue(AllowOrDeny.ALLOW)
                }
                if (request.hasUserGesture) owner.ask("external", mapOf("id" to id, "url" to request.uri)) { }
                return GeckoResult.fromValue(AllowOrDeny.DENY)
            }
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError): GeckoResult<String>? {
                owner.event(id, "error", mapOf("message" to "This page could not be loaded. Check the address and your connection.")); return null
            }
        }
        session.progressDelegate = object : GeckoSession.ProgressDelegate {
            override fun onProgressChange(session: GeckoSession, progress: Int) { owner.event(id, "progress", mapOf("progress" to progress / 100.0)) }
            override fun onPageStop(session: GeckoSession, success: Boolean) { location(); owner.event(id, "finish", mapOf("success" to success)) }
        }
        session.contentDelegate = object : GeckoSession.ContentDelegate {
            override fun onTitleChange(session: GeckoSession, title: String?) { owner.event(id, "title", mapOf("title" to (title ?: ""))) }
            override fun onExternalResponse(session: GeckoSession, response: WebResponse) { owner.download(this@BrowserPage, response) }
            override fun onCrash(session: GeckoSession) { owner.event(id, "error", mapOf("message" to "This page stopped responding. Reload to try again.")) }
        }
        session.mediaSessionDelegate = object : MediaSession.Delegate {
            override fun onActivated(session: GeckoSession, mediaSession: MediaSession) { media = mediaSession }
        }
        session.permissionDelegate = object : GeckoSession.PermissionDelegate {
            override fun onAndroidPermissionsRequest(session: GeckoSession, permissions: Array<out String>?, callback: GeckoSession.PermissionDelegate.Callback) {
                owner.requestAndroidPermissions(permissions?.map { it }?.toTypedArray(), callback)
            }
            override fun onContentPermissionRequest(session: GeckoSession, permission: GeckoSession.PermissionDelegate.ContentPermission): GeckoResult<Int> {
                val reply = GeckoResult<Int>()
                if (permission.permission != GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION) return GeckoResult.fromValue(GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY)
                owner.ask("permission", mapOf("id" to id, "kind" to "location", "origin" to MobileBrowserBridge.origin(permission.uri))) {
                    reply.complete(if (it == true) GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW else GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY)
                }
                return reply
            }
            override fun onMediaPermissionRequest(session: GeckoSession, uri: String, video: Array<out GeckoSession.PermissionDelegate.MediaSource>?, audio: Array<out GeckoSession.PermissionDelegate.MediaSource>?, callback: GeckoSession.PermissionDelegate.MediaCallback) {
                val kind = if (!video.isNullOrEmpty() && !audio.isNullOrEmpty()) "camera and microphone" else if (!video.isNullOrEmpty()) "camera" else "microphone"
                owner.ask("permission", mapOf("id" to id, "kind" to kind, "origin" to MobileBrowserBridge.origin(uri))) {
                    if (it == true) callback.grant(video?.firstOrNull(), audio?.firstOrNull()) else callback.reject()
                }
            }
        }
        session.promptDelegate = object : GeckoSession.PromptDelegate {
            override fun onAlertPrompt(session: GeckoSession, prompt: GeckoSession.PromptDelegate.AlertPrompt): GeckoResult<GeckoSession.PromptDelegate.PromptResponse> {
                val reply = GeckoResult<GeckoSession.PromptDelegate.PromptResponse>()
                owner.ask("alert", mapOf("id" to id, "message" to (prompt.message ?: ""))) { reply.complete(prompt.dismiss()) }; return reply
            }
            override fun onButtonPrompt(session: GeckoSession, prompt: GeckoSession.PromptDelegate.ButtonPrompt): GeckoResult<GeckoSession.PromptDelegate.PromptResponse> {
                val reply = GeckoResult<GeckoSession.PromptDelegate.PromptResponse>()
                owner.ask("confirm", mapOf("id" to id, "message" to (prompt.message ?: ""))) { reply.complete(prompt.confirm(if (it == true) GeckoSession.PromptDelegate.ButtonPrompt.Type.POSITIVE else GeckoSession.PromptDelegate.ButtonPrompt.Type.NEGATIVE)) }; return reply
            }
            override fun onTextPrompt(session: GeckoSession, prompt: GeckoSession.PromptDelegate.TextPrompt): GeckoResult<GeckoSession.PromptDelegate.PromptResponse> {
                val reply = GeckoResult<GeckoSession.PromptDelegate.PromptResponse>()
                owner.ask("prompt", mapOf("id" to id, "title" to (prompt.message ?: ""), "value" to (prompt.defaultValue ?: ""))) {
                    reply.complete(if (it is String) prompt.confirm(it) else prompt.dismiss())
                }; return reply
            }
            override fun onFilePrompt(session: GeckoSession, prompt: GeckoSession.PromptDelegate.FilePrompt) = owner.chooseFiles(prompt)
        }
        session.open(owner.runtime)
        owner.credentials.attach(this)
    }
    private fun location() = owner.event(id, "location", mapOf("url" to url, "back" to back, "forward" to forward))
    fun startPending() { pendingUrl?.let { pendingUrl = null; load(it) } }
    fun load(uri: String) {
        if (!MobileBrowserBridge.webURL(uri)) return
        if (owner.credentials.ready) session.loadUri(uri) else pendingUrl = uri
    }
}
