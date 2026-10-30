import Foundation
import WebKit
import CryptoKit

final class AdBlockLists {
  var enabled=true
  var disabledHosts=Set<String>()
  private var rule:WKContentRuleList?
  var working:Bool {rule != nil}
  private var completed=false
  private var waiting:[(WKContentRuleList?)->Void]=[]
  func prepare(_ callback:@escaping (WKContentRuleList?)->Void) {
    if completed {callback(rule);return}
    waiting.append(callback); if waiting.count>1 {return}
    guard let root=Bundle.main.privateFrameworksURL,
      let data=try? Data(contentsOf:root.appendingPathComponent("App.framework/flutter_assets/assets/adblock/webkit.json")),
      let text=String(data:data,encoding:.utf8) else {finish(nil);return}
    let id="Arbiter-"+SHA256.hash(data:data).map{String(format:"%02x",$0)}.joined()
    WKContentRuleListStore.default().lookUpContentRuleList(forIdentifier:id) { [weak self] list,_ in
      guard let self else {return}
      if let list { self.finish(list);return }
      WKContentRuleListStore.default().compileContentRuleList(forIdentifier:id,encodedContentRuleList:text) { [weak self] list,_ in self?.finish(list) }
    }
  }
  private func finish(_ value:WKContentRuleList?) { rule=value;completed=true;let callbacks=waiting;waiting=[];callbacks.forEach{$0(value)} }
  func apply(_ controller:WKUserContentController,url:URL?) {
    controller.removeAllContentRuleLists()
    if enabled,let rule,!disabledHosts.contains(url?.host?.lowercased() ?? "") {controller.add(rule)}
  }
}
