#include "designpreview.h"
#include "mobilemodel.h"
#include "nativepage.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QTimer>
namespace {
QQuickItem *visualItem(QQuickItem *item, const QString &name) {
    if (!item)
        return nullptr;
    if (item->objectName() == name)
        return item;
    for (auto child : item->childItems())
        if (auto found = visualItem(child, name))
            return found;
    return nullptr;
}
QJsonArray fixtureTabs() {
    const QList<QStringList> sites = {{"article", "A quieter web", "https://example.com", "personal"},
                                      {"wikipedia", "Wikipedia", "https://wikipedia.org", "personal"},
                                      {"github", "GitHub", "https://github.com", "personal"},
                                      {"youtube", "YouTube", "https://youtube.com", "personal"},
                                      {"reddit", "Reddit", "https://reddit.com", "personal"},
                                      {"mdn", "MDN Web Docs", "https://developer.mozilla.org", "personal"},
                                      {"docs", "Docs", "https://docs.google.com", "work"},
                                      {"figma", "Figma", "https://figma.com", "work"},
                                      {"linear", "Linear", "https://linear.app", "work"},
                                      {"verge", "The Verge", "https://theverge.com", "reading"},
                                      {"ars", "Ars Technica", "https://arstechnica.com", "reading"},
                                      {"polygon", "Polygon", "https://polygon.com", "reading"},
                                      {"reading-four", "A longer read", "https://example.org", "reading"},
                                      {"work-four", "Project notes", "https://example.org", "work"}};
    QJsonArray tabs;
    for (auto site : sites)
        tabs.append(QJsonObject{{"id", site[0]},
                                {"title", site[1]},
                                {"url", site[2]},
                                {"workspace", site[3]},
                                {"profile", "shared"},
                                {"private", false},
                                {"sleeping", true},
                                {"lastUsed", double(QDateTime::currentMSecsSinceEpoch())}});
    return tabs;
}
QImage article(const QString &title) {
    QImage image(430, 880, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor("#283344"));
    p.setFont(QFont("serif", 27, QFont::Bold));
    p.drawText(QRect(24, 56, 382, 72), Qt::TextWordWrap,
               title == "A quieter web" ? title : title + "\nA quieter web");
    p.setFont(QFont("sans-serif", 14));
    p.drawText(QRect(24, 140, 382, 55), Qt::TextWordWrap,
               "A more focused, humane internet is possible. Here’s where to begin.");
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#e4e5e9"));
    p.drawRoundedRect(QRectF(24, 212, 382, 188), 8, 8);
    p.setBrush(QColor("#d0d2da"));
    QPolygonF hills;
    hills << QPointF(24, 376) << QPointF(140, 285) << QPointF(223, 347) << QPointF(290, 300)
          << QPointF(406, 376);
    p.drawPolygon(hills);
    p.setBrush(QColor("#f6f6f8"));
    p.drawEllipse(QPointF(310, 261), 18, 18);
    p.setBrush(QColor("#d0d2da"));
    for (int i = 0; i < 4; ++i)
        p.drawRoundedRect(QRectF(24, 426 + i * 24, 382 - (i % 3) * 42, 9), 4, 4);
    p.setPen(QColor("#283344"));
    p.setFont(QFont("serif", 21, QFont::Bold));
    p.drawText(QPointF(24, 572), "Less noise, more meaning");
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#d0d2da"));
    for (int i = 0; i < 9; ++i)
        p.drawRoundedRect(QRectF(24, 596 + i * 24, 382 - (i % 4) * 24, 9), 4, 4);
    return image;
}
} // namespace
void seedDesignPreview(const QString &directory) {
    const QJsonArray workspaces = {
        QJsonObject{{"id", "personal"}, {"name", "Personal"}, {"color", "#9d83ee"}, {"isolated", false}},
        QJsonObject{{"id", "work"}, {"name", "Work"}, {"color", "#4da3ee"}, {"isolated", false}},
        QJsonObject{{"id", "reading"}, {"name", "Reading"}, {"color", "#80c966"}, {"isolated", false}}};
    const QJsonObject settings{{"theme", "dark"},
                               {"darkAccent", "#a78bfa"},
                               {"darkBackground", "#171e28"},
                               {"onboardingDone", true},
                               {"backgroundSync", false}};
    const QJsonArray history = {QJsonObject{{"url", "https://github.com"},
                                            {"title", "GitHub"},
                                            {"time", double(QDateTime::currentMSecsSinceEpoch())}}};
    QFile file(directory + "/browser-session-v1.json");
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(QJsonObject{{"settings", settings},
                                             {"tabs", fixtureTabs()},
                                             {"workspaces", workspaces},
                                             {"workspaceId", "personal"},
                                             {"activeId", "article"},
                                             {"history", history}})
                       .toJson());
}
void renderDesignPreview(MobileModel &model, QQmlApplicationEngine &engine, const QString &screen,
                         const QString &output) {
    for (auto tab : model.tabs()) {
        auto value = tab.toMap();
        if (value.value("id") != "start")
            model.setPreview(value.value("id").toString(), article(value.value("title").toString()));
    }
    const auto state = screen.section(':', 0, 0);
    const auto detail = screen.section(':', 1);
    if (state == "start")
        model.addTab();
    if (qEnvironmentVariableIsSet("ARBITER_PREVIEW_THEME"))
        model.setSetting("theme", qEnvironmentVariable("ARBITER_PREVIEW_THEME"));
    const QString panel = state == "start" || state == "browsing" || state == "private" || state == "suggestions" || state == "interactions" ? QString()
                          : state == "search" ? QStringLiteral("address")
                                               : state;
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window) {
        qApp->exit(1);
        return;
    }
    if (const auto width = qEnvironmentVariableIntValue("ARBITER_PREVIEW_WIDTH"); width > 0)
        window->setWidth(width);
    if (const auto height = qEnvironmentVariableIntValue("ARBITER_PREVIEW_HEIGHT"); height > 0)
        window->setHeight(height);
    window->setProperty("panel", panel);
    if (state == "private") {
        model.newPrivateTab();
        qGuiApp->applicationStateChanged(Qt::ApplicationInactive);
    }
    if (state == "suggestions")
        model.updatePage(model.activeId(), "credentialFocus", {{"origin", "https://example.com"}});
    window->setProperty("pendingOrigin", "https://accounts.example.com");
    window->setProperty("pendingUsername", "alex@example.com");
    window->setProperty("pairDesktop", "Alex’s MacBook Pro");
    window->setProperty("pairPin", "482 916");
    if (state == "accounts")
        window->setProperty("accounts", QVariantList{QVariantMap{{"id", "fixture"}, {"username", "alex@example.com"}}, QVariantMap{{"id", "fixture2"}, {"username", "A longer account name to test narrow screens"}}});
    if (detail.endsWith("keyboard"))
        window->setProperty("previewKeyboardInset", 280.);
    if (state == "onboarding")
        window->setProperty("onboardingStep", detail.toInt());
    if (state == "downloads" && detail != "empty") {
        model.downloadEvent("download", {{"id", "fixture-download"}, {"name", "Arbiter-desktop-release-with-a-long-filename.zip"}, {"url", "https://example.com/download.zip"}}, false);
        model.downloadEvent("downloadProgress", {{"id", "fixture-download"}, {"received", 25000000}, {"total", 100000000}, {"speed", 2500000}}, false);
    }
    if (state == "history" && detail == "empty")
        model.clearHistory("");
    QTimer::singleShot(100, window, [window, state, detail] {
        if (state == "settings" && !detail.isEmpty()) {
            auto loader = window->findChild<QObject *>("settingsLoader");
            if (loader) {
                auto item = loader->property("item").value<QObject *>();
                if (item) item->setProperty("category", detail.section(':', 0, 0));
            }
        }
        if (state == "workspaces" && detail.startsWith("editor")) {
            auto workspace = window->findChild<QObject *>("workspaceScreen");
            if (workspace) QMetaObject::invokeMethod(workspace, "edit", Q_ARG(QVariant, QVariant("work")), Q_ARG(QVariant, QVariant("A longer workspace title")));
        }
    });
    if (state == "interactions") {
        QTimer::singleShot(700, window, [window, &model, output] {
            auto pill = window->findChild<QQuickItem *>("mobilePill");
            if (!pill) {
                qCritical("No pill in design fixture");
                qApp->exit(1);
                return;
            }
            const auto before = model.activeId();
            auto swipe = [&](const QPoint &delta) {
                const auto start =
                    pill->mapToScene(QPointF(pill->width() * .5, pill->height() * .5)).toPoint();
                QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
                for (int i = 1; i <= 6; ++i) {
                    QTest::mouseMove(window, start + delta * i / 6, 20);
                }
                QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start + delta);
                QTest::qWait(100);
            };
            swipe(QPoint(-100, 0));
            if (model.activeId() == before || window->property("panel").toString() != "") {
                qCritical() << "Horizontal pill gesture did not switch tabs cleanly" << before
                            << model.activeId() << window->property("panel") << pill->property("lastDrag");
                qApp->exit(1);
                return;
            }
            const auto focused = model.activeId();
            model.updatePage(focused, "error", {{"message", "Fixture renderer recovery"}, {"crashed", true}});
            QTest::qWait(50);
            model.selectTab(focused);
            QTest::qWait(100);
            if (!NativePage::pages.value(focused)) {
                qCritical("The page did not recreate after renderer recovery");
                qApp->exit(1);
                return;
            }
            swipe(QPoint(0, -100));
            if (window->property("panel").toString() != "tabs") {
                qCritical("Upward pill gesture did not open tabs");
                qApp->exit(1);
                return;
            }
            QTest::qWait(100);
            auto close = visualItem(window->contentItem(), "closeTab-article");
            if (!close) {
                qCritical("No tab close button");
                qApp->exit(1);
                return;
            }
            const auto count = model.tabs().size();
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                              close->mapToScene(QPointF(close->width() / 2, close->height() / 2)).toPoint());
            QTest::qWait(100);
            if (model.tabs().size() != count - 1 || window->property("panel").toString() != "tabs") {
                qCritical("Closing a tab also dismissed overview");
                qApp->exit(1);
                return;
            }
            QTest::keyClick(window, Qt::Key_Escape);
            QTest::qWait(50);
            if (window->property("panel").toString() != "") {
                qCritical("Escape did not dismiss the sheet");
                qApp->exit(1);
                return;
            }
            window->grabWindow().save(output, "PNG");
            qInfo("Pill swipes and tab-close interaction checks passed");
            qApp->exit(0);
        });
        return;
    }
    if (detail.endsWith(":bottom"))
        QTimer::singleShot(400, window, [window] {
            if (auto scroll = window->findChild<QObject *>("pageScroll")) {
                auto flick = scroll->property("contentItem").value<QObject *>();
                if (flick) flick->setProperty("contentY", qMax(0., flick->property("contentHeight").toDouble() - flick->property("height").toDouble()));
            }
        });
    if (detail.endsWith(":popup"))
        QTimer::singleShot(400, window, [window] {
            std::function<QObject *(QQuickItem *)> findCombo = [&](QQuickItem *item) -> QObject * {
                if (!item || !item->isVisible()) return nullptr;
                const auto y = item->mapToScene(QPointF(0, item->height() / 2)).y();
                if (item->property("currentIndex").isValid() && y > 70 && y < window->height() - 60)
                    if (auto popup = item->property("popup").value<QObject *>()) return popup;
                for (auto child : item->childItems())
                    if (auto popup = findCombo(child)) return popup;
                return nullptr;
            };
            if (auto popup = findCombo(window->contentItem())) QMetaObject::invokeMethod(popup, "open");
        });
    QTimer::singleShot(700, window, [window, output] {
        QDir().mkpath(QFileInfo(output).absolutePath());
        const bool saved = window->grabWindow().save(output, "PNG");
        qApp->exit(saved ? 0 : 1);
    });
}
