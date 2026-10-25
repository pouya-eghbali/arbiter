// Copyright 2026 Arbiter contributors
// SPDX-License-Identifier: GPL-3.0-only
// Native extension functions. No web-page JavaScript bridge is exposed.
#include "arbiter_extensions.h"
#include "web_contents_adapter.h"
#include "extensions/extension_web_contents_observer_qt.h"
#include "extensions/common/mojom/frame.mojom.h"
#include <QHash>
#include <QPointer>
#include <QWindow>
#include <QShortcut>
#include <QColor>
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/manifest.h"
#include "base/supports_user_data.h"
#include <QMetaProperty>
#include <QTimer>
#include <QVariant>
#include <algorithm>
#include <map>
#include <deque>
#include "base/base64.h"
#include "base/json/json_writer.h"
#include "extensions/browser/scripting_utils.h"
#include "extensions/common/mojom/code_injection.mojom.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"
#include "extensions/common/mojom/match_origin_as_fallback.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace QtWebEngineCore {
using content::WebContents;
using content::BrowserContext;
using extensions::PermissionsData;
using extensions::Extension;
namespace {
int nextTabId = 1, nextWindowId = 1;
QHash<QObject *, int> windowIds;
QPointer<QWindow> lastWindow;
struct Tab;
void windowFocusChanged(QWindow *);
std::map<WebContentsAdapter *, std::unique_ptr<Tab>> tabs;
void broadcast(BrowserContext *context, extensions::events::HistogramValue histogram,
               const std::string &name, base::Value::List args);

struct Tab : content::WebContentsObserver {
    explicit Tab(WebContentsAdapter *a) : WebContentsObserver(a->webContents()), adapter(a),
        context(a->extensionContext()), executor(a->webContents()) {
        int stored = context->property("extensionTabId").toInt();
        id = stored > 0 ? stored : nextTabId++;
        context->setProperty("extensionTabId", id);
        window = qobject_cast<QWindow *>(context->property("window").value<QObject *>());
        if (window) {
            if (!windowIds.contains(window)) {
                windowIds[window] = nextWindowId++;
                QObject::connect(window, &QObject::destroyed, [key = window.data()] { windowIds.remove(key); });
                QObject::connect(window, &QWindow::activeChanged, window, [w = window] {
                    if (!w) return;
                    if (w->isActive()) lastWindow = w;
                    windowFocusChanged(w);
                });
            }
            windowId = windowIds.value(window);
            if (window->isActive() || !lastWindow) lastWindow = window;
        }
    }
    WebContentsAdapter *adapter;
    QPointer<QObject> context;
    QPointer<QWindow> window;
    int id = -1, windowId = -1;
    bool wasActive = false, wasPinned = false;
    std::map<std::string, GURL> grants;
    std::map<std::string, std::deque<base::TimeTicks>> captures;
    extensions::ScriptExecutor executor;
    std::unique_ptr<ArbiterExtensionTabNotifier> notifier;
    bool usable() const { return web_contents() && context && window && !context->property("blocked").toBool()
        && !web_contents()->GetBrowserContext()->IsOffTheRecord(); }
    bool active() const { return context && context->property("active").toBool(); }
    void revoke() {
        if (!web_contents()) { grants.clear(); return; }
        std::vector<std::string> ids;
        for (const auto &[id, origin] : grants) {
            const auto *e = extensions::ExtensionRegistry::Get(web_contents()->GetBrowserContext())->enabled_extensions().GetByID(id);
            if (e) e->permissions_data()->ClearTabSpecificPermissions(this->id);
            ids.push_back(id);
        }
        if (!ids.empty()) {
            auto *helper = extensions::RendererStartupHelperFactory::GetForBrowserContext(web_contents()->GetBrowserContext());
            for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd(); it.Advance()) {
                auto *process = it.GetCurrentValue();
                if (process->GetBrowserContext() == web_contents()->GetBrowserContext())
                    if (auto *renderer = helper->GetRenderer(process)) renderer->ClearTabSpecificPermissions(ids, id, false);
            }
        }
        grants.clear();
    }
    void DidFinishNavigation(content::NavigationHandle *navigation) override {
        if (!usable()) return;
        if (!navigation->HasCommitted() || !navigation->IsInPrimaryMainFrame()) return;
        if (!navigation->IsSameDocument()) {
            const auto origin = web_contents()->GetLastCommittedURL().DeprecatedGetOriginAsURL();
            for (const auto &[extensionId, granted] : grants) if (granted.DeprecatedGetOriginAsURL() != origin) { revoke(); break; }
        }
        base::Value::Dict change;
        change.Set("status", web_contents()->IsLoading() ? "loading" : "complete");
        change.Set("url", web_contents()->GetLastCommittedURL().spec());
        base::Value::List args; args.Append(id); args.Append(std::move(change));
        args.Append(*arbiterExtensionTabInfo(web_contents()));
        broadcast(web_contents()->GetBrowserContext(), extensions::events::TABS_ON_UPDATED, "tabs.onUpdated", std::move(args));
    }
    void change(base::Value::Dict change) {
        if (!usable()) return;
        base::Value::List args; args.Append(id); args.Append(std::move(change)); args.Append(*arbiterExtensionTabInfo(web_contents()));
        broadcast(web_contents()->GetBrowserContext(), extensions::events::TABS_ON_UPDATED, "tabs.onUpdated", std::move(args));
    }
    void TitleWasSet(content::NavigationEntry *) override { change(base::Value::Dict().Set("title", base::UTF16ToUTF8(web_contents()->GetTitle()))); }
    void OnAudioStateChanged(bool audible) override { change(base::Value::Dict().Set("audible", audible)); }
    void DidUpdateAudioMutingState(bool muted) override { change(base::Value::Dict().Set("mutedInfo", base::Value::Dict().Set("muted", muted))); }
    void DidStopLoading() override {
        if (!usable()) return;
        base::Value::List args; args.Append(id); args.Append(base::Value::Dict().Set("status", "complete"));
        args.Append(*arbiterExtensionTabInfo(web_contents()));
        broadcast(web_contents()->GetBrowserContext(), extensions::events::TABS_ON_UPDATED, "tabs.onUpdated", std::move(args));
    }
};
void windowFocusChanged(QWindow *window) {
    std::set<BrowserContext *> profiles;
    for (const auto &[adapter, tab] : tabs) if (tab->window == window && tab->usable()) profiles.insert(tab->web_contents()->GetBrowserContext());
    for (auto *profile : profiles) {
        base::Value::List args; args.Append(window->isActive() ? windowIds.value(window) : -1);
        broadcast(profile, extensions::events::WINDOWS_ON_FOCUS_CHANGED, "windows.onFocusChanged", std::move(args));
    }
}
Tab *find(WebContents *contents) {
    for (const auto &[a, tab] : tabs) if (tab->web_contents() == contents && tab->usable()) return tab.get();
    return nullptr;
}
std::vector<Tab *> available(BrowserContext *context) {
    std::vector<Tab *> result;
    for (const auto &[a, tab] : tabs)
        if (tab->usable() && tab->web_contents()->GetBrowserContext() == context) result.push_back(tab.get());
    std::sort(result.begin(), result.end(), [](Tab *a, Tab *b) {
        if (a->windowId != b->windowId) return a->windowId < b->windowId;
        return a->context->property("index").toInt() < b->context->property("index").toInt();
    });
    return result;
}
Tab *target(BrowserContext *context, int id, int windowId = -2) {
    for (auto *tab : available(context)) {
        if (id >= 0 && tab->id == id) return tab;
        if (id < 0 && tab->active() && (windowId >= 0 ? tab->windowId == windowId : tab->window == lastWindow)) return tab;
    }
    if (id < 0 && windowId < 0) for (auto *tab : available(context)) if (tab->active()) return tab;
    return nullptr;
}
bool access(const Extension *extension, Tab *tab, std::string *error) {
    const auto &url = tab->web_contents()->GetLastCommittedURL();
    if (extensions::ExtensionRegistry::Get(tab->web_contents()->GetBrowserContext())->enabled_extensions().GetByID(extension->id()) != extension) {
        *error = "The extension is no longer enabled."; return false;
    }
    // No browser internals, other extensions, local files, opaque documents, or store pages.
    if (!url.SchemeIsHTTPOrHTTPS() || url.host() == "chromewebstore.google.com" || (url.host() == "chrome.google.com" && url.path().starts_with("/webstore"))) { *error = "Cannot access this restricted page."; return false; }
    return extension->permissions_data()->CanAccessPage(url, tab->id, error);
}
base::Value::Dict describe(Tab *tab, const Extension *extension = nullptr) {
    WebContents *wc = tab->web_contents();
    base::Value::Dict value;
    value.Set("id", tab->id); value.Set("windowId", tab->windowId);
    value.Set("index", tab->context->property("index").toInt());
    value.Set("active", tab->active()); value.Set("selected", tab->active()); value.Set("highlighted", tab->active());
    value.Set("pinned", tab->context->property("pinned").toBool()); value.Set("incognito", false);
    value.Set("status", wc->IsLoading() ? "loading" : "complete");
    value.Set("audible", wc->IsCurrentlyAudible());
    value.Set("mutedInfo", base::Value::Dict().Set("muted", wc->IsAudioMuted()));
    value.Set("width", wc->GetContainerBounds().width()); value.Set("height", wc->GetContainerBounds().height());
    const auto *permissions = extension ? extension->permissions_data() : nullptr;
    if (!permissions || permissions->HasAPIPermission(extensions::mojom::APIPermissionID::kTab)
        || permissions->HasHostPermission(wc->GetLastCommittedURL())
        || permissions->HasTabPermissionsForSecurityOrigin(tab->id, wc->GetLastCommittedURL())) {
        value.Set("url", wc->GetLastCommittedURL().spec()); value.Set("title", base::UTF16ToUTF8(wc->GetTitle()));
        if (auto *pending = wc->GetController().GetPendingEntry()) value.Set("pendingUrl", pending->GetVirtualURL().spec());
        if (auto *entry = wc->GetController().GetVisibleEntry()) value.Set("favIconUrl", entry->GetFavicon().url.spec());
    }
    return value;
}
base::Value::Dict describeWindow(Tab *tab, const Extension *extension, bool populate) {
    auto *w = tab->window.data(); base::Value::Dict value;
    value.Set("id", tab->windowId); value.Set("focused", w->isActive()); value.Set("incognito", false);
    value.Set("type", "normal"); value.Set("alwaysOnTop", bool(w->flags() & Qt::WindowStaysOnTopHint));
    value.Set("left", w->x()); value.Set("top", w->y()); value.Set("width", w->width()); value.Set("height", w->height());
    const auto state = w->windowState();
    value.Set("state", state == Qt::WindowFullScreen ? "fullscreen" : state == Qt::WindowMaximized ? "maximized" : state == Qt::WindowMinimized ? "minimized" : "normal");
    if (populate) {
        base::Value::List list;
        for (auto *other : available(tab->web_contents()->GetBrowserContext())) if (other->windowId == tab->windowId) list.Append(describe(other, extension));
        value.Set("tabs", std::move(list));
    }
    return value;
}
struct MenuEntry {
    std::string key;
    base::Value id;
    base::Value::Dict properties;
};
std::string menuKey(const base::Value &id) { return id.is_string() ? "s:" + id.GetString() : id.is_int() ? "i:" + std::to_string(id.GetInt()) : ""; }
QString commandKey(const base::Value::Dict &command) {
    const auto *suggestion = command.FindDict("suggested_key"); if (!suggestion) return {};
#if defined(Q_OS_MACOS)
    const auto *key = suggestion->FindString("mac");
#elif defined(Q_OS_WIN)
    const auto *key = suggestion->FindString("windows");
#else
    const auto *key = suggestion->FindString("linux");
#endif
    if (!key) key = suggestion->FindString("default");
    if (!key) return {};
    QString result = QString::fromStdString(*key);
#if defined(Q_OS_MACOS)
    result.replace(QStringLiteral("MacCtrl"), QStringLiteral("__CONTROL__"));
    result.replace(QStringLiteral("Ctrl"), QStringLiteral("Meta"));
    result.replace(QStringLiteral("Command"), QStringLiteral("Meta"));
    result.replace(QStringLiteral("__CONTROL__"), QStringLiteral("Ctrl"));
#endif
    return result;
}
class ProfileUi final : public QObject, public base::SupportsUserData::Data, public extensions::ExtensionRegistryObserver {
public:
    explicit ProfileUi(BrowserContext *context) : context(context), registry(extensions::ExtensionRegistry::Get(context)) { registry->AddObserver(this); }
    ~ProfileUi() override { clearShortcuts(); if (registry) registry->RemoveObserver(this); }
    BrowserContext *context;
    extensions::ExtensionRegistry *registry;
    std::map<std::string, std::vector<MenuEntry>> menus;
    std::map<std::pair<QWindow *, std::string>, std::vector<QPointer<QShortcut>>> shortcuts;
    static ProfileUi *get(BrowserContext *context) {
        static int key;
        auto *value = static_cast<ProfileUi *>(context->GetUserData(&key));
        if (!value) { auto created = std::make_unique<ProfileUi>(context); value = created.get(); context->SetUserData(&key, std::move(created)); }
        return value;
    }
    void clearShortcuts(const std::string &id = "") {
        for (auto it = shortcuts.begin(); it != shortcuts.end();) {
            if (id.empty() || it->first.second == id) {
                for (auto &shortcut : it->second) if (shortcut) delete shortcut;
                it = shortcuts.erase(it);
            } else ++it;
        }
    }
    void install(QWindow *window, const Extension *extension) {
        const auto key = std::make_pair(window, extension->id());
        if (!window || shortcuts.count(key)) return;
        auto &installed = shortcuts[key];
        const auto *commands = extension->manifest()->FindDictPath("commands"); if (!commands) return;
        for (const auto [name, value] : *commands) {
            if (!value.is_dict() || name.starts_with("_")) continue;
            const auto sequence = QKeySequence::fromString(commandKey(value.GetDict()), QKeySequence::PortableText);
            if (sequence.isEmpty()) continue;
            auto *shortcut = new QShortcut(sequence, window);
            shortcut->setContext(Qt::WindowShortcut); shortcut->setAutoRepeat(false);
            installed.push_back(shortcut);
            QObject::connect(shortcut, &QShortcut::activated, this, [this, window, id = extension->id(), name] {
                auto *tab = target(context, -1, windowIds.value(window, -1));
                const auto *extension = registry ? registry->enabled_extensions().GetByID(id) : nullptr;
                if (!tab || !extension || !window->isActive()) return;
                arbiterGrantActiveTab(tab->adapter, QString::fromStdString(id));
                base::Value::List args; args.Append(name); args.Append(describe(tab, extension));
                auto event = std::make_unique<extensions::Event>(extensions::events::COMMANDS_ON_COMMAND, "commands.onCommand", std::move(args), context);
                event->user_gesture = extensions::EventRouter::UserGestureState::kEnabled;
                extensions::EventRouter::Get(context)->DispatchEventToExtension(id, std::move(event));
            });
        }
    }
    void installWindow(QWindow *window) {
        for (const auto &extension : registry->enabled_extensions()) install(window, extension.get());
    }
    void OnExtensionLoaded(BrowserContext *, const Extension *extension) override {
        for (auto *tab : available(context)) install(tab->window, extension);
    }
    void OnExtensionUnloaded(BrowserContext *, const Extension *extension, extensions::UnloadedExtensionReason) override {
        menus.erase(extension->id()); clearShortcuts(extension->id());
    }
    void OnShutdown(extensions::ExtensionRegistry *) override { clearShortcuts(); menus.clear(); registry = nullptr; }
};
bool menuMatches(const MenuEntry &entry, const QVariantMap &data) {
    if (!entry.properties.FindBool("visible").value_or(true)) return false;
    if (const auto *patterns = entry.properties.FindList("documentUrlPatterns")) {
        bool found = false;
        for (const auto &value : *patterns) if (value.is_string()) {
            URLPattern pattern(URLPattern::SCHEME_ALL);
            if (pattern.Parse(value.GetString()) == URLPattern::ParseResult::kSuccess && pattern.MatchesURL(GURL(data.value(QStringLiteral("pageUrl")).toString().toStdString()))) found = true;
        }
        if (!found) return false;
    }
    if (const auto *patterns = entry.properties.FindList("targetUrlPatterns")) {
        bool found = false;
        for (const auto &value : *patterns) if (value.is_string()) {
            URLPattern pattern(URLPattern::SCHEME_ALL);
            if (pattern.Parse(value.GetString()) == URLPattern::ParseResult::kSuccess &&
                (pattern.MatchesURL(GURL(data.value(QStringLiteral("linkUrl")).toString().toStdString())) || pattern.MatchesURL(GURL(data.value(QStringLiteral("srcUrl")).toString().toStdString())))) found = true;
        }
        if (!found) return false;
    }
    const auto *contexts = entry.properties.FindList("contexts"); if (!contexts) return true;
    for (const auto &context : *contexts) if (context.is_string()) {
        const auto &type = context.GetString();
        if (type == "all" || type == "page" || (type == "selection" && !data.value(QStringLiteral("selectionText")).toString().isEmpty())
            || (type == "link" && !data.value(QStringLiteral("linkUrl")).toString().isEmpty())
            || (type == "editable" && data.value(QStringLiteral("editable")).toBool())
            || (type == data.value(QStringLiteral("mediaType")).toString().toStdString())) return true;
    }
    return false;
}
void broadcast(BrowserContext *context, extensions::events::HistogramValue histogram,
               const std::string &name, base::Value::List args) {
    auto *router = extensions::EventRouter::Get(context); if (!router) return;
    for (const auto &extension : extensions::ExtensionRegistry::Get(context)->enabled_extensions()) {
        auto values = args.Clone();
        if (name == "tabs.onCreated" && !values.empty() && values[0].is_dict()) {
            auto *tab = target(context, values[0].GetDict().FindInt("id").value_or(-1));
            if (!tab) continue;
            values[0] = base::Value(describe(tab, extension.get()));
        } else if (name == "tabs.onUpdated" && values.size() == 3 && values[0].is_int()) {
            auto *tab = target(context, values[0].GetInt()); if (!tab) continue;
            auto description = describe(tab, extension.get());
            if (!description.contains("url")) { values[1].GetDict().Remove("url"); values[1].GetDict().Remove("title"); }
            values[2] = base::Value(std::move(description));
        }
        router->DispatchEventToExtension(extension->id(), std::make_unique<extensions::Event>(histogram, name, std::move(values), context));
    }
}
}
ArbiterExtensionTabNotifier::ArbiterExtensionTabNotifier(WebContentsAdapter *a) : adapter(a) {
    QObject *object = a->extensionContext();
    const auto slot = metaObject()->method(metaObject()->indexOfSlot("refresh()"));
    for (int i = 0; i < object->metaObject()->propertyCount(); ++i) {
        const auto property = object->metaObject()->property(i);
        if (property.hasNotifySignal()) QObject::connect(object, property.notifySignal(), this, slot);
    }
}
ArbiterExtensionTabNotifier::~ArbiterExtensionTabNotifier() = default;
void ArbiterExtensionTabNotifier::refresh() {
    auto it = tabs.find(adapter); if (it == tabs.end()) return;
    auto *tab = it->second.get(); if (!tab->usable()) return;
    if (tab->active() && !tab->wasActive) {
        base::Value::List args; args.Append(base::Value::Dict().Set("tabId", tab->id).Set("windowId", tab->windowId));
        broadcast(tab->web_contents()->GetBrowserContext(), extensions::events::TABS_ON_ACTIVATED, "tabs.onActivated", std::move(args));
    }
    const bool pinned = tab->context->property("pinned").toBool();
    if (pinned != tab->wasPinned) {
        base::Value::List args; args.Append(tab->id); args.Append(base::Value::Dict().Set("pinned", pinned)); args.Append(describe(tab));
        broadcast(tab->web_contents()->GetBrowserContext(), extensions::events::TABS_ON_UPDATED, "tabs.onUpdated", std::move(args));
    }
    tab->wasPinned = pinned;
    tab->wasActive = tab->active();
}
int arbiterEnsureExtensionTabId(QObject *context) {
    if (!context) return -1;
    int id = context->property("extensionTabId").toInt();
    if (id <= 0) { id = nextTabId++; context->setProperty("extensionTabId", id); }
    return id;
}
void arbiterRegisterExtensionTab(WebContentsAdapter *adapter, bool notify) {
    if (tabs.count(adapter) || !adapter->isInitialized() || !adapter->extensionContext()) return;
    auto tab = std::make_unique<Tab>(adapter); tab->wasActive = tab->active(); tab->wasPinned = tab->context->property("pinned").toBool();
    tab->notifier = std::make_unique<ArbiterExtensionTabNotifier>(adapter);
    auto *raw = tab.get(); tabs.emplace(adapter, std::move(tab));
    if (raw->usable()) ProfileUi::get(raw->web_contents()->GetBrowserContext())->installWindow(raw->window);
    if (auto *observer = extensions::ExtensionWebContentsObserverQt::FromWebContents(raw->web_contents()))
        raw->web_contents()->ForEachRenderFrameHost([&](content::RenderFrameHost *host) {
            if (auto *frame = observer->GetLocalFrame(host)) frame->SetTabId(raw->id);
        });
    if (notify && raw->usable()) { base::Value::List args; args.Append(describe(raw));
        broadcast(raw->web_contents()->GetBrowserContext(), extensions::events::TABS_ON_CREATED, "tabs.onCreated", std::move(args)); }
}
void arbiterRemoveExtensionTab(WebContentsAdapter *adapter, bool notify) {
    auto it = tabs.find(adapter); if (it == tabs.end()) return;
    auto tab = std::move(it->second); tabs.erase(it); tab->revoke();
    base::Value::List args; args.Append(tab->id); args.Append(base::Value::Dict().Set("windowId", tab->windowId).Set("isWindowClosing", false));
    if (notify && tab->web_contents()) broadcast(tab->web_contents()->GetBrowserContext(), extensions::events::TABS_ON_REMOVED, "tabs.onRemoved", std::move(args));
}
void arbiterExtensionIds(WebContents *contents, int *id, int *window) {
    auto *tab = find(contents); if (id) *id = tab ? tab->id : -1; if (window) *window = tab ? tab->windowId : -1;
}
WebContents *arbiterExtensionContents(BrowserContext *context, int id) { auto *tab = target(context, id); return tab ? tab->web_contents() : nullptr; }
extensions::ScriptExecutor *arbiterExtensionScriptExecutor(WebContents *contents) { auto *tab = find(contents); return tab ? &tab->executor : nullptr; }
std::optional<base::Value::Dict> arbiterExtensionTabInfo(WebContents *contents) { auto *tab = find(contents); return tab ? std::make_optional(describe(tab)) : std::nullopt; }
QVariantList arbiterExtensionMenuItems(WebContentsAdapter *adapter, const QVariantMap &data) {
    QVariantList output;
    auto *tab = find(adapter->webContents()); if (!tab || data.value(QStringLiteral("pageUrl")).toString().toStdString() != tab->web_contents()->GetLastCommittedURL().spec()) return output;
    auto *ui = ProfileUi::get(tab->web_contents()->GetBrowserContext());
    for (const auto &[extensionId, entries] : ui->menus) {
        const auto *extension = ui->registry ? ui->registry->enabled_extensions().GetByID(extensionId) : nullptr; if (!extension) continue;
        for (const auto &entry : entries) {
            if (!menuMatches(entry, data)) continue;
            bool container = false;
            for (const auto &other : entries) if (const auto *parent = other.properties.Find("parentId"); parent && menuKey(*parent) == entry.key) container = true;
            if (container) continue;
            QString title = QString::fromStdString(entry.properties.FindString("title") ? *entry.properties.FindString("title") : "");
            bool enabled = entry.properties.FindBool("enabled").value_or(true), visible = true;
            const MenuEntry *ancestor = &entry;
            for (size_t depth = 0; depth < entries.size(); ++depth) {
                const auto *parentId = ancestor->properties.Find("parentId"); if (!parentId) break;
                const auto key = menuKey(*parentId);
                auto parent = std::find_if(entries.begin(), entries.end(), [&](const auto &candidate) { return candidate.key == key; });
                if (parent == entries.end() || !menuMatches(*parent, data)) { visible = false; break; }
                enabled &= parent->properties.FindBool("enabled").value_or(true);
                if (const auto *label = parent->properties.FindString("title")) title.prepend(QString::fromStdString(*label) + QStringLiteral(" › "));
                ancestor = &*parent;
            }
            if (!visible) continue;
            title.replace(QStringLiteral("%s"), data.value(QStringLiteral("selectionText")).toString().left(80));
            title.prepend(QString::fromStdString(extension->name()) + QStringLiteral(" · "));
            const auto *type = entry.properties.FindString("type"); const std::string kind = type ? *type : "normal";
            output.append(QVariantMap{{QStringLiteral("key"), QString::fromStdString(extensionId + "/" + entry.key)}, {QStringLiteral("title"), title},
                {QStringLiteral("enabled"), enabled}, {QStringLiteral("separator"), kind == "separator"},
                {QStringLiteral("checkable"), kind == "checkbox" || kind == "radio"}, {QStringLiteral("checked"), entry.properties.FindBool("checked").value_or(false)}});
        }
    }
    return output;
}
void arbiterInvokeExtensionMenu(WebContentsAdapter *adapter, const QString &key, const QVariantMap &data) {
    auto *tab = find(adapter->webContents()); if (!tab) return;
    bool allowed = false;
    for (const auto &value : arbiterExtensionMenuItems(adapter, data)) {
        const auto item = value.toMap(); if (item.value(QStringLiteral("key")).toString() == key && item.value(QStringLiteral("enabled")).toBool()) allowed = true;
    }
    if (!allowed) return;
    const auto split = key.indexOf(u'/'); if (split < 0) return;
    const auto extensionId = key.left(split).toStdString(), itemKey = key.mid(split + 1).toStdString();
    auto *ui = ProfileUi::get(tab->web_contents()->GetBrowserContext());
    auto &entries = ui->menus[extensionId];
    auto item = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) { return entry.key == itemKey; }); if (item == entries.end()) return;
    const auto *extension = ui->registry->enabled_extensions().GetByID(extensionId); if (!extension) return;
    arbiterGrantActiveTab(adapter, QString::fromStdString(extensionId));
    base::Value::Dict click;
    click.Set("menuItemId", item->id.Clone());
    if (const auto *parent = item->properties.Find("parentId")) click.Set("parentMenuItemId", parent->Clone());
    for (const auto *field : {"pageUrl", "linkUrl", "srcUrl", "selectionText", "mediaType"}) {
        const auto value = data.value(QString::fromLatin1(field)).toString(); if (!value.isEmpty()) click.Set(field, value.toStdString());
    }
    click.Set("editable", data.value(QStringLiteral("editable")).toBool()); click.Set("frameId", 0);
    const auto *type = item->properties.FindString("type");
    if (type && (*type == "checkbox" || *type == "radio")) {
        const bool wasChecked = item->properties.FindBool("checked").value_or(false);
        const bool checked = *type == "radio" || !wasChecked;
        item->properties.Set("checked", checked); click.Set("wasChecked", wasChecked); click.Set("checked", checked);
        if (*type == "radio") {
            auto parentKey = [](const MenuEntry &entry) { const auto *parent = entry.properties.Find("parentId"); return parent ? menuKey(*parent) : ""; };
            const auto parent = parentKey(*item); auto index = std::distance(entries.begin(), item);
            for (int direction : {-1, 1}) for (int i = int(index) + direction; i >= 0 && i < int(entries.size()); i += direction) {
                auto &other = entries[i]; const auto *otherType = other.properties.FindString("type");
                if (!otherType || *otherType != "radio" || parentKey(other) != parent) break;
                other.properties.Set("checked", false);
            }
        }
    }
    for (bool internal : {true, false}) {
        base::Value::List args; args.Append(click.Clone()); args.Append(describe(tab, extension));
        auto event = std::make_unique<extensions::Event>(internal ? extensions::events::CONTEXT_MENUS : extensions::events::CONTEXT_MENUS_ON_CLICKED,
            internal ? "contextMenus" : "contextMenus.onClicked", std::move(args), tab->web_contents()->GetBrowserContext());
        event->user_gesture = extensions::EventRouter::UserGestureState::kEnabled;
        extensions::EventRouter::Get(tab->web_contents()->GetBrowserContext())->DispatchEventToExtension(extensionId, std::move(event));
    }
}
void arbiterGrantActiveTab(WebContentsAdapter *adapter, const QString &id) {
    auto *tab = find(adapter->webContents()); if (!tab || !tab->active()) return;
    const auto *extension = extensions::ExtensionRegistry::Get(tab->web_contents()->GetBrowserContext())->enabled_extensions().GetByID(id.toStdString());
    if (!extension || !extension->permissions_data()->HasAPIPermission(extensions::mojom::APIPermissionID::kActiveTab)) return;
    const auto &url = tab->web_contents()->GetLastCommittedURL(); if (!url.SchemeIsHTTPOrHTTPS()) return;
    extensions::URLPatternSet hosts;
    URLPattern pattern(URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS);
    if (pattern.Parse(url.DeprecatedGetOriginAsURL().spec() + "*") != URLPattern::ParseResult::kSuccess) return;
    hosts.AddPattern(pattern);
    extensions::APIPermissionSet apis; apis.insert(extensions::mojom::APIPermissionID::kTab);
    extensions::PermissionSet permissions(std::move(apis), extensions::ManifestPermissionSet(), hosts.Clone(), hosts.Clone());
    extension->permissions_data()->UpdateTabSpecificPermissions(tab->id, permissions);
    tab->grants[id.toStdString()] = url;
    auto *helper = extensions::RendererStartupHelperFactory::GetForBrowserContext(tab->web_contents()->GetBrowserContext());
    for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd(); it.Advance()) {
        auto *process = it.GetCurrentValue();
        if (process->GetBrowserContext() == tab->web_contents()->GetBrowserContext())
            if (auto *renderer = helper->GetRenderer(process)) renderer->UpdateTabSpecificPermissions(extension->id(), hosts.Clone(), tab->id, false);
    }
}

namespace {
class ArbiterFunction : public ExtensionFunction {
public:
    ResponseAction Run() override;
    base::Value::Dict injection;
    base::Value::Dict creation;
    void inject(std::vector<extensions::scripting::InjectedFileSource> files, std::optional<std::string> error);
    void injected(std::vector<extensions::ScriptExecutor::FrameResult> results);
    void created(int id, int attempt = 0);
    void captured(int id, const GURL &url, std::string format, int quality, const SkBitmap &bitmap);
protected:
    ~ArbiterFunction() override = default;
};
#define FUNCTION(Class, Api, Histogram) class Class : public ArbiterFunction { public: DECLARE_EXTENSION_FUNCTION(Api, Histogram) };
FUNCTION(ArbiterTabsQuery, "tabs.query", TABS_QUERY)
FUNCTION(ArbiterTabsGet, "tabs.get", TABS_GET)
FUNCTION(ArbiterTabsGetCurrent, "tabs.getCurrent", TABS_GETCURRENT)
FUNCTION(ArbiterTabsGetSelected, "tabs.getSelected", TABS_GETSELECTED)
FUNCTION(ArbiterTabsGetAllInWindow, "tabs.getAllInWindow", TABS_GETALLINWINDOW)
FUNCTION(ArbiterTabsCreate, "tabs.create", TABS_CREATE)
FUNCTION(ArbiterTabsUpdate, "tabs.update", TABS_UPDATE)
FUNCTION(ArbiterTabsRemove, "tabs.remove", TABS_REMOVE)
FUNCTION(ArbiterTabsReload, "tabs.reload", TABS_RELOAD)
FUNCTION(ArbiterTabsGoBack, "tabs.goBack", TABS_GOBACK)
FUNCTION(ArbiterTabsGoForward, "tabs.goForward", TABS_GOFORWARD)
FUNCTION(ArbiterTabsGetZoom, "tabs.getZoom", TABS_GETZOOM)
FUNCTION(ArbiterTabsSetZoom, "tabs.setZoom", TABS_SETZOOM)
FUNCTION(ArbiterTabsCapture, "tabs.captureVisibleTab", TABS_CAPTUREVISIBLETAB)
FUNCTION(ArbiterWindowsGet, "windows.get", WINDOWS_GET)
FUNCTION(ArbiterWindowsGetCurrent, "windows.getCurrent", WINDOWS_GETCURRENT)
FUNCTION(ArbiterWindowsGetLastFocused, "windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
FUNCTION(ArbiterWindowsGetAll, "windows.getAll", WINDOWS_GETALL)
FUNCTION(ArbiterWindowsUpdate, "windows.update", WINDOWS_UPDATE)
FUNCTION(ArbiterWindowsRemove, "windows.remove", WINDOWS_REMOVE)
FUNCTION(ArbiterScriptingExecute, "scripting.executeScript", SCRIPTING_EXECUTESCRIPT)
FUNCTION(ArbiterScriptingInsertCSS, "scripting.insertCSS", SCRIPTING_INSERTCSS)
FUNCTION(ArbiterScriptingRemoveCSS, "scripting.removeCSS", SCRIPTING_REMOVECSS)
FUNCTION(ArbiterActionGetBadge, "action.getBadgeText", ACTION_GETBADGETEXT)
FUNCTION(ArbiterActionSetBadge, "action.setBadgeText", ACTION_SETBADGETEXT)
FUNCTION(ArbiterActionGetTitle, "action.getTitle", ACTION_GETTITLE)
FUNCTION(ArbiterActionSetTitle, "action.setTitle", ACTION_SETTITLE)
FUNCTION(ArbiterCommandsGetAll, "commands.getAll", COMMANDS_GETALL)
FUNCTION(ArbiterMenusCreate, "contextMenus.create", CONTEXTMENUS_CREATE)
FUNCTION(ArbiterMenusUpdate, "contextMenus.update", CONTEXTMENUS_UPDATE)
FUNCTION(ArbiterMenusRemove, "contextMenus.remove", CONTEXTMENUS_REMOVE)
FUNCTION(ArbiterMenusRemoveAll, "contextMenus.removeAll", CONTEXTMENUS_REMOVEALL)
FUNCTION(ArbiterPermissionsContains, "permissions.contains", PERMISSIONS_CONTAINS)
FUNCTION(ArbiterPermissionsGetAll, "permissions.getAll", PERMISSIONS_GETALL)
FUNCTION(ArbiterPermissionsRequest, "permissions.request", PERMISSIONS_REQUEST)
FUNCTION(ArbiterActionGetBadgeColor, "action.getBadgeBackgroundColor", ACTION_GETBADGEBACKGROUNDCOLOR)
FUNCTION(ArbiterActionSetBadgeColor, "action.setBadgeBackgroundColor", ACTION_SETBADGEBACKGROUNDCOLOR)
FUNCTION(ArbiterActionGetTextColor, "action.getBadgeTextColor", ACTION_GETBADGETEXTCOLOR)
FUNCTION(ArbiterActionSetTextColor, "action.setBadgeTextColor", ACTION_SETBADGETEXTCOLOR)
#undef FUNCTION
ExtensionFunction::ResponseAction ArbiterFunction::Run() {
    const std::string method = name();
    const auto &input = args();
    const base::Value::Dict empty;
    const auto &object = !input.empty() && input[0].is_dict() ? input[0].GetDict() : empty;
    const int firstId = !input.empty() && input[0].is_int() ? input[0].GetInt() : -1;
    auto result = [this](base::Value value) { return RespondNow(WithArguments(std::move(value))); };
    auto error = [this](std::string value) { return RespondNow(Error(std::move(value))); };
    if ((method == "tabs.get" || method == "tabs.remove") && (input.empty() || (method == "tabs.get" ? !input[0].is_int() || firstId < 0 : !input[0].is_int() && !input[0].is_list()))) return error("A valid tab ID is required.");
    if (method == "tabs.remove" && input[0].is_int() && firstId < 0) return error("Invalid tab ID.");
    if ((method == "tabs.query" || method == "tabs.create" || method.starts_with("scripting.") || method.starts_with("permissions.") && method != "permissions.getAll" || method.starts_with("action.set")) && (input.empty() || !input[0].is_dict())) return error("A properties object is required.");
    if (method == "contextMenus.create" && (input.empty() || !input[0].is_dict())) return error("A menu properties object is required.");
    if (method == "contextMenus.update" && (input.size() < 2 || !input[1].is_dict())) return error("Menu update properties are required.");
    if (method == "commands.getAll") {
        base::Value::List values;
        if (const auto *commands = extension()->manifest()->FindDictPath("commands")) for (const auto [name, value] : *commands) if (value.is_dict()) {
            base::Value::Dict command; command.Set("name", name);
            const auto *description = value.GetDict().FindString("description"); command.Set("description", description ? *description : "");
            command.Set("shortcut", name.starts_with("_") ? "" : commandKey(value.GetDict()).toStdString()); values.Append(std::move(command));
        }
        return result(base::Value(std::move(values)));
    }
    if (method.starts_with("permissions.")) {
        const auto &permissions = extension()->permissions_data()->active_permissions();
        if (method == "permissions.getAll") {
            base::Value::List names; for (const auto &name : permissions.GetAPIsAsStrings()) names.Append(name);
            return result(base::Value(base::Value::Dict().Set("permissions", std::move(names)).Set("origins", permissions.explicit_hosts().ToValue())));
        }
        bool approved = true;
        if (const auto *names = object.FindList("permissions")) for (const auto &permission : *names)
            approved &= permission.is_string() && extension()->permissions_data()->HasAPIPermission(permission.GetString());
        if (const auto *origins = object.FindList("origins")) for (const auto &origin : *origins) {
            URLPattern pattern(URLPattern::SCHEME_ALL);
            approved &= origin.is_string() && pattern.Parse(origin.GetString()) == URLPattern::ParseResult::kSuccess && permissions.explicit_hosts().ContainsPattern(pattern);
        }
        if (method == "permissions.request" && !approved) return error("Additional optional permissions require browser support for a new permission review; no access was granted.");
        return result(base::Value(approved));
    }
    if (method.starts_with("contextMenus.")) {
        auto &entries = ProfileUi::get(browser_context())->menus[extension()->id()];
        if (method == "contextMenus.removeAll") { entries.clear(); return RespondNow(NoArguments()); }
        const auto *id = method == "contextMenus.create" ? object.Find("id") : !input.empty() ? &input[0] : nullptr;
        if (!id && method == "contextMenus.create") id = object.Find("generatedId");
        if (!id || menuKey(*id).empty()) return error("A valid menu ID is required.");
        const auto key = menuKey(*id);
        auto found = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) { return entry.key == key; });
        if (method == "contextMenus.remove") {
            if (found == entries.end()) return error("The menu item does not exist.");
            std::set<std::string> removing{key};
            for (size_t pass = 0; pass < entries.size(); ++pass) for (const auto &entry : entries)
                if (const auto *parent = entry.properties.Find("parentId"); parent && removing.count(menuKey(*parent))) removing.insert(entry.key);
            std::erase_if(entries, [&](const auto &entry) { return removing.count(entry.key); });
            return RespondNow(NoArguments());
        }
        const bool create = method == "contextMenus.create";
        if (create && found != entries.end()) return error("A menu item with this ID already exists.");
        if (!create && found == entries.end()) return error("The menu item does not exist.");
        if (create && entries.size() >= 500) return error("Too many extension menu items.");
        auto properties = create ? object.Clone() : found->properties.Clone();
        if (!create && input.size() > 1 && input[1].is_dict()) properties.Merge(input[1].GetDict().Clone());
        const auto *type = properties.FindString("type");
        if ((!type || *type != "separator") && !properties.FindString("title")) return error("A menu title is required.");
        if (const auto *parent = properties.Find("parentId")) {
            auto ancestor = menuKey(*parent); std::set<std::string> seen{key};
            while (!ancestor.empty()) {
                if (!seen.insert(ancestor).second) return error("Menu parent cycle is not allowed.");
                auto owner = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) { return entry.key == ancestor; });
                if (owner == entries.end()) return error("The parent menu does not exist.");
                const auto *next = owner->properties.Find("parentId"); ancestor = next ? menuKey(*next) : "";
            }
        }
        for (const auto *field : {"documentUrlPatterns", "targetUrlPatterns"}) if (const auto *patterns = properties.FindList(field))
            for (const auto &value : *patterns) { URLPattern pattern(URLPattern::SCHEME_ALL); if (!value.is_string() || pattern.Parse(value.GetString()) != URLPattern::ParseResult::kSuccess) return error("Invalid menu URL pattern."); }
        if (create) entries.push_back(MenuEntry{key, id->Clone(), std::move(properties)}); else found->properties = std::move(properties);
        return RespondNow(NoArguments());
    }
    if (method.starts_with("action.")) {
        const auto id = object.FindInt("tabId").value_or(extensions::ExtensionAction::kDefaultTabId);
        if (id >= 0 && !target(browser_context(), id)) return error("No tab with the specified ID exists in this profile.");
        auto *action = extensions::ExtensionActionManager::Get(browser_context())->GetExtensionAction(*extension());
        if (!action) return error("The extension does not define an action.");
        if (method.find("Color") != std::string::npos) {
            const bool textColor = method.find("TextColor") != std::string::npos;
            if (method.find(".set") != std::string::npos) {
                QColor color;
                if (const auto *value = object.FindString("color")) color = QColor(QString::fromStdString(*value));
                else if (const auto *value = object.FindList("color"); value && value->size() == 4) {
                    for (const auto &component : *value) if (!component.is_int() || component.GetInt() < 0 || component.GetInt() > 255) return error("Invalid RGBA color component.");
                    color = QColor((*value)[0].GetInt(), (*value)[1].GetInt(), (*value)[2].GetInt(), (*value)[3].GetInt());
                }
                if (!color.isValid()) return error("Unsupported badge color; use a CSS hex/name or RGBA array.");
                const auto rgba = SkColorSetARGB(color.alpha(), color.red(), color.green(), color.blue());
                if (textColor) action->SetBadgeTextColor(id, rgba); else action->SetBadgeBackgroundColor(id, rgba);
                return RespondNow(NoArguments());
            }
            const auto color = textColor ? action->GetBadgeTextColor(id) : action->GetBadgeBackgroundColor(id);
            base::Value::List values; values.Append(int(SkColorGetR(color))); values.Append(int(SkColorGetG(color))); values.Append(int(SkColorGetB(color))); values.Append(int(SkColorGetA(color)));
            return result(base::Value(std::move(values)));
        }
        if (method == "action.getBadgeText") return result(base::Value(action->GetExplicitlySetBadgeText(id)));
        if (method == "action.getTitle") return result(base::Value(action->GetTitle(id)));
        if (method == "action.setBadgeText") { const auto *text = object.FindString("text"); action->SetBadgeText(id, text ? *text : ""); }
        if (method == "action.setTitle") { const auto *text = object.FindString("title"); if (!text) return error("A title is required."); action->SetTitle(id, *text); }
        return RespondNow(NoArguments());
    }
    if (method.starts_with("scripting.")) {
        injection = object.Clone();
        const auto *targetInfo = injection.FindDict("target");
        if (!targetInfo || !targetInfo->FindInt("tabId")) return error("A target tab ID is required.");
        const auto *files = injection.FindList("files");
        const bool javascript = method == "scripting.executeScript";
        const auto *code = javascript ? injection.FindString("func") : injection.FindString("css");
        if (javascript && !code) code = injection.FindString("function");
        if (bool(files) == bool(code)) return error("Specify exactly one of files and code.");
        if (javascript && files && injection.contains("args")) return error("Arguments require an injected function.");
        if (files) {
            std::vector<std::string> paths;
            for (const auto &file : *files) { if (!file.is_string()) return error("Invalid script filename."); paths.push_back(file.GetString()); }
            if (paths.empty()) return error("At least one file is required.");
            std::string reason;
            if (!extensions::scripting::CheckAndLoadFiles(std::move(paths), javascript ? extensions::script_parsing::ContentScriptType::kJs : extensions::script_parsing::ContentScriptType::kCss,
                *extension(), !javascript, base::BindOnce(&ArbiterFunction::inject, this), &reason)) return error(reason);
            return RespondLater();
        }
        inject({}, std::nullopt);
        return did_respond() ? AlreadyResponded() : RespondLater();
    }
    if (method.starts_with("windows.")) {
        const auto all = available(browser_context()); Tab *tab = nullptr;
        for (auto *candidate : all) {
            if (firstId >= 0 ? candidate->windowId == firstId : candidate->window == lastWindow) { tab = candidate; break; }
        }
        if (!tab && firstId < 0 && !all.empty()) tab = all.front();
        const auto *options = input.size() > 1 && input[1].is_dict() ? &input[1].GetDict() : &object;
        const bool populate = options->FindBool("populate").value_or(false);
        if (method == "windows.update") {
            for (const auto [key, value] : *options) if (key != "focused" && key != "state" && key != "left" && key != "top" && key != "width" && key != "height") return error("Unsupported windows.update option: " + key);
            if (options->FindInt("width").value_or(1) < 1 || options->FindInt("height").value_or(1) < 1) return error("Window dimensions must be positive.");
        }
        if (method == "windows.getAll") {
            base::Value::List values; std::set<int> ids;
            for (auto *candidate : all) if (ids.insert(candidate->windowId).second) values.Append(describeWindow(candidate, extension(), populate));
            return result(base::Value(std::move(values)));
        }
        if (!tab) return error("No window with the specified ID exists in this profile.");
        if (method == "windows.update") {
            auto *w = tab->window.data();
            if (options->contains("drawAttention")) return error("Window attention requests are not supported.");
            if (const auto *state = options->FindString("state")) {
                if (*state == "minimized") w->showMinimized(); else if (*state == "maximized") w->showMaximized();
                else if (*state == "fullscreen") w->showFullScreen(); else if (*state == "normal") w->showNormal();
                else return error("Unsupported window state.");
            }
            w->setX(options->FindInt("left").value_or(w->x())); w->setY(options->FindInt("top").value_or(w->y()));
            w->resize(options->FindInt("width").value_or(w->width()), options->FindInt("height").value_or(w->height()));
            if (options->FindBool("focused").value_or(false)) { w->raise(); w->requestActivate(); lastWindow = w; }
        }
        if (method == "windows.remove") { QMetaObject::invokeMethod(tab->window.data(), "close", Qt::QueuedConnection); return RespondNow(NoArguments()); }
        return result(base::Value(describeWindow(tab, extension(), populate)));
    }
    if (method == "tabs.getCurrent") {
        auto *tab = find(GetSenderWebContents()); return tab ? result(base::Value(describe(tab, extension()))) : RespondNow(NoArguments());
    }
    if (method == "tabs.query" || method == "tabs.getAllInWindow") {
        base::Value::List values;
        for (auto *tab : available(browser_context())) {
            auto info = describe(tab, extension()); bool match = true;
            if (method == "tabs.getAllInWindow" && (firstId >= 0 ? tab->windowId != firstId : tab->window != lastWindow)) continue;
            for (const char *key : {"active", "pinned", "audible", "highlighted", "incognito"})
                if (auto expected = object.FindBool(key); expected && info.FindBool(key) != expected) match = false;
            if (auto id = object.FindInt("windowId"); id && (*id >= 0 ? tab->windowId != *id : tab->window != lastWindow)) match = false;
            for (const char *key : {"currentWindow", "lastFocusedWindow"})
                if (auto expected = object.FindBool(key); expected && (tab->window == lastWindow) != *expected) match = false;
            if (auto index = object.FindInt("index"); index && info.FindInt("index") != index) match = false;
            if (const auto *status = object.FindString("status"); status && *info.FindString("status") != *status) match = false;
            if (const auto *urls = object.Find("url")) {
                const auto *url = info.FindString("url"); bool urlMatch = false;
                auto matches = [&](const base::Value &value) {
                    if (!value.is_string() || !url) return false;
                    URLPattern pattern(URLPattern::SCHEME_ALL);
                    return pattern.Parse(value.GetString()) == URLPattern::ParseResult::kSuccess && pattern.MatchesURL(GURL(*url));
                };
                if (urls->is_string()) urlMatch = matches(*urls);
                else if (urls->is_list()) for (const auto &value : urls->GetList()) urlMatch |= matches(value);
                if (!urlMatch) match = false;
            }
            // Reject rather than silently ignore unsupported filters.
            for (const auto [key, value] : object)
                if (key != "active" && key != "pinned" && key != "audible" && key != "highlighted" && key != "incognito" && key != "windowId" && key != "currentWindow" && key != "lastFocusedWindow" && key != "index" && key != "status" && key != "url")
                    return error("Unsupported tabs.query filter: " + key);
            if (match) values.Append(std::move(info));
        }
        return result(base::Value(std::move(values)));
    }
    if (method == "tabs.create") {
        auto *parent = target(browser_context(), -1, object.FindInt("windowId").value_or(-2));
        if (!parent) return error("No active browser tab exists in this profile.");
        const auto *requested = object.FindString("url");
        GURL url = requested ? extension()->GetResourceURL(*requested) : GURL("about:blank");
        if (requested && GURL(*requested).is_valid()) url = GURL(*requested);
        if (!(url.SchemeIsHTTPOrHTTPS() || url == GURL("about:blank") || (url.SchemeIs("chrome-extension") && url.host() == extension()->id())))
            return error("Cannot create a tab with this restricted URL.");
        for (const auto [key, value] : object)
            if (key != "url" && key != "active" && key != "selected" && key != "windowId" && key != "pinned") return error("Unsupported tabs.create option: " + key);
        creation = object.Clone();
        int id = -1;
        const bool invoked = QMetaObject::invokeMethod(parent->context, "createTab", Q_RETURN_ARG(int, id), Q_ARG(QString, QString::fromStdString(url.spec())), Q_ARG(bool, object.FindBool("active").value_or(object.FindBool("selected").value_or(true))));
        if (!invoked || id <= 0) return error("Browser could not create the tab.");
        created(id);
        return did_respond() ? AlreadyResponded() : RespondLater();
    }
    int id = firstId;
    if (method == "tabs.update" && (input.empty() || input[0].is_none())) id = -1;
    auto *tab = target(browser_context(), id);
    if (method == "tabs.getSelected" || method == "tabs.captureVisibleTab") tab = target(browser_context(), -1, firstId >= 0 ? firstId : -2);
    if (!tab) return error("No tab with the specified ID exists in this profile.");
    WebContents *wc = tab->web_contents();
    if (method == "tabs.get" || method == "tabs.getSelected") return result(base::Value(describe(tab, extension())));
    if (method == "tabs.update") {
        const auto *options = input.size() > 1 && input[1].is_dict() ? &input[1].GetDict() : &object;
        for (const auto [key, value] : *options)
            if (key != "url" && key != "active" && key != "highlighted" && key != "muted" && key != "pinned") return error("Unsupported tabs.update option: " + key);
        if (const auto *url = options->FindString("url")) {
            GURL destination(*url); if (!destination.is_valid()) destination = extension()->GetResourceURL(*url);
            if (!(destination.SchemeIsHTTPOrHTTPS() || destination == GURL("about:blank") || (destination.SchemeIs("chrome-extension") && destination.host() == extension()->id()))) return error("Cannot navigate to this restricted URL.");
            tab->adapter->load(QUrl(QString::fromStdString(destination.spec())));
        }
        if (auto muted = options->FindBool("muted")) wc->SetAudioMuted(*muted);
        if (auto pinned = options->FindBool("pinned")) QMetaObject::invokeMethod(tab->context, "setPinned", Q_ARG(bool, *pinned));
        if (options->FindBool("active").value_or(false) || options->FindBool("highlighted").value_or(false)) QMetaObject::invokeMethod(tab->context, "activate");
        return result(base::Value(describe(tab, extension())));
    }
    if (method == "tabs.remove") {
        std::vector<QPointer<QObject>> targets;
        if (!input.empty() && input[0].is_list()) for (const auto &value : input[0].GetList()) {
            auto *other = value.is_int() ? target(browser_context(), value.GetInt()) : nullptr;
            if (!other) return error("No tab with the specified ID exists in this profile."); targets.push_back(other->context);
        } else targets.push_back(tab->context);
        for (auto &context : targets) if (context) QMetaObject::invokeMethod(context, "closeTab", Qt::QueuedConnection);
        return RespondNow(NoArguments());
    }
    if (method == "tabs.reload") {
        const auto *options = input.size() > 1 && input[1].is_dict() ? &input[1].GetDict() : &object;
        if (options->FindBool("bypassCache").value_or(false)) tab->adapter->reloadAndBypassCache(); else tab->adapter->reload();
        return RespondNow(NoArguments());
    }
    if (method == "tabs.goBack" || method == "tabs.goForward") {
        const bool back = method == "tabs.goBack";
        if (back ? !tab->adapter->canGoBack() : !tab->adapter->canGoForward()) return error("No history entry exists in that direction.");
        if (back) tab->adapter->navigateBack(); else tab->adapter->navigateForward(); return RespondNow(NoArguments());
    }
    if (method == "tabs.getZoom") return result(base::Value(tab->adapter->currentZoomFactor()));
    if (method == "tabs.setZoom") {
        if (input.empty()) return error("A zoom factor is required.");
        const auto &factor = input.back(); if (!factor.is_double() && !factor.is_int()) return error("Invalid zoom factor.");
        const double zoom = factor.GetDouble(); if (zoom != 0 && (zoom < 0.25 || zoom > 5)) return error("Zoom factor must be between 0.25 and 5.");
        tab->adapter->setZoomFactor(zoom == 0 ? 1 : zoom); return RespondNow(NoArguments());
    }
    if (method == "tabs.captureVisibleTab") {
        std::string reason;
        if (!access(extension(), tab, &reason) || !extension()->permissions_data()->CanCaptureVisiblePage(wc->GetLastCommittedURL(), tab->id, &reason, extensions::CaptureRequirement::kActiveTabOrAllUrls)) return error(reason);
        if (!tab->adapter->isVisible() || !tab->window->isVisible() || tab->window->windowState() == Qt::WindowMinimized) return error("The active page is not visible.");
        auto &times = tab->captures[extension()->id()]; const auto now = base::TimeTicks::Now();
        while (!times.empty() && now - times.front() >= base::Seconds(1)) times.pop_front();
        if (times.size() >= 2) return error("MAX_CAPTURE_VISIBLE_TAB_CALLS_PER_SECOND quota exceeded.");
        const auto *options = input.size() > 1 && input[1].is_dict() ? &input[1].GetDict() : &object;
        const auto *format = options->FindString("format"); const std::string encoding = format ? *format : "jpeg";
        const int quality = options->FindInt("quality").value_or(92);
        if ((encoding != "png" && encoding != "jpeg") || quality < 0 || quality > 100) return error("Invalid screenshot format or quality.");
        auto *view = wc->GetRenderWidgetHostView(); if (!view) return error("The page surface is unavailable."); times.push_back(now);
        view->CopyFromSurface(gfx::Rect(), gfx::Size(), base::BindOnce(&ArbiterFunction::captured, this, tab->id, wc->GetLastCommittedURL(), encoding, quality));
        return RespondLater();
    }
    return error("This extension operation is not supported.");
}
void ArbiterFunction::created(int id, int attempt) {
    if (!browser_context()) { Respond(Error("Browser profile was closed.")); return; }
    if (auto *tab = target(browser_context(), id)) {
        if (auto pinned = creation.FindBool("pinned")) QMetaObject::invokeMethod(tab->context, "setPinned", Q_ARG(bool, *pinned));
        Respond(WithArguments(describe(tab, extension()))); return;
    }
    if (attempt >= 500) { Respond(Error("Browser could not initialize the new tab.")); return; }
    QTimer::singleShot(10, [self = scoped_refptr<ArbiterFunction>(this), id, attempt] { self->created(id, attempt + 1); });
}
void ArbiterFunction::inject(std::vector<extensions::scripting::InjectedFileSource> files, std::optional<std::string> loadError) {
    if (loadError) { Respond(Error(*loadError)); return; }
    const auto *targetInfo = injection.FindDict("target");
    auto *tab = target(browser_context(), targetInfo->FindInt("tabId").value_or(-1));
    std::string reason;
    if (!tab || !access(extension(), tab, &reason)) { Respond(Error(reason.empty() ? "Target tab is unavailable." : reason)); return; }
    extensions::scripting::InjectionTarget injectionTarget;
    injectionTarget.tab_id = tab->id;
    injectionTarget.all_frames = targetInfo->FindBool("allFrames");
    if (const auto *frames = targetInfo->FindList("frameIds")) {
        injectionTarget.frame_ids.emplace();
        for (const auto &frame : *frames) { if (!frame.is_int()) { Respond(Error("Invalid frame ID.")); return; } injectionTarget.frame_ids->push_back(frame.GetInt()); }
    }
    if (const auto *documents = targetInfo->FindList("documentIds")) {
        injectionTarget.document_ids.emplace();
        for (const auto &document : *documents) { if (!document.is_string()) { Respond(Error("Invalid document ID.")); return; } injectionTarget.document_ids->push_back(document.GetString()); }
    }
    extensions::ScriptExecutor *executor = nullptr;
    extensions::ScriptExecutor::FrameScope scope = extensions::ScriptExecutor::SPECIFIED_FRAMES;
    std::set<int> frameIds;
    if (!extensions::scripting::CanAccessTarget(*extension()->permissions_data(), injectionTarget, browser_context(), false, &executor, &scope, &frameIds, &reason)) {
        Respond(Error(reason)); return;
    }
    using namespace extensions;
    if (name() == "scripting.executeScript") {
        std::vector<mojom::JSSourcePtr> sources;
        for (auto &file : files) sources.push_back(mojom::JSSource::New(std::move(*file.data), extension()->GetResourceURL(file.file_name)));
        if (files.empty()) {
            const auto *function = injection.FindString("func"); if (!function) function = injection.FindString("function");
            if (!function) { Respond(Error("An injected function is required.")); return; }
            std::string arguments;
            if (const auto *args = injection.FindList("args")) for (const auto &arg : *args) {
                auto encoded = base::WriteJson(arg); if (!encoded) { Respond(Error("Unserializable argument.")); return; }
                if (!arguments.empty()) arguments += ","; arguments += *encoded;
            }
            sources.push_back(mojom::JSSource::New("(" + *function + ")(" + arguments + ")", GURL()));
        }
        const auto *world = injection.FindString("world");
        scripting::ExecuteScript(extension()->id(), std::move(sources), world && *world == "MAIN" ? mojom::ExecutionWorld::kMain : mojom::ExecutionWorld::kIsolated,
            std::nullopt, executor, scope, std::move(frameIds), injection.FindBool("injectImmediately").value_or(false), user_gesture(), base::BindOnce(&ArbiterFunction::injected, this));
    } else {
        mojom::HostID host(mojom::HostID::HostType::kExtensions, extension()->id());
        std::vector<mojom::CSSSourcePtr> sources;
        for (auto &file : files) {
            const auto url = extension()->GetResourceURL(file.file_name);
            sources.push_back(mojom::CSSSource::New(std::move(*file.data), ScriptExecutor::GenerateInjectionKey(host, url, "")));
        }
        if (files.empty()) {
            const auto *css = injection.FindString("css"); if (!css) { Respond(Error("CSS is required.")); return; }
            sources.push_back(mojom::CSSSource::New(*css, ScriptExecutor::GenerateInjectionKey(host, GURL(), *css)));
        }
        const auto *origin = injection.FindString("origin");
        executor->ExecuteScript(host, mojom::CodeInjection::NewCss(mojom::CSSInjection::New(std::move(sources), origin && *origin == "USER" ? mojom::CSSOrigin::kUser : mojom::CSSOrigin::kAuthor,
            name() == "scripting.removeCSS" ? mojom::CSSInjection::Operation::kRemove : mojom::CSSInjection::Operation::kAdd)),
            scope, frameIds, mojom::MatchOriginAsFallbackBehavior::kAlways, mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS, GURL(), base::BindOnce(&ArbiterFunction::injected, this));
    }
}
void ArbiterFunction::injected(std::vector<extensions::ScriptExecutor::FrameResult> results) {
    if (results.empty()) { Respond(Error("No frames responded to the injection.")); return; }
    if (results.size() == 1 && !results[0].error.empty()) { Respond(Error(results[0].error)); return; }
    if (name() != "scripting.executeScript") { Respond(NoArguments()); return; }
    std::stable_sort(results.begin(), results.end(), [](const auto &a, const auto &b) { return a.frame_id == 0 && b.frame_id != 0; });
    base::Value::List values;
    for (auto &frame : results) if (frame.error.empty() && frame.frame_responded) {
        base::Value::Dict value; value.Set("frameId", frame.frame_id); value.Set("documentId", frame.document_id.ToString()); value.Set("result", std::move(frame.value)); values.Append(std::move(value));
    }
    Respond(WithArguments(std::move(values)));
}
void ArbiterFunction::captured(int id, const GURL &url, std::string format, int quality, const SkBitmap &bitmap) {
    auto *tab = target(browser_context(), id); std::string reason;
    if (!tab || tab->web_contents()->GetLastCommittedURL() != url || !access(extension(), tab, &reason)) { Respond(Error("Page changed or access was revoked during capture.")); return; }
    if (bitmap.drawsNothing()) { Respond(Error("The page returned an empty screenshot.")); return; }
    auto encoded = format == "png" ? gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false) : gfx::JPEGCodec::Encode(bitmap, quality);
    if (!encoded) { Respond(Error("Could not encode the screenshot.")); return; }
    Respond(WithArguments("data:image/" + format + ";base64," + base::Base64Encode(*encoded)));
}
}
void arbiterRegisterExtensionFunctions(ExtensionFunctionRegistry *registry) {
#define REGISTER(Type) registry->RegisterFunction<Type>();
    REGISTER(ArbiterTabsQuery) REGISTER(ArbiterTabsGet) REGISTER(ArbiterTabsGetCurrent)
    REGISTER(ArbiterTabsGetSelected) REGISTER(ArbiterTabsGetAllInWindow) REGISTER(ArbiterTabsCreate)
    REGISTER(ArbiterTabsUpdate) REGISTER(ArbiterTabsRemove) REGISTER(ArbiterTabsReload)
    REGISTER(ArbiterTabsGoBack) REGISTER(ArbiterTabsGoForward) REGISTER(ArbiterTabsGetZoom)
    REGISTER(ArbiterTabsSetZoom) REGISTER(ArbiterTabsCapture)
    REGISTER(ArbiterWindowsGet) REGISTER(ArbiterWindowsGetCurrent) REGISTER(ArbiterWindowsGetLastFocused)
    REGISTER(ArbiterWindowsGetAll) REGISTER(ArbiterWindowsUpdate) REGISTER(ArbiterWindowsRemove)
    REGISTER(ArbiterScriptingExecute) REGISTER(ArbiterScriptingInsertCSS) REGISTER(ArbiterScriptingRemoveCSS)
    REGISTER(ArbiterActionGetBadge) REGISTER(ArbiterActionSetBadge) REGISTER(ArbiterActionGetTitle) REGISTER(ArbiterActionSetTitle)
    REGISTER(ArbiterCommandsGetAll) REGISTER(ArbiterMenusCreate) REGISTER(ArbiterMenusUpdate) REGISTER(ArbiterMenusRemove) REGISTER(ArbiterMenusRemoveAll)
    REGISTER(ArbiterPermissionsContains) REGISTER(ArbiterPermissionsGetAll) REGISTER(ArbiterPermissionsRequest)
    REGISTER(ArbiterActionGetBadgeColor) REGISTER(ArbiterActionSetBadgeColor) REGISTER(ArbiterActionGetTextColor) REGISTER(ArbiterActionSetTextColor)
#undef REGISTER
}
}
