import Flutter
import UIKit

enum PendingNavigation {static var url:String?}
class SceneDelegate: FlutterSceneDelegate {
  override func scene(_ scene:UIScene,willConnectTo session:UISceneSession,options connectionOptions:UIScene.ConnectionOptions) {
    super.scene(scene,willConnectTo:session,options:connectionOptions)
    if let url=connectionOptions.urlContexts.first?.url { (UIApplication.shared.delegate as? AppDelegate)?.handleWebURL(url) }
  }
  override func scene(_ scene:UIScene,openURLContexts contexts:Set<UIOpenURLContext>) {
    if let url=contexts.first?.url,MobileBrowserBridge.webURL(url.absoluteString) != nil { (UIApplication.shared.delegate as? AppDelegate)?.handleWebURL(url) }
    else {super.scene(scene,openURLContexts:contexts)}
  }
  override func sceneDidEnterBackground(_ scene:UIScene) {super.sceneDidEnterBackground(scene);BackgroundSync.shared.schedule()}
}
