import UIKit
import Flutter
import BackgroundTasks

final class BackgroundSync {
  static let shared=BackgroundSync()
  static let identifier="com.arbiter.mobile.sync"
  private var manualResult:FlutterResult?
  private var owner=UUID().uuidString
  private var engine:FlutterEngine?
  private var task:BGAppRefreshTask?
  private var bridge:FlutterMethodChannel?
  private var completion:FlutterMethodChannel?
  func register() {
    BGTaskScheduler.shared.register(forTaskWithIdentifier:Self.identifier,using:.main) { task in
      guard let task=task as? BGAppRefreshTask else {task.setTaskCompleted(success:false);return}
      self.start(task)
    }
  }
  func configure(_ enabled:Bool) {
    UserDefaults.standard.set(enabled,forKey:"arbiter.backgroundSync")
    if enabled {schedule()} else {
      BGTaskScheduler.shared.cancel(taskRequestWithIdentifier:Self.identifier)
      if task != nil {finish(false)}
    }
  }
  func schedule() {
    guard UserDefaults.standard.bool(forKey:"arbiter.backgroundSync") else {return}
    let request=BGAppRefreshTaskRequest(identifier:Self.identifier)
    request.earliestBeginDate=Date(timeIntervalSinceNow:15*60)
    try? BGTaskScheduler.shared.submit(request)
  }
  private func start(_ task:BGAppRefreshTask) {
    guard self.task==nil,self.engine==nil,UserDefaults.standard.bool(forKey:"arbiter.backgroundSync") else {task.setTaskCompleted(success:false);return}
    self.task=task;schedule();begin(smoke:false)
    task.expirationHandler = { [weak self,weak task] in DispatchQueue.main.async {guard let self,let task,self.task===task else {return};self.finish(false)}}
  }
  func smoke(_ result:@escaping FlutterResult) {
    guard engine==nil else {result(false);return}
    manualResult=result;begin(smoke:true)
  }
  private func begin(smoke:Bool) {
    owner=UUID().uuidString
    let worker=FlutterEngine(name:"ArbiterBackgroundSync",project:nil,allowHeadlessExecution:true)
    engine=worker
    worker.run(withEntrypoint:smoke ? "backgroundSyncSmokeMain":"backgroundSyncMain",libraryURI:"package:arbiter_mobile/main.dart")
    bridge=FlutterMethodChannel(name:"com.arbiter.mobile/browser",binaryMessenger:worker.binaryMessenger)
    bridge?.setMethodCallHandler {call,result in
      switch call.method {
      case "acquireVaultLock":VaultTransactions.shared.acquire(self.owner,result:result)
      case "releaseVaultLock":
        let data=call.arguments as? [String:Any] ?? [:]
        VaultTransactions.shared.release(self.owner,token:data["token"] as? String ?? "");result(nil)
      case "vaultConfiguration":
        do {result(["directory":try AppleCredentialVault.directory().path,"group":AppleCredentialVault.accessGroup ?? ""])}
        catch {result(FlutterError(code:"vault_unavailable",message:"Credential storage is locked.",details:nil))}
      case "credentialsChanged":
        do {try AppleCredentialVault.refreshIdentities();result(nil)}
        catch {result(FlutterError(code:"identities_unavailable",message:"Could not refresh identities.",details:nil))}
      default:result(FlutterMethodNotImplemented)
      }
    }
    completion=FlutterMethodChannel(name:"com.arbiter.mobile/background",binaryMessenger:worker.binaryMessenger)
    completion?.setMethodCallHandler { [weak self] call,result in
      guard call.method=="complete" else {result(FlutterMethodNotImplemented);return}
      let data=call.arguments as? [String:Any];result(nil)
      DispatchQueue.main.async {self?.finish(data?["success"] as? Bool ?? false)}
    }
    GeneratedPluginRegistrant.register(with:worker)
  }
  private func finish(_ success:Bool) {
    let task=self.task;self.task=nil
    let callback=manualResult;manualResult=nil
    completion?.invokeMethod("cancel",arguments:nil)
    let closingOwner=owner
    VaultTransactions.shared.closeWhenIdle(closingOwner) { [weak self] in
      DispatchQueue.main.async {
      guard let self else {return}
      self.bridge?.setMethodCallHandler(nil);self.completion?.setMethodCallHandler(nil)
      self.bridge=nil;self.completion=nil;self.engine?.destroyContext();self.engine=nil
      VaultTransactions.shared.forget(closingOwner)
      callback?(success)
      task?.setTaskCompleted(success:success)
      }
    }
  }
}
