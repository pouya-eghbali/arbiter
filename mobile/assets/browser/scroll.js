// Isolated-world, main-frame scroll intent. Never send page contents or URLs.
(() => {
  if (window.top !== window || !/^https?:$/.test(location.protocol)) return;
  const positions = new WeakMap();
  let intentUntil = 0, pending = null, scheduled = false;
  const root = () => document.scrollingElement;
  const top = element => Math.max(0, Math.min(element.scrollTop, Math.max(0, element.scrollHeight - element.clientHeight)));
  function baseline(event) {
    for (let element = event.target; element; element = element.parentElement)
      if (element.nodeType === 1) positions.set(element, top(element));
    if (root()) positions.set(root(), top(root()));
  }
  function intent(event) {
    if (!event.isTrusted) return;
    intentUntil = performance.now() + 1800;
  }
  document.addEventListener('touchstart', event => { if (event.isTrusted) baseline(event); }, {passive: true, capture: true});
  document.addEventListener('touchmove', intent, {passive: true, capture: true});
  document.addEventListener('wheel', event => { if (event.isTrusted) { baseline(event); intent(event); } }, {passive: true, capture: true});
  document.addEventListener('scroll', event => {
    const element = event.target === document ? root() : event.target;
    if (!element || typeof element.scrollTop !== 'number') return;
    const y = top(element), previous = positions.get(element) ?? y;
    positions.set(element, y);
    const delta = y - previous;
    if (performance.now() > intentUntil || !delta) return;
    // Accumulate only the same scroller/direction within one compositor frame.
    if (pending && pending.element === element && Math.sign(pending.delta) === Math.sign(delta)) pending.delta += delta;
    else pending = {element, delta, y};
    pending.y = y;
    if (scheduled) return;
    scheduled = true;
    requestAnimationFrame(() => {
      scheduled = false;
      const value = pending; pending = null;
      if (!value) return;
      const message = {type: 'toolbarScroll', delta: value.delta, offset: value.y, documentScroll: value.element === root()};
      if (globalThis.__arbiterSend) globalThis.__arbiterSend(message);
      else if (window.webkit?.messageHandlers?.arbiterForms) window.webkit.messageHandlers.arbiterForms.postMessage(message);
      else window.arbiterForms?.postMessage(JSON.stringify(message));
    });
  }, {passive: true, capture: true});
  // Keyboard and viewport changes are layout, not an instruction to hide chrome.
  function reset() { intentUntil = 0; pending = null; }
  addEventListener('resize', reset);
  addEventListener('pagehide', reset);
  window.visualViewport?.addEventListener('resize', reset);
})();
