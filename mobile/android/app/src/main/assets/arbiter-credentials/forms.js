// Isolated, built-in content script. No page-to-native command listener.
(() => {
  if (window.top !== window) return;
  let port;
  function connect() {
  if (port) port.disconnect();
  port = browser.runtime.connectNative('arbiterCredentials');
  port.onMessage.addListener(message => {
    if (message.action !== 'fill' || typeof message.requestId !== 'string') return;
    let filled = false;
    if (location.origin === message.origin && /^https?:$/.test(location.protocol)) {
      const field = [...document.querySelectorAll('input[type=password]')].find(e => e.getClientRects().length && !e.disabled);
      if (field) {
        const form = field.form || document;
        const username = [...form.querySelectorAll('input')].find(e => ['text', 'email'].includes(e.type) && e.getClientRects().length && !e.disabled);
        const set = (element, value) => {
          if (!element) return;
          Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set.call(element, value);
          element.dispatchEvent(new Event('input', {bubbles: true}));
          element.dispatchEvent(new Event('change', {bubbles: true}));
        };
        set(username, message.username); set(field, message.password); filled = true;
      }
    }
    port.postMessage({type: 'filled', requestId: message.requestId, filled});
  });
  }
  connect();
  // A cached history document can suspend a native port. Reconnect when the
  // document is restored instead of sending credentials into an inactive page.
  addEventListener('pagehide', () => { if (port) port.disconnect(); port = null; });
  addEventListener('pageshow', event => { if (event.persisted) connect(); });
})();
