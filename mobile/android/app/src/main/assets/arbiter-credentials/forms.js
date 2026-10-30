// Runs only in Arbiter's isolated content world. Never exposes secrets to a page bridge.
(() => {
  if (window.top!==window || !/^https?:$/.test(location.protocol)) return;
  let target, dirty=false, submitted=false, port,editVersion=0,lastForm=null,lastVersion=-1,lastCapture=0;
  const visible=e=>e && e.getClientRects().length && !e.disabled && !e.readOnly;
  const prefs=()=>globalThis.__arbiterPreferences || {savePasswords:false,suggestPasswords:false};
  function userField(form,field) {
    const candidates=[...form.querySelectorAll('input')].filter(e=>['text','email'].includes(e.type)&&visible(e));
    return candidates.find(e=>['username','email'].includes(e.autocomplete)) || candidates.find(e=>e.type==='email') || candidates.filter(e=>field && (e.compareDocumentPosition(field)&Node.DOCUMENT_POSITION_FOLLOWING)).at(-1) || candidates[0];
  }
  const passwords=form=>[...form.querySelectorAll('input[type=password]')].filter(visible);
  function send(message) {
    if(window.webkit?.messageHandlers?.arbiterForms) window.webkit.messageHandlers.arbiterForms.postMessage(message);
    else if(message.type==='loginSubmitted') {browser.runtime.sendNativeMessage('arbiterCredentials',message).catch(()=>{});}
    else {try {port?.postMessage(message)} catch {}}
  }
  function state() {send({type:'activityState',audio:[...document.querySelectorAll('audio,video')].some(e=>!e.paused&&!e.ended&&!e.muted),dirty});}
  function filled(message) {
    if(location.origin!==message.origin) return false;
    let field=visible(target) ? target : passwords(document)[0] || [...document.querySelectorAll('input[autocomplete=username],input[type=email]')].find(visible);
    if(!field) return false;
    const form=field.form||document;
    const fields=passwords(form); if(!fields.length && message.generate) return false;
    const username=field.type!=='password'?field:userField(form,fields[0]);
    function set(e,v) {if(!e)return;Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(e,v);e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));}
    if(!fields.length){set(username,message.username);editVersion++;dirty=true;state();return !!username;}
    if(message.generate) {
      const entries=fields.filter(e=>e.autocomplete!=='current-password');
      if(!entries.length) entries.push(fields[0]);
      entries.forEach(e=>set(e,message.password));
    } else {set(username,message.username);fields.forEach(e=>set(e,message.password));}
    editVersion++;dirty=true;state();return true;
  }
  // This function exists only in the isolated native-client world on WebKit.
  globalThis.__arbiterFill=filled;
  globalThis.__arbiterHasPassword=()=>passwords(document).length>0;
  function connect() {
    if(!globalThis.browser?.runtime) return;
    port=browser.runtime.connectNative('arbiterCredentials');
    port.onMessage.addListener(message=>{
      if(message.action==='preferences'){globalThis.__arbiterUpdatePreferences(message);return;}
      if(message.action==='inspect'){port.postMessage({type:'filled',requestId:message.requestId,filled:passwords(document).length>0});return;}
      if(message.action!=='fill' || typeof message.requestId!=='string')return;
      port.postMessage({type:'filled',requestId:message.requestId,filled:filled(message)});
    });
    port.postMessage({type:'preferences'});
  }
  function focus(e) {if(!prefs().suggestPasswords || !(e instanceof HTMLInputElement))return;
    const fields=passwords(e.form||document);
    if(!['password','email','text'].includes(e.type) || (!fields.length && e.type!=='email' && !['username','email'].includes(e.autocomplete)))return;
    target=e;
    send({type:'credentialFocus',origin:location.origin,hasPassword:fields.length>0,newPassword:fields.some(p=>p.autocomplete==='new-password')});
  }
  globalThis.__arbiterUpdatePreferences=value=>{globalThis.__arbiterPreferences=value;focus(document.activeElement)};
  document.addEventListener('focusin',event=>focus(event.target),true);
  document.addEventListener('input',event=>{if(event.isTrusted){editVersion++;dirty=true;state()}},true);
  function capture(form) {
    if(!prefs().savePasswords)return;
    if(form===lastForm && editVersion===lastVersion && performance.now()-lastCapture<1000)return;
    const fields=passwords(form);if(!fields.length)return;
    const password=fields.find(e=>e.autocomplete==='new-password')||fields[0];
    const user=userField(form,fields[0]);
    if(!password.value || password.value.length>4096)return;
    lastForm=form;lastVersion=editVersion;lastCapture=performance.now();
    submitted=true;
    send({type:'loginSubmitted',origin:location.origin,username:(user?.value||'').slice(0,512),password:password.value});state();
  }
  document.addEventListener('submit',event=>{if(event.isTrusted)capture(event.target)},true);
  document.addEventListener('click',event=>{const button=event.target.closest?.('button,input[type=submit]');if(event.isTrusted && button && button.type==='submit')capture(button.form||document)},true);
  new MutationObserver(()=>{if(submitted&&!passwords(document).length){submitted=false;dirty=false;state();send({type:'loginSucceeded'})}}).observe(document,{childList:true,subtree:true,attributes:true,attributeFilter:['style','class','hidden']});
  for(const name of ['play','pause','ended','volumechange']) document.addEventListener(name,state,true);
  addEventListener('pagehide',()=>{try{port?.disconnect()}catch{};port=null});
  addEventListener('pageshow',event=>{if(event.persisted)connect();state()});
  connect();state();
})();
