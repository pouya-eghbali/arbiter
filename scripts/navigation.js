(() => {
    // Read only the main document in ApplicationWorld. Never invoke page handlers.
    const clean = value => (value || '').replace(/[\u0000-\u001f\u007f-\u009f\u202a-\u202e\u2066-\u2069]/g, ' ').replace(/\s+/g, ' ').trim().slice(0, 80);
    const name = element => {
        const ids = (element.getAttribute('aria-labelledby') || '').split(/\s+/).slice(0, 8);
        return clean(ids.map(id => document.getElementById(id)?.textContent || '').join(' '))
            || clean(element.getAttribute('aria-label'));
    };
    const selector = 'nav,[role="navigation"]';
    const nodes = [];
    const seen = new Set();
    let budget = 120;
    for (const nav of Array.from(document.querySelectorAll(selector)).slice(0, 24)) {
        const children = [];
        const owners = new Map();
        for (const link of Array.from(nav.querySelectorAll('a[href]')).slice(0, 500)) {
            if (budget <= 0) break;
            if (link.closest(selector) !== nav || link.hasAttribute('download') || link.closest('[hidden],[inert],[aria-hidden="true"]')) continue;
            if (!link.getClientRects().length) continue;
            const style = getComputedStyle(link);
            if (style.visibility !== 'visible' || style.display === 'none') continue;
            let url;
            try { url = new URL(link.getAttribute('href'), document.baseURI); } catch { continue; }
            if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password || seen.has(url.href)) continue;
            const text = name(link) || clean(link.innerText) || clean(link.querySelector('img')?.alt) || clean(link.title);
            if (!text) continue;
            seen.add(url.href); --budget;
            const node = {label: text, url: url.href, current: link.getAttribute('aria-current') === 'page' || url.href === location.href, children: []};
            // Preserve common nested-list navigation as submenus.
            const li = link.closest('li');
            let owner = li?.parentElement?.closest('li');
            while (owner && nav.contains(owner) && !owners.has(owner)) owner = owner.parentElement?.closest('li');
            if (owner && owners.has(owner)) owners.get(owner).children.push(node);
            else children.push(node);
            if (li && !owners.has(li)) owners.set(li, node);
        }
        if (children.length) nodes.push({label: name(nav) || 'Navigation', children});
    }
    return {url: location.href, nodes};
})()
