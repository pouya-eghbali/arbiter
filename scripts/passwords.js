// Injected only in the main frame, in ApplicationWorld. The website cannot
// call this object or the write-only WebChannel through its own JS world.
(() => {
    "use strict";
    if (window !== window.top || location.protocol !== "https:" || !isSecureContext)
        return;
    const documentId = crypto.randomUUID();
    const forms = new Map();
    const ids = new WeakMap();
    const passwordFields = new Map();
    const passwordFieldIds = new WeakMap();
    let nextId = 0;
    let bridge = null;
    let lastCapture = "";
    let lastCaptureTime = 0;

    const hasAutocomplete = (input, token) => input.autocomplete.split(/\s+/).includes(token);

    const visible = input => input instanceof HTMLInputElement && !input.disabled
        && !input.readOnly && input.getClientRects().length > 0
        && getComputedStyle(input).visibility === "visible"
        && getComputedStyle(input).display !== "none";
    function safeAction(form) {
        if (!(form instanceof HTMLFormElement)) return true;
        try { return new URL(form.action || location.href, location.href).origin === location.origin; }
        catch (_) { return false; }
    }
    function fields(form) {
        const inputs = Array.from(document.querySelectorAll("input"))
            .filter(input => visible(input) && (form instanceof HTMLFormElement
                ? input.form === form : input.form === null));
        const passwords = inputs.filter(input => input.type === "password");
        const password = passwords.find(input => hasAutocomplete(input, "current-password"))
            || passwords.find(input => !hasAutocomplete(input, "new-password"));
        const texts = inputs.filter(input => ["text", "email", "tel"].includes(input.type));
        const username = texts.find(input => input.autocomplete.split(/\s+/).includes("username"))
            || texts.find(input => input.type === "email")
            || texts.filter(input => !password || inputs.indexOf(input) < inputs.indexOf(password)).at(-1);
        return {username, password, passwords};
    }
    function candidates(includeNew = false) {
        return [...Array.from(document.forms), document].filter(form => {
            if (!safeAction(form)) return false;
            const f = fields(form);
            return f.password || (includeNew && f.passwords.length > 0)
                || (f.username && hasAutocomplete(f.username, "username"));
        });
    }
    function selected(includeNew = false) {
        const available = candidates(includeNew);
        const active = document.activeElement;
        const activeForm = active instanceof HTMLInputElement ? (active.form || document) : null;
        if (activeForm && available.includes(activeForm)) return activeForm;
        return available.length === 1 ? available[0] : null;
    }
    function formId(form) {
        if (!ids.has(form)) {
            const id = String(++nextId);
            ids.set(form, id);
            forms.set(id, form);
        }
        return ids.get(form);
    }
    function inspect() {
        const form = selected();
        return form ? {origin: location.origin, document: documentId, form: formId(form)} : null;
    }
    function capture(form = selected(true)) {
        if (!bridge || !form || !safeAction(form)) return false;
        const f = fields(form);
        const newPasswords = f.passwords.filter(input => hasAutocomplete(input, "new-password") && input.value);
        if (newPasswords.length > 1 && newPasswords.some(input => input.value !== newPasswords[0].value))
            return false;
        const password = newPasswords[0] || f.password;
        if (!password || !password.value) return false;
        const username = f.username ? f.username.value : "";
        // Suppress duplicate click/submit events without retaining the password.
        const now = Date.now();
        const signature = formId(form) + "\n" + username;
        if (signature === lastCapture && now - lastCaptureTime < 1500) return false;
        lastCapture = signature;
        lastCaptureTime = now;
        bridge.submit(location.origin, username, password.value);
        return true;
    }
    function setValue(input, value) {
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, "value").set.call(input, value);
        input.dispatchEvent(new Event("input", {bubbles: true}));
        input.dispatchEvent(new Event("change", {bubbles: true}));
    }
    function fill(origin, expectedDocument, id, username, password) {
        if (location.origin !== origin || documentId !== expectedDocument || !isSecureContext)
            return false;
        const form = forms.get(id);
        if (!form || (form !== document && !form.isConnected) || !safeAction(form)) return false;
        const f = fields(form);
        if (!f.password && !f.username) return false;
        if (f.username) setValue(f.username, username);
        // Input handlers may replace the form, navigate, or change its action.
        if (location.origin !== origin || !safeAction(form)
            || (form !== document && !form.isConnected)) return false;
        if (f.password) {
            if (!visible(f.password) || !f.password.isConnected || f.password.type !== "password"
                || (form instanceof HTMLFormElement && f.password.form !== form)) return false;
            setValue(f.password, password);
        }
        return true;
    }
    function inspectPasswordAt(x, y) {
        if (!Number.isFinite(x) || !Number.isFinite(y)) return null;
        const input = document.elementFromPoint(x, y);
        if (!visible(input) || input.type !== "password") return null;
        const form = input.form || document;
        if (!safeAction(form)) return null;
        if (!passwordFieldIds.has(input)) {
            const id = String(passwordFields.size + 1);
            passwordFieldIds.set(input, id);
            passwordFields.set(id, input);
        }
        return {origin: location.origin, document: documentId, form: formId(form), field: passwordFieldIds.get(input)};
    }
    function suggestPassword(origin, expectedDocument, formToken, fieldToken, password) {
        if (location.origin !== origin || documentId !== expectedDocument || !isSecureContext
            || typeof password !== "string" || password.length < 12 || password.length > 2560) return false;
        const form = forms.get(formToken);
        const input = passwordFields.get(fieldToken);
        const valid = field => visible(field) && field.isConnected && field.type === "password"
            && (field.form || document) === form && safeAction(form)
            && (form === document || form.isConnected) && location.origin === origin;
        if (!input || !form || !valid(input)) return false;
        // Only explicit new-password fields can be paired with confirmation.
        // A change-password form's current-password field must stay untouched.
        const targets = hasAutocomplete(input, "new-password")
            ? fields(form).passwords.filter(field => hasAutocomplete(field, "new-password")) : [input];
        for (const field of targets) {
            if (!valid(field)) return false;
            setValue(field, password);
        }
        return true;
    }
    globalThis.__arbiterPasswords = Object.freeze({inspect, capture: () => capture(), fill, inspectPasswordAt, suggestPassword});
    if (typeof qt === "undefined" || !qt.webChannelTransport) return;
    new QWebChannel(qt.webChannelTransport, channel => { bridge = channel.objects.passwordCapture; });
    let userGesture = -Infinity;
    function suggest(input) {
        if (!bridge || !visible(input) || !["text", "email", "tel", "password"].includes(input.type)
            || hasAutocomplete(input, "new-password")) return;
        const form = input.form || document;
        if (!safeAction(form) || !candidates().includes(form)) return;
        const f = fields(form);
        if (input !== f.username && input !== f.password) return;
        const rect = input.getBoundingClientRect();
        bridge.focus({origin: location.origin, document: documentId, form: formId(form),
            x: rect.x, y: rect.y, width: rect.width, height: rect.height});
    }
    document.addEventListener("pointerdown", event => {
        if (!event.isTrusted || event.button !== 0) return;
        userGesture = performance.now();
        if (event.target instanceof HTMLInputElement) suggest(event.target);
    }, true);
    document.addEventListener("keydown", event => { if (event.isTrusted) userGesture = performance.now(); }, true);
    document.addEventListener("focusin", event => {
        if (event.isTrusted && performance.now() - userGesture < 500 && event.target instanceof HTMLInputElement)
            suggest(event.target);
    }, true);
    document.addEventListener("submit", event => {
        if (event.target instanceof HTMLFormElement) capture(event.target);
    }, true);
    // Covers JS-driven sign-in buttons on forms that don't dispatch submit.
    document.addEventListener("click", event => {
        if (!event.isTrusted || !(event.target instanceof Element)) return;
        const button = event.target.closest("button,input[type=submit],input[type=button],[role=button]");
        if (button && (button.type === "submit" || /sign.?in|log.?in|sign.?up|create.?account|register/i.test(button.textContent || button.value || "")))
            capture(button.form || selected(true));
    }, true);
})();
