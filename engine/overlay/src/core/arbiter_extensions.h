// Copyright 2026 Arbiter contributors
// SPDX-License-Identifier: GPL-3.0-only
#ifndef ARBITER_EXTENSIONS_H
#define ARBITER_EXTENSIONS_H
#include <QObject>
#include <QString>
#include <QVariant>
#include <optional>
#include "base/values.h"
namespace content { class WebContents; class BrowserContext; }
class ExtensionFunctionRegistry;
namespace extensions { class ScriptExecutor; }
namespace QtWebEngineCore {
class WebContentsAdapter;
int arbiterEnsureExtensionTabId(QObject *);
QVariantList arbiterExtensionMenuItems(WebContentsAdapter *, const QVariantMap &);
void arbiterInvokeExtensionMenu(WebContentsAdapter *, const QString &, const QVariantMap &);
void arbiterRegisterExtensionTab(WebContentsAdapter *, bool notify = true);
void arbiterRemoveExtensionTab(WebContentsAdapter *, bool notify = true);
void arbiterGrantActiveTab(WebContentsAdapter *, const QString &);
void arbiterRegisterExtensionFunctions(ExtensionFunctionRegistry *);
content::WebContents *arbiterExtensionContents(content::BrowserContext *, int);
extensions::ScriptExecutor *arbiterExtensionScriptExecutor(content::WebContents *);
void arbiterExtensionIds(content::WebContents *, int *, int *);
std::optional<base::Value::Dict> arbiterExtensionTabInfo(content::WebContents *);
class ArbiterExtensionTabNotifier : public QObject {
    Q_OBJECT
public:
    explicit ArbiterExtensionTabNotifier(WebContentsAdapter *);
    ~ArbiterExtensionTabNotifier() override;
    WebContentsAdapter *adapter;
public Q_SLOTS:
    void refresh();
};
}
#endif
