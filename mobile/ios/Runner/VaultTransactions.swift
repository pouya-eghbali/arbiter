import Foundation
import Flutter

// POSIX record locks are process-wide. Serialize separate Flutter engines here,
// in addition to the on-disk lock shared with the credential extension process.
final class VaultTransactions {
  static let shared=VaultTransactions()
  private var holder:(String,String)?
  private var waiting:[(String,FlutterResult)]=[]
  private var idle:[String:()->Void]=[:]
  private var closed=Set<String>()
  func acquire(_ owner:String,result:@escaping FlutterResult) {
    if closed.contains(owner) {result(FlutterError(code:"cancelled",message:"Sync ended.",details:nil));return}
    if holder != nil {waiting.append((owner,result));return}
    let token=UUID().uuidString;holder=(owner,token);result(token)
  }
  func release(_ owner:String,token:String) {
    guard holder?.0==owner,holder?.1==token else {return}
    holder=nil
    idle.removeValue(forKey:owner)?()
    advance()
  }
  private func advance() {
    guard holder==nil,!waiting.isEmpty else {return}
    let (owner,result)=waiting.removeFirst();acquire(owner,result:result)
  }
  func forget(_ owner:String) {closed.remove(owner)}
  func closeWhenIdle(_ owner:String,completion:@escaping ()->Void) {
    closed.insert(owner)
    let cancelled=waiting.filter{$0.0==owner};waiting.removeAll{$0.0==owner}
    cancelled.forEach{$0.1(FlutterError(code:"cancelled",message:"Sync ended.",details:nil))}
    if holder?.0==owner {idle[owner]=completion} else {completion()}
  }
}
