#include "appearance.h"
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include <QEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QImageReader>
#include <QCryptographicHash>

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

QUrl Appearance::backgroundImage(bool dark) const
{
    const auto value=m_settings.value(dark ? "appearance/darkBackgroundImage" : "appearance/lightBackgroundImage").toString();
    if(value.isEmpty()) return {};
    if(QDir::isAbsolutePath(value)) return QUrl::fromLocalFile(value);
    const QUrl url(value);
    if(url.isLocalFile()) return url;
    // Dotfiles may also reference an absolute path or a path relative to settings.toml.
    if(!url.isRelative() || value.startsWith("//")) return {};
    return QUrl::fromLocalFile(QDir(QFileInfo(Settings::configFile()).absolutePath()).absoluteFilePath(value));
}
QUrl Appearance::lightBackgroundImage() const { return backgroundImage(false); }
QUrl Appearance::darkBackgroundImage() const { return backgroundImage(true); }
QString Appearance::setBackgroundImage(bool dark,const QUrl &url)
{
    if(!url.isLocalFile()) return tr("Choose an image from this computer.");
    QFile input(url.toLocalFile());
    if(!input.open(QIODevice::ReadOnly) || input.size()>64*1024*1024) return tr("Choose a readable image smaller than 64 MB.");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if(!hash.addData(&input) || !input.seek(0)) return tr("The image could not be read. Try another file.");
    QImageReader reader(&input); reader.setAutoTransform(true);
    const auto size=reader.size();
    if(!size.isValid() || qint64(size.width())*size.height()>64000000) return tr("Choose an image with fewer than 64 million pixels.");
    // Bound decoded wallpaper size while retaining enough detail for HiDPI windows.
    if(size.width()>5120 || size.height()>5120) reader.setScaledSize(size.scaled(5120,5120,Qt::KeepAspectRatio));
    const auto image=reader.read();
    if(image.isNull()) return tr("This image format could not be opened. Try PNG, JPEG, or WebP.");
    const QString relative="backgrounds/"+QString::fromLatin1(hash.result().toHex())+".png";
    const QDir config(QFileInfo(Settings::configFile()).absolutePath());
    if(!config.mkpath("backgrounds")) return tr("Could not create the background image folder.");
    QSaveFile output(config.filePath(relative));
    if(!output.open(QIODevice::WriteOnly)) return tr("Could not save the background image.");
    output.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    if(!image.save(&output,"PNG") || !output.commit()) return tr("Could not save the background image.");
    m_settings.setValue(dark ? "appearance/darkBackgroundImage" : "appearance/lightBackgroundImage",relative);
    if(m_settings.status()!=QSettings::NoError) return tr("The background image was copied, but its setting could not be saved.");
    emit backgroundImagesChanged();
    return {};
}
void Appearance::clearBackgroundImage(bool dark)
{
    m_settings.setValue(dark ? "appearance/darkBackgroundImage" : "appearance/lightBackgroundImage",QString());
    emit backgroundImagesChanged();
}
