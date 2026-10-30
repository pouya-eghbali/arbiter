import Flutter
import UIKit
import WebKit
import AuthenticationServices

final class MobileBrowserBridge: NSObject, FlutterPlatformViewFactory {
  let transactionOwner=UUID().uuidString
  var savePasswords=true
  var suggestPasswords=true
  let adblock=AdBlockLists()
  let channel: FlutterMethodChannel
  private var pages: [String: BrowserPage] = [:]
  private var privateStores: [String: WKWebsiteDataStore] = [:]
  private var downloads: [String: BrowserDownload] = [:]
  private var privacyCover: UIView?
  private var privateVisible = false
  private var secureScreen = false

  init(messenger: FlutterBinaryMessenger) {
    channel = FlutterMethodChannel(name: "com.arbiter.mobile/browser", binaryMessenger: messenger)
    super.init()
    channel.setMethodCallHandler { [weak self] call, result in self?.handle(call, result: result) }
    NotificationCenter.default.addObserver(self, selector: #selector(hidePrivateContent), name: UIApplication.willResignActiveNotification, object: nil)
    NotificationCenter.default.addObserver(self, selector: #selector(revealApp), name: UIApplication.didBecomeActiveNotification, object: nil)
  }
  func createArgsCodec() -> FlutterMessageCodec & NSObjectProtocol { FlutterStandardMessageCodec.sharedInstance() }
  func create(withFrame frame: CGRect, viewIdentifier viewId: Int64, arguments args: Any?) -> FlutterPlatformView {
    let data = args as? [String: Any] ?? [:]
    let id = data["id"] as? String ?? UUID().uuidString
    let page: BrowserPage
    if let existing = pages[id] { page = existing }
    else {
      let configuration = WKWebViewConfiguration()
      let isPrivate = data["private"] as? Bool ?? false
      let context = data["context"] as? String
      if isPrivate {
        let key = context ?? "private"
        let store = privateStores[key] ?? WKWebsiteDataStore.nonPersistent()
        privateStores[key] = store
        configuration.websiteDataStore = store
      } else if let context, let uuid = UUID(uuidString: context) {
        configuration.websiteDataStore = WKWebsiteDataStore(forIdentifier: uuid)
      } else { configuration.websiteDataStore = .default() }
      configuration.allowsInlineMediaPlayback = true
      configuration.mediaTypesRequiringUserActionForPlayback = [.audio, .video]
      configuration.preferences.javaScriptCanOpenWindowsAutomatically = false
      page = BrowserPage(id: id, configuration: configuration, owner: self, isPrivate: isPrivate)
      pages[id] = page
      if let url=data["url"] as? String {page.load(url)}
    }
    return HostedBrowserPage(frame: frame, page: page)
  }
  func event(_ id: String, _ type: String, _ values: [String: Any] = [:]) {
    var data = values; data["id"] = id; data["type"] = type
    channel.invokeMethod("event", arguments: data)
  }
  func ask(_ name: String, _ data: [String: Any], reply: @escaping (Any?) -> Void) {
    channel.invokeMethod(name, arguments: data, result: reply)
  }
  static func webURL(_ text: String) -> URL? {
    guard let url = URL(string: text), ["https", "http"].contains(url.scheme?.lowercased() ?? ""),
      url.host != nil, url.user == nil, url.password == nil else { return nil }
    return url
  }
  private func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    let data = call.arguments as? [String: Any] ?? [:]
    let page = pages[data["id"] as? String ?? ""]
    switch call.method {
    #if DEBUG
    case "backgroundSyncSmoke":BackgroundSync.shared.smoke(result)
    #endif
    case "acquireVaultLock":VaultTransactions.shared.acquire(transactionOwner,result:result)
    case "releaseVaultLock":VaultTransactions.shared.release(transactionOwner,token:data["token"] as? String ?? "");result(nil)
    case "configureCredentialPreferences":
      savePasswords=data["savePasswords"] as? Bool ?? true;suggestPasswords=data["suggestPasswords"] as? Bool ?? true
      for page in pages.values {
        page.installForms()
        page.web.callAsyncJavaScript("globalThis.__arbiterUpdatePreferences?.(prefs)",arguments:["prefs":["savePasswords":savePasswords && !page.isPrivate,"suggestPasswords":suggestPasswords]],in:nil,in:.defaultClient,completionHandler:nil)
      }
      result(nil)
    case "configureLanguage":
      UserDefaults(suiteName:AppleCredentialVault.group)?.set(data["language"] as? String ?? "system",forKey:"interface-language");result(nil)
    case "configureBackgroundSync":BackgroundSync.shared.configure(data["enabled"] as? Bool ?? false);result(nil)
    case "adblockStatus":result(adblock.working)
    case "configureAdblock":
      adblock.enabled=data["enabled"] as? Bool ?? true
      adblock.disabledHosts=Set(data["disabledHosts"] as? [String] ?? [])
      for page in pages.values {adblock.apply(page.web.configuration.userContentController,url:page.web.url)}
      result(nil)
    case "canSleep":
      guard let page,page.web.window==nil else {result(false);return}
      page.web.requestMediaPlaybackState { state in result(state != .playing) }
    case "sleep":
      guard let page,page.web.window==nil else {result(false);return}
      page.web.stopLoading();page.web.removeFromSuperview();pages.removeValue(forKey:page.id)
      result(true)
    case "vaultConfiguration":
      do { result(["directory": try AppleCredentialVault.directory().path, "group": AppleCredentialVault.accessGroup ?? ""]) }
      catch { result(FlutterError(code: "vault_unavailable", message: "Shared credential storage is unavailable.", details: nil)) }
    case "credentialsChanged":
      do { try AppleCredentialVault.refreshIdentities(); result(nil) }
      catch { result(FlutterError(code: "identities_unavailable", message: "Could not refresh credential identities.", details: nil)) }
    case "enableCredentialProvider":
      if #available(iOS 18.0, *) { ASSettingsHelper.requestToTurnOnCredentialProviderExtension { enabled in if enabled { try? AppleCredentialVault.refreshIdentities() }; result(enabled) } }
      else { ASSettingsHelper.openCredentialProviderAppSettings { _ in result(false) } }
    case "requestDefaultBrowser":
      if let url=URL(string:UIApplication.openSettingsURLString) { UIApplication.shared.open(url) }
      result(nil)
    case "ready": let url=PendingNavigation.url;PendingNavigation.url=nil;result(url)
    case "privateVisible": privateVisible = data["value"] as? Bool ?? false; result(nil)
    case "secureScreen": secureScreen = data["value"] as? Bool ?? false; result(nil)
    case "close":
      if let page {
        page.web.stopLoading(); page.web.removeFromSuperview(); pages.removeValue(forKey: page.id)
        if !pages.values.contains(where: { $0.isPrivate }) { privateStores.removeAll() }
      }
      result(nil)
    case "pausePrivate":
      for page in pages.values where page.isPrivate { page.web.pauseAllMediaPlayback(completionHandler: {}) }
      result(nil)
    case "load": page?.load(data["url"] as? String ?? ""); result(nil)
    case "back": page?.web.goBack(); result(nil)
    case "forward": page?.web.goForward(); result(nil)
    case "reload": page?.web.reload(); result(nil)
    case "stop": page?.web.stopLoading(); result(nil)
    case "snapshot":
      guard let page, page.web.bounds.width > 0, page.web.window != nil else { result(nil); return }
      let config = WKSnapshotConfiguration(); config.snapshotWidth = 360
      page.web.takeSnapshot(with: config) { image, _ in
        guard let data = image?.jpegData(compressionQuality: 0.65) else { result(nil); return }
        result(FlutterStandardTypedData(bytes: data))
      }
    case "fill":
      guard let page, let origin = data["origin"] as? String,
        let current = page.web.url, MobileBrowserBridge.webURL(current.absoluteString) != nil else { result(false); return }
      page.web.callAsyncJavaScript("""
        return globalThis.__arbiterFill ? globalThis.__arbiterFill({origin:expectedOrigin,username,password,generate}) : false;
        """, arguments: ["expectedOrigin": origin, "username": data["username"] as? String ?? "", "password": data["password"] as? String ?? "", "generate": data["generate"] as? Bool ?? false],
        in: nil, in: .defaultClient) { outcome in
          switch outcome { case .success(let value): result(value); case .failure(_): result(false) }
        }
    case "hasPasswordField":
      guard let page else {result(false);return}
      page.web.callAsyncJavaScript("return globalThis.__arbiterHasPassword?.() ?? false",arguments:[:],in:nil,in:.defaultClient) { outcome in
        if case .success(let value)=outcome {result(value)} else {result(false)}
      }
    case "find":
      let config = WKFindConfiguration(); config.backwards = data["backwards"] as? Bool ?? false; config.wraps = true
      page?.web.find(data["query"] as? String ?? "", configuration: config) { _ in result(nil) }
      if page == nil { result(nil) }
    default: result(FlutterMethodNotImplemented)
    }
  }
  @objc private func hidePrivateContent() {
    guard privateVisible || secureScreen, privacyCover == nil, let window = UIApplication.shared.connectedScenes
      .compactMap({ $0 as? UIWindowScene }).flatMap({ $0.windows }).first(where: { $0.isKeyWindow }) else { return }
    let cover = UIVisualEffectView(effect: UIBlurEffect(style: .systemUltraThinMaterialDark))
    cover.frame = window.bounds; cover.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    cover.contentView.backgroundColor = UIColor.black.withAlphaComponent(0.45)
    window.addSubview(cover); privacyCover = cover
  }
  @objc private func revealApp() {
    DispatchQueue.main.asyncAfter(deadline: .now() + 0.15) { [weak self] in self?.privacyCover?.removeFromSuperview(); self?.privacyCover = nil }
  }
  func startDownload(_ download: WKDownload, page: BrowserPage) {
    let job = BrowserDownload(download: download, page: page, owner: self)
    downloads[job.id] = job
    download.delegate = job
  }
  func finishedDownload(_ id: String) { downloads.removeValue(forKey: id) }
}

private final class HostedBrowserPage: NSObject, FlutterPlatformView {
  let container: UIView
  init(frame: CGRect, page: BrowserPage) {
    container = UIView(frame: frame)
    super.init()
    page.web.removeFromSuperview()
    page.web.frame = container.bounds
    page.web.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    container.addSubview(page.web)
  }
  func view() -> UIView { container }
}

private final class FormHandler:NSObject,WKScriptMessageHandler {
  weak var page:BrowserPage?
  func userContentController(_ controller:WKUserContentController,didReceive message:WKScriptMessage) {
    guard let page,message.frameInfo.isMainFrame,let data=message.body as? [String:Any],let type=data["type"] as? String,
      ["credentialFocus","loginSubmitted","loginSucceeded","activityState"].contains(type),let url=page.web.url,
      message.frameInfo.securityOrigin.host==url.host,message.frameInfo.securityOrigin.protocol==url.scheme else {return}
    let frame=message.frameInfo.securityOrigin
    let defaultPort=url.scheme=="https" ? 443 : 80
    guard (frame.port==0 ? defaultPort:frame.port)==(url.port ?? defaultPort) else {return}
    if type=="loginSubmitted" && (!page.owner.savePasswords || page.isPrivate) {return}
    page.emit(type,data.filter{$0.key != "type" && $0.key != "id"})
  }
}
final class BrowserPage: NSObject, WKNavigationDelegate, WKUIDelegate {
  let id: String
  let web: WKWebView
  let isPrivate: Bool
  unowned let owner: MobileBrowserBridge
  private var observations: [NSKeyValueObservation] = []
  private var loadGeneration=0
  init(id: String, configuration: WKWebViewConfiguration, owner: MobileBrowserBridge, isPrivate: Bool) {
    self.id = id; self.owner = owner; self.isPrivate = isPrivate
    web = WKWebView(frame: .zero, configuration: configuration)
    super.init()
    let handler=FormHandler();handler.page=self
    configuration.userContentController.add(handler,contentWorld:.defaultClient,name:"arbiterForms")
    installForms()
    web.navigationDelegate = self; web.uiDelegate = self
    web.allowsBackForwardNavigationGestures = true
    web.isInspectable = false
    web.scrollView.contentInset.bottom = 90
    web.scrollView.verticalScrollIndicatorInsets.bottom = 90
    let activityGesture = UITapGestureRecognizer(target: self, action: #selector(activity))
    activityGesture.cancelsTouchesInView = false; activityGesture.delegate = self
    web.addGestureRecognizer(activityGesture)
    web.scrollView.panGestureRecognizer.addTarget(self, action: #selector(activity))
    observations = [
      web.observe(\.estimatedProgress, options: [.new]) { [weak self] web, _ in self?.emit("progress", ["progress": web.estimatedProgress]) },
      web.observe(\.title, options: [.new]) { [weak self] web, _ in self?.emit("title", ["title": web.title ?? ""]) },
      web.observe(\.url, options: [.new]) { [weak self] _, _ in self?.location() },
      web.observe(\.canGoBack, options: [.new]) { [weak self] _, _ in self?.location() },
      web.observe(\.canGoForward, options: [.new]) { [weak self] _, _ in self?.location() },
    ]
  }
  func installForms() {
    let controller=web.configuration.userContentController
    controller.removeAllUserScripts()
    if let path=Bundle.main.path(forResource:"BrowserForms",ofType:"js"),let script=try? String(contentsOfFile:path,encoding:.utf8) {
      let prefs="globalThis.__arbiterPreferences={savePasswords:\(owner.savePasswords && !isPrivate),suggestPasswords:\(owner.suggestPasswords)};\n"
      controller.addUserScript(WKUserScript(source:prefs+script,injectionTime:.atDocumentStart,forMainFrameOnly:true,in:.defaultClient))
    }
  }
  @objc func activity() { emit("activity") }
  func emit(_ type: String, _ data: [String: Any] = [:]) { owner.event(id, type, data) }
  func location() { emit("location", ["url": web.url?.absoluteString ?? "", "back": web.canGoBack, "forward": web.canGoForward]) }
  func load(_ text: String) {
    guard let url = MobileBrowserBridge.webURL(text) else { return }
    loadGeneration+=1;let generation=loadGeneration
    owner.adblock.prepare { [weak self] rule in
      guard let self,self.loadGeneration==generation else {return}
      self.owner.event("","adblockReady",["working":rule != nil])
      self.owner.adblock.apply(self.web.configuration.userContentController,url:url)
      self.web.load(URLRequest(url:url))
    }
  }
  func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) { location(); emit("finish", ["success": true]) }
  func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) { failed(error) }
  func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) { failed(error) }
  private func failed(_ error: Error) {
    guard (error as NSError).code != NSURLErrorCancelled else { return }
    emit("error", ["message": error.localizedDescription]); emit("finish", ["success": false])
  }
  func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
    emit("error", ["message": "The page stopped responding. Reload to try again."])
  }
  func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
    guard let url = action.request.url else { decisionHandler(.cancel); return }
    if action.targetFrame?.isMainFrame == true {owner.adblock.apply(web.configuration.userContentController,url:url)}
    if action.shouldPerformDownload { decisionHandler(.download); return }
    if MobileBrowserBridge.webURL(url.absoluteString) != nil || url.absoluteString == "about:blank" {
      if action.targetFrame == nil {
        if action.navigationType == .linkActivated { emit("newTab", ["url": url.absoluteString]) }
        decisionHandler(.cancel)
      } else { decisionHandler(.allow) }
      return
    }
    if ["mailto", "tel", "sms", "geo"].contains(url.scheme ?? ""), action.navigationType == .linkActivated {
      owner.ask("external", ["id": id, "url": url.absoluteString]) { _ in }
    }
    decisionHandler(.cancel)
  }
  func webView(_ webView: WKWebView, decidePolicyFor response: WKNavigationResponse, decisionHandler: @escaping (WKNavigationResponsePolicy) -> Void) {
    let attachment = (response.response as? HTTPURLResponse)?.value(forHTTPHeaderField: "Content-Disposition")?.lowercased().contains("attachment") == true
    decisionHandler(!response.canShowMIMEType || attachment ? .download : .allow)
  }
  func webView(_ webView: WKWebView, navigationAction: WKNavigationAction, didBecome download: WKDownload) { owner.startDownload(download, page: self) }
  func webView(_ webView: WKWebView, navigationResponse: WKNavigationResponse, didBecome download: WKDownload) { owner.startDownload(download, page: self) }
  func webView(_ webView: WKWebView, requestMediaCapturePermissionFor origin: WKSecurityOrigin, initiatedByFrame frame: WKFrameInfo, type: WKMediaCaptureType, decisionHandler: @escaping (WKPermissionDecision) -> Void) {
    decisionHandler(.prompt)
  }
  func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping () -> Void) {
    owner.ask("alert", ["id": id, "message": message]) { _ in completionHandler() }
  }
  func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (Bool) -> Void) {
    owner.ask("confirm", ["id": id, "message": message]) { completionHandler($0 as? Bool ?? false) }
  }
  func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String, defaultText: String?, initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (String?) -> Void) {
    owner.ask("prompt", ["id": id, "title": prompt, "value": defaultText ?? ""]) { completionHandler($0 as? String) }
  }
}
extension BrowserPage: UIGestureRecognizerDelegate {
  func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool { true }
}

final class BrowserDownload: NSObject, WKDownloadDelegate {
  let id = UUID().uuidString
  private let download: WKDownload
  private let page: BrowserPage
  private unowned let owner: MobileBrowserBridge
  private var path = "", name = "Download"
  init(download: WKDownload, page: BrowserPage, owner: MobileBrowserBridge) { self.download = download; self.page = page; self.owner = owner }
  func download(_ download: WKDownload, decideDestinationUsing response: URLResponse, suggestedFilename: String, completionHandler: @escaping (URL?) -> Void) {
    name = URL(fileURLWithPath: suggestedFilename).lastPathComponent.replacingOccurrences(of: ":", with: "_")
    owner.ask("download", ["id": page.id, "name": name]) { [weak self] allowed in
      guard let self, allowed as? Bool == true else { completionHandler(nil); return }
      do {
        let directory = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("Downloads/\(self.id)", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let target = directory.appendingPathComponent(self.name.isEmpty ? "download" : String(self.name.prefix(180)))
        self.path = target.path
        self.emit(complete: false, size: 0, total: response.expectedContentLength)
        completionHandler(target)
      } catch { completionHandler(nil); self.emit(complete: false, error: error.localizedDescription) }
    }
  }
  func downloadDidFinish(_ download: WKDownload) {
    let size = (try? FileManager.default.attributesOfItem(atPath: path)[.size] as? NSNumber)?.int64Value ?? 0
    emit(complete: true, size: size, total: size); owner.finishedDownload(id)
  }
  func download(_ download: WKDownload, didFailWithError error: Error, resumeData: Data?) {
    emit(complete: false, error: error.localizedDescription); owner.finishedDownload(id)
  }
  private func emit(complete: Bool, size: Int64 = 0, total: Int64 = -1, error: String? = nil) {
    var data: [String: Any] = ["downloadId": id, "name": name, "path": path, "private": page.isPrivate, "complete": complete, "received": size, "total": total]
    if let error { data["error"] = error }
    owner.event(page.id, "download", data)
  }
}
