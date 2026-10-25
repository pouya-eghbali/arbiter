#pragma once
#include <QObject>
#include <QTranslator>
#include <QVariantList>

// Load before constructing UI models: native menus and cached C++ labels use
// the same language as QML. Changes are persisted for the next application run.
class Localization final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString activeLanguage READ activeLanguage CONSTANT)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY languageChanged)
public:
    explicit Localization(QObject *parent = nullptr);
    ~Localization() override;
    QString language() const { return m_language; }
    QString activeLanguage() const { return m_activeLanguage; }
    QVariantList languages() const;
    bool restartRequired() const;
    void setLanguage(const QString &language);
    static QString resolve(const QString &language, const QStringList &systemLanguages);
signals:
    void languageChanged();
private:
    static QVariantList supportedLanguages();
    QTranslator m_translator;
    QString m_language, m_activeLanguage;
};
