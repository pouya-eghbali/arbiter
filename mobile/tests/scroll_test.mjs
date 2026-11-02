import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
import assert from 'node:assert/strict';
const listeners = new Map(), frames = [], messages = [];
const element = () => ({nodeType: 1, scrollTop: 0, scrollHeight: 2000, clientHeight: 800, parentElement: null});
const page = element(), nested = element(); nested.parentElement = page;
let now = 100;
const listen = (name, handler) => listeners.set(name, handler);
const document = {scrollingElement: page, addEventListener: listen};
const window = {addEventListener: listen, visualViewport: {addEventListener: (name, handler) => listeners.set('viewport:' + name, handler)}};
window.top = window;
runInNewContext(readFileSync(new URL('../assets/browser/scroll.js', import.meta.url), 'utf8'), {
  window, document, location: {protocol: 'https:'}, performance: {now: () => now},
  addEventListener: listen, requestAnimationFrame: callback => frames.push(callback),
  __arbiterSend: message => messages.push(message),
});
const event = (name, target = page, isTrusted = true) => listeners.get(name)({target, isTrusted});
const flush = () => { while (frames.length) frames.shift()(); };
page.scrollTop = 100; event('scroll', document); flush(); assert.equal(messages.length, 0, 'programmatic scroll');
event('touchstart'); event('touchmove');
page.scrollTop = 140; event('scroll', document); page.scrollTop = 180; event('scroll', document); flush();
assert.equal(messages.length, 1); assert.equal(messages[0].delta, 80); assert.equal(messages[0].offset, 180); assert.equal(messages[0].documentScroll, true);
page.scrollTop = 160; event('scroll', document); flush(); assert.equal(messages.at(-1).delta, -20);
event('touchstart', nested); event('touchmove', nested); nested.scrollTop = 90; event('scroll', nested); flush();
assert.equal(messages.at(-1).delta, 90, 'nested page scroller'); assert.equal(messages.at(-1).documentScroll, false);
page.scrollTop = -30; event('scroll', document); flush(); assert.equal(messages.at(-1).offset, 0, 'top bounce clamped');
const count = messages.length;
page.scrollTop = -80; event('scroll', document); flush(); assert.equal(messages.length, count, 'bounce jitter ignored');
event('viewport:resize'); page.scrollTop = 40; event('scroll', document); flush(); assert.equal(messages.length, count, 'viewport changes are not scroll intent');
event('touchstart'); event('touchmove', page, false); page.scrollTop = 80; event('scroll', document); flush(); assert.equal(messages.length, count, 'untrusted gesture ignored');
event('wheel'); page.scrollTop = 120; event('scroll', document); flush(); assert.equal(messages.at(-1).delta, 40);
now += 2000; page.scrollTop = 180; event('scroll', document); flush(); assert.equal(messages.at(-1).offset, 120, 'expired gesture ignored');
console.log('Scroll intent, batching, nested scrollers, bounce, viewport changes and trust checks passed');
