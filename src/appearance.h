#pragma once
#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QColor>

class Appearance final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY colorsChanged)
    Q_PROPERTY(bool navigationPill READ navigationPill WRITE setNavigationPill NOTIFY navigationPillChanged)
    Q_PROPERTY(QString siteMenuMode READ siteMenuMode WRITE setSiteMenuMode NOTIFY siteMenuModeChanged)
    Q_PROPERTY(QColor lightAccent READ lightAccent WRITE setLightAccent NOTIFY accentColorsChanged)
    Q_PROPERTY(QColor darkAccent READ darkAccent WRITE setDarkAccent NOTIFY accentColorsChanged)
    Q_PROPERTY(QColor lightBackground READ lightBackground WRITE setLightBackground NOTIFY backgroundColorsChanged)
    Q_PROPERTY(QColor darkBackground READ darkBackground WRITE setDarkBackground NOTIFY backgroundColorsChanged)
public:
    explicit Appearance(QObject *parent = nullptr);
    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);
    bool dark() const;
    bool navigationPill() const { return m_navigationPill; }
    void setNavigationPill(bool enabled);
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
    Q_INVOKABLE QString displayAddress(const QUrl &url) const;
signals:
    void themeChanged();
    void colorsChanged();
    void navigationPillChanged();
    void siteMenuModeChanged();
    void accentColorsChanged();
    void backgroundColorsChanged();
private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void applyTheme();
    QColor savedColor(const QString &key, const QColor &fallback) const;
    void setAccent(const QString &key, const QColor &color, const QColor &current);
    QSettings m_settings;
    QString m_theme;
    bool m_navigationPill;
};
