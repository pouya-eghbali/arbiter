import Flutter
import UIKit

@main
@objc class AppDelegate: FlutterAppDelegate, FlutterImplicitEngineDelegate {
  private var browserBridge: MobileBrowserBridge?

  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    BackgroundSync.shared.register()
    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }

  func handleWebURL(_ url:URL) {
    guard MobileBrowserBridge.webURL(url.absoluteString) != nil else {return}
    PendingNavigation.url=url.absoluteString
    browserBridge?.event("","openUrl",["url":url.absoluteString])
  }
  func didInitializeImplicitFlutterEngine(_ engineBridge: FlutterImplicitEngineBridge) {
    GeneratedPluginRegistrant.register(with: engineBridge.pluginRegistry)
    if let registrar = engineBridge.pluginRegistry.registrar(forPlugin: "ArbiterBrowser") {
      let bridge = MobileBrowserBridge(messenger: registrar.messenger())
      registrar.register(bridge, withId: "com.arbiter.mobile/webview")
      browserBridge = bridge
    }
  }
}
