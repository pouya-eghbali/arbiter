#pragma once
#include <QObject>
#include "settings.h"
#include <QUrl>
#include <QColor>

class Appearance final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY colorsChanged)
    Q_PROPERTY(bool navigationPill READ navigationPill WRITE setNavigationPill NOTIFY navigationPillChanged)
    Q_PROPERTY(QString navigationRevealMode READ navigationRevealMode WRITE setNavigationRevealMode NOTIFY navigationRevealModeChanged)
    Q_PROPERTY(QString pageEdgeMode READ pageEdgeMode WRITE setPageEdgeMode NOTIFY pageEdgeModeChanged)
    Q_PROPERTY(bool windowDecorations READ windowDecorations WRITE setWindowDecorations NOTIFY windowDecorationsChanged)
    Q_PROPERTY(QString siteMenuMode READ siteMenuMode WRITE setSiteMenuMode NOTIFY siteMenuModeChanged)
    Q_PROPERTY(QColor lightAccent READ lightAccent WRITE setLightAccent NOTIFY accentColorsChanged)
    Q_PROPERTY(QColor darkAccent READ darkAccent WRITE setDarkAccent NOTIFY accentColorsChanged)
    Q_PROPERTY(QColor lightBackground READ lightBackground WRITE setLightBackground NOTIFY backgroundColorsChanged)
    Q_PROPERTY(QColor darkBackground READ darkBackground WRITE setDarkBackground NOTIFY backgroundColorsChanged)
    Q_PROPERTY(QUrl lightBackgroundImage READ lightBackgroundImage NOTIFY backgroundImagesChanged)
    Q_PROPERTY(QUrl darkBackgroundImage READ darkBackgroundImage NOTIFY backgroundImagesChanged)
public:
    explicit Appearance(QObject *parent = nullptr);
    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);
    bool dark() const;
    bool navigationPill() const { return m_navigationPill; }
    void setNavigationPill(bool enabled);
    QString navigationRevealMode() const;
    void setNavigationRevealMode(const QString &mode);
    QString pageEdgeMode() const;
    void setPageEdgeMode(const QString &mode);
    bool windowDecorations() const { return m_windowDecorations; }
    void setWindowDecorations(bool enabled);
    QString siteMenuMode() const;
    void setSiteMenuMode(const QString &mode);
    QColor lightAccent() const;
    QColor darkAccent() const;
    void setLightAccent(const QColor &color);
    void setDarkAccent(const QColor &color);
    QColor lightBackground() const;
    QColor darkBackground() const;
    void setLightBackground(const QColor &color);
    void setDarkBackground(const QColor &color);
    QUrl lightBackgroundImage() const;
    QUrl darkBackgroundImage() const;
    Q_INVOKABLE QString setBackgroundImage(bool dark,const QUrl &file);
    Q_INVOKABLE void clearBackgroundImage(bool dark);
    Q_INVOKABLE QString displayAddress(const QUrl &url) const;
signals:
    void themeChanged();
    void colorsChanged();
    void navigationPillChanged();
    void navigationRevealModeChanged();
    void pageEdgeModeChanged();
    void windowDecorationsChanged();
    void siteMenuModeChanged();
    void accentColorsChanged();
    void backgroundColorsChanged();
    void backgroundImagesChanged();
private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void applyTheme();
    QUrl backgroundImage(bool dark) const;
    QColor savedColor(const QString &key, const QColor &fallback) const;
    void setAccent(const QString &key, const QColor &color, const QColor &current);
    Settings m_settings;
    QString m_theme;
    bool m_navigationPill;
    bool m_windowDecorations;
};
