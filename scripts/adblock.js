// ApplicationWorld only: no website-facing bridge. QML polls bounded DOM deltas.
(() => {
    if (!/^https?:$/.test(location.protocol) || globalThis.__arbiterAdblock) return;
    const seenClasses = new Set(), seenIds = new Set(), selectors = new Set();
    const pending = [document.documentElement];
    let walker = null, style = null;
    const token = `${Date.now()}-${Math.random()}`;
    const observer = new MutationObserver(records => {
        for (const record of records) {
            const nodes = record.type === 'attributes' ? [record.target] : record.addedNodes;
            for (const node of nodes) if (node instanceof Element && pending.length < 2048) pending.push(node);
        }
    });
    observer.observe(document.documentElement, {subtree: true, childList: true, attributes: true, attributeFilter: ['class', 'id']});
    globalThis.__arbiterAdblock = {
        collect() {
            const classes = [], ids = [];
            for (let visited = 0; visited < 2048; ++visited) {
                let node = walker?.nextNode();
                if (!node) {
                    walker = null;
                    node = pending.shift();
                    if (!node) break;
                    if (!node.isConnected) continue;
                    walker = document.createTreeWalker(node, NodeFilter.SHOW_ELEMENT);
                }
                for (const name of node.classList) if (name.length < 256 && !seenClasses.has(name) && classes.length < 2048 && seenClasses.size < 20000) { seenClasses.add(name); classes.push(name); }
                if (node.id && node.id.length < 256 && !seenIds.has(node.id) && ids.length < 2048 && seenIds.size < 20000) { seenIds.add(node.id); ids.push(node.id); }
            }
            return {url: location.href, token, classes, ids};
        },
        apply(expectedToken, rules) {
            if (token !== expectedToken) return;
            if (!style || !style.isConnected) {
                style = document.createElement('style');
                (document.head || document.documentElement).append(style);
                selectors.clear();
            }
            for (const selector of rules) {
                if (selectors.has(selector) || selectors.size >= 10000) continue;
                try { style.sheet.insertRule(`${selector} { display: none !important; }`); selectors.add(selector); } catch (_) {}
            }
        },
        clear() {
            style?.remove(); style = null; walker = null;
            selectors.clear(); seenClasses.clear(); seenIds.clear();
            pending.length = 0; pending.push(document.documentElement);
        }
    };
})();
