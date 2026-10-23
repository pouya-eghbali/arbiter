#include "sitenavigation.h"
#include <QAction>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>

namespace {
QString label(QString text) {
    // Page text must not introduce native mnemonics, shortcuts or bidi controls.
    for (auto &ch : text) {
        if (ch.category() == QChar::Other_Control || ch.category() == QChar::Other_Format) ch = QChar(' ');
    }
    return text.simplified().left(80).replace('&', "&&");
}
bool webUrl(const QUrl &url) {
    return url.isValid() && !url.host().isEmpty() && url.userInfo().isEmpty()
        && (url.scheme() == "https" || url.scheme() == "http");
}
}
SiteNavigation::SiteNavigation(QMenuBar *bar, QObject *parent) : QObject(parent), m_bar(bar) {}
QString SiteNavigation::extractionScript() const {
    static const QString script = [] {
        QFile file(":/navigation/navigation.js");
        if (!file.open(QIODevice::ReadOnly)) return QString();
        return QString::fromUtf8(file.readAll());
    }();
    return script;
}
void SiteNavigation::reset(const QUrl &url, const QString &title, const QString &mode) {
    ++m_revision;
    m_url = url; m_title = title; m_mode = mode;
    m_nodes.clear(); rebuild();
}
void SiteNavigation::setTitle(const QString &title) {
    if (title == m_title) return;
    m_title = title; rebuild();
}
QVariantList SiteNavigation::sanitize(const QVariantList &nodes, int depth, int &budget) const {
    QVariantList result;
    if (depth > 3) return result;
    for (const auto &value : nodes) {
        if (budget <= 0) break;
        const auto node = value.toMap();
        const QString text = label(node.value("label").toString());
        if (text.isEmpty()) continue;
        --budget;
        auto children = sanitize(node.value("children").toList(), depth + 1, budget);
        const QUrl url(node.value("url").toString());
        if (!webUrl(url) && children.isEmpty()) continue;
        result.append(QVariantMap{{"label", text}, {"url", webUrl(url) ? url.toString() : QString()},
            {"children", children}, {"current", node.value("current").toBool()}});
    }
    return result;
}
void SiteNavigation::update(const QUrl &url, const QVariantMap &snapshot) {
    if (url != m_url || QUrl(snapshot.value("url").toString()) != m_url || !webUrl(m_url)) return;
    int budget = 120;
    const auto nodes = sanitize(snapshot.value("nodes").toList(), 0, budget);
    if (nodes == m_nodes) return;
    m_nodes = nodes; rebuild();
}
void SiteNavigation::populate(QMenu *menu, const QVariantList &nodes) {
    for (const auto &value : nodes) {
        const auto node = value.toMap();
        const auto children = node.value("children").toList();
        QMenu *target = menu;
        const auto text = node.value("label").toString();
        if (!children.isEmpty()) target = menu->addMenu(text);
        const QUrl url(node.value("url").toString());
        if (webUrl(url)) {
            auto *action = target->addAction(children.isEmpty() ? text : tr("Open %1").arg(text));
            action->setMenuRole(QAction::NoRole);
            action->setStatusTip(url.toDisplayString());
            action->setToolTip(url.toDisplayString());
            if (node.value("current").toBool()) { action->setCheckable(true); action->setChecked(true); }
            const auto revision = m_revision;
            connect(action, &QAction::triggered, this, [this, revision, url] {
                if (revision == m_revision && m_mode != "off") emit linkActivated(url);
            });
        }
        if (!children.isEmpty()) populate(target, children);
    }
}
void SiteNavigation::rebuild() {
    // Do not replace native actions while the user is interacting with a menu.
    for (const auto *menu : m_menus) {
        if (menu->isVisible()) { m_rebuildPending = true; return; }
    }
    m_rebuildPending = false;
    qDeleteAll(m_menus); m_menus.clear();
    if (m_mode == "off" || !webUrl(m_url)) return;
    const auto makeMenu = [this](const QString &text) {
        auto *menu = m_bar->addMenu(text);
        menu->menuAction()->setMenuRole(QAction::NoRole);
        m_menus.append(menu);
        connect(menu, &QMenu::aboutToHide, this, [this] {
            QTimer::singleShot(0, this, [this] { if (m_rebuildPending) rebuild(); });
        });
        return menu;
    };
    if (m_mode == "navigation" && !m_nodes.isEmpty()) {
        QVariantList items;
        for (const auto &node : m_nodes) items.append(node.toMap().value("children").toList());
        int count = 0;
        for (const auto &item : items) {
            if (count++ == 6) break;
            const auto node = item.toMap();
            auto *menu = makeMenu(node.value("label").toString());
            auto heading = node;
            heading["children"] = QVariantList();
            populate(menu, {heading});
            const auto children = node.value("children").toList();
            if (!menu->actions().isEmpty() && !children.isEmpty()) menu->addSeparator();
            populate(menu, children);
        }
        if (items.size() > 6) populate(makeMenu(tr("More")), items.mid(6));
        if (!items.isEmpty()) return;
    }
    auto title = label(m_title);
    if (title.isEmpty()) title = label(m_url.host());
    auto *menu = makeMenu(title);
    if (m_nodes.isEmpty()) menu->addAction(tr("No navigation links found"))->setEnabled(false);
    else if (m_nodes.size() == 1) populate(menu, m_nodes.first().toMap().value("children").toList());
    else populate(menu, m_nodes);
}
