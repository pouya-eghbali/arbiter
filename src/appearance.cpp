#include "appearance.h"
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include <QEvent>

Appearance::Appearance(QObject *parent) : QObject(parent),
    m_theme(m_settings.value(QStringLiteral("appearance/theme"), QStringLiteral("system")).toString()),
    m_navigationPill(m_settings.value(QStringLiteral("appearance/navigationPill"), true).toBool()),
    m_windowDecorations(m_settings.value(QStringLiteral("appearance/windowDecorations"), false).toBool())
{
    if (m_theme != "light" && m_theme != "dark") m_theme = QStringLiteral("system");
    connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &Appearance::colorsChanged);
    qGuiApp->installEventFilter(this);
    applyTheme();
}
bool Appearance::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qGuiApp && event->type() == QEvent::ApplicationPaletteChange) emit colorsChanged();
    return QObject::eventFilter(watched, event);
}
bool Appearance::dark() const
{
    if (m_theme != "system") return m_theme == "dark";
    const auto scheme = qGuiApp->styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark || (scheme == Qt::ColorScheme::Unknown
        && qGuiApp->palette().color(QPalette::Window).lightnessF() < 0.5);
}
void Appearance::applyTheme()
{
    if (m_theme == "system") qGuiApp->styleHints()->unsetColorScheme();
    else qGuiApp->styleHints()->setColorScheme(m_theme == "dark" ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
}
void Appearance::setTheme(const QString &theme)
{
    if ((theme != "system" && theme != "dark" && theme != "light") || theme == m_theme) return;
    m_theme = theme;
    m_settings.setValue(QStringLiteral("appearance/theme"), theme);
    applyTheme();
    emit themeChanged();
    emit colorsChanged();
}
void Appearance::setNavigationPill(bool enabled)
{
    if (m_navigationPill == enabled) return;
    m_navigationPill = enabled;
    m_settings.setValue(QStringLiteral("appearance/navigationPill"), enabled);
    emit navigationPillChanged();
}
void Appearance::setWindowDecorations(bool enabled)
{
    if (m_windowDecorations == enabled) return;
    m_windowDecorations = enabled;
    m_settings.setValue(QStringLiteral("appearance/windowDecorations"), enabled);
    emit windowDecorationsChanged();
}
QString Appearance::displayAddress(const QUrl &url) const
{
    if (url.scheme() == "https" || url.scheme() == "http")
        return url.toDisplayString(QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery
            | QUrl::RemoveFragment | QUrl::StripTrailingSlash);
    return url.scheme() == "about" ? tr("New tab") : url.toDisplayString(QUrl::RemoveUserInfo);
}
QString Appearance::siteMenuMode() const
{
    const auto mode = m_settings.value(QStringLiteral("appearance/siteMenuMode"), QStringLiteral("title")).toString();
    return mode == "navigation" || mode == "off" ? mode : QStringLiteral("title");
}
void Appearance::setSiteMenuMode(const QString &mode)
{
    if ((mode != "title" && mode != "navigation" && mode != "off") || mode == siteMenuMode()) return;
    m_settings.setValue(QStringLiteral("appearance/siteMenuMode"), mode);
    emit siteMenuModeChanged();
}
QColor Appearance::savedColor(const QString &key, const QColor &fallback) const
{
    const QColor saved(m_settings.value(key).toString());
    return saved.isValid() && saved.alpha() == 255 ? saved : fallback;
}
QColor Appearance::lightAccent() const { return savedColor(QStringLiteral("appearance/lightAccent"), QColor("#2868b8")); }
QColor Appearance::darkAccent() const { return savedColor(QStringLiteral("appearance/darkAccent"), QColor("#8dbbff")); }
void Appearance::setAccent(const QString &key, const QColor &color, const QColor &current)
{
    if (!color.isValid() || color.alpha() != 255 || color == current) return;
    m_settings.setValue(key, color.name(QColor::HexRgb));
    emit accentColorsChanged();
}
void Appearance::setLightAccent(const QColor &color) { setAccent(QStringLiteral("appearance/lightAccent"), color, lightAccent()); }
void Appearance::setDarkAccent(const QColor &color) { setAccent(QStringLiteral("appearance/darkAccent"), color, darkAccent()); }

QColor Appearance::lightBackground() const { return savedColor(QStringLiteral("appearance/lightBackground"), QColor("#edf2f7")); }
QColor Appearance::darkBackground() const { return savedColor(QStringLiteral("appearance/darkBackground"), QColor("#11161e")); }
void Appearance::setLightBackground(const QColor &color)
{
    if (!color.isValid() || color.alpha() != 255 || color == lightBackground()) return;
    m_settings.setValue(QStringLiteral("appearance/lightBackground"), color.name(QColor::HexRgb));
    emit backgroundColorsChanged();
}
void Appearance::setDarkBackground(const QColor &color)
{
    if (!color.isValid() || color.alpha() != 255 || color == darkBackground()) return;
    m_settings.setValue(QStringLiteral("appearance/darkBackground"), color.name(QColor::HexRgb));
    emit backgroundColorsChanged();
}
