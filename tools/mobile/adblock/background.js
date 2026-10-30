import { WebExtensionBlocker } from '@ghostery/adblocker-webextension';
async function main() {
let enabled = true, disabled = new Set(), port;
const tabs = new Map();
const engine = WebExtensionBlocker.deserialize(new Uint8Array(await (await fetch(browser.runtime.getURL('filters.bin'))).arrayBuffer()));
function allowed(url) {
  try { return enabled && !disabled.has(new URL(url).hostname); } catch { return enabled; }
}
function source(d) { return tabs.get(d.tabId) || d.documentUrl || d.originUrl || d.url; }
const request = engine.onBeforeRequest.bind(engine);
engine.onBeforeRequest = (api, d) => {
  if (d.type === 'main_frame') tabs.set(d.tabId, d.url);
  return allowed(source(d)) ? request(api, d) : {};
};
const headers = engine.onHeadersReceived.bind(engine);
engine.onHeadersReceived = (api,d) => allowed(source(d)) ? headers(api,d) : {};
browser.tabs.onRemoved.addListener(id => tabs.delete(id));
browser.webRequest.onBeforeRequest.addListener(d=>engine.onBeforeRequest(browser,d),{urls:['http://*/*','https://*/*']},['blocking']);
browser.webRequest.onHeadersReceived.addListener(d=>engine.onHeadersReceived(browser,d),{urls:['http://*/*','https://*/*']},['blocking','responseHeaders']);
browser.runtime.onMessage.addListener((message, sender) => {
  if (message?.type !== 'cosmetics' || !sender.tab || !allowed(tabs.get(sender.tab.id) || sender.url)) return undefined;
  const url = sender.url;
  try {
    const hostname = new URL(url).hostname;
    return Promise.resolve(engine.getCosmeticsFilters({url, hostname, domain: hostname,
      classes: (message.classes || []).slice(0,500), ids: (message.ids || []).slice(0,500),
      getBaseRules: true, getRulesFromHostname: true, getRulesFromDOM: true,
      getInjectionRules: false, getExtendedRules: false}).styles);
  } catch { return Promise.resolve(''); }
});
function connect() {
  port = browser.runtime.connectNative('arbiterAdblock');
  port.onMessage.addListener(m => {
    if (m.action !== 'configure') return;
    enabled = m.enabled === true;
    disabled = new Set((m.disabledHosts || []).filter(h => typeof h === 'string'));
    port.postMessage({type:'ready',revision:m.revision || 0});
  });
  port.onDisconnect.addListener(() => setTimeout(connect,1000));
  port.postMessage({type:'configure'});
}
connect();

}
main().catch(()=>{});
