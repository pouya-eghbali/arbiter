#include "localization.h"
#include "settings.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>

QVariantList Localization::supportedLanguages() {
    QFile file(QStringLiteral(":/i18n/languages.json"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).array().toVariantList();
}
QString Localization::resolve(const QString &language, const QStringList &systemLanguages) {
    const auto supported = supportedLanguages();
    const auto candidates = language == "system" ? systemLanguages : QStringList{language};
    bool traditionalChinese = false;
    for (const auto &candidate : candidates) {
        const QLocale locale(candidate);
        if (locale.language() == QLocale::Chinese && locale.script() == QLocale::TraditionalHanScript)
            traditionalChinese = true;
        // Qt appends a generic "zh" fallback for Traditional Chinese locales.
        // Its default script is Simplified, so it must not change script here.
        if (traditionalChinese && candidate.compare("zh", Qt::CaseInsensitive) == 0) continue;
        for (const auto &item : supported) {
            const auto code = item.toMap().value("code").toString();
            if (code == candidate) return code;
            const QLocale available(code);
            if (locale.language() == available.language() && locale.language() != QLocale::C
                && locale.script() == available.script()) return code;
        }
    }
    return QStringLiteral("en");
}
Localization::Localization(QObject *parent) : QObject(parent) {
    m_language = Settings().value("General/language", QStringLiteral("system")).toString();
    bool supported = m_language == "system";
    for (const auto &item : supportedLanguages()) supported |= item.toMap().value("code").toString() == m_language;
    if (!supported) m_language = QStringLiteral("system");
    m_activeLanguage = resolve(m_language, QLocale::system().uiLanguages());
    if (m_activeLanguage != "en") {
        if (m_translator.load(QStringLiteral(":/i18n/arbiter_%1.qm").arg(m_activeLanguage)))
            QCoreApplication::installTranslator(&m_translator);
        else m_activeLanguage = QStringLiteral("en");
    }
    // Preserve regional date/number conventions when following a supported
    // system language; an explicit choice uses that language's default region.
    const auto locale = m_language == "system" && QLocale::system().language() == QLocale(m_activeLanguage).language()
        ? QLocale::system() : QLocale(m_activeLanguage);
    QLocale::setDefault(locale);
}
Localization::~Localization() { QCoreApplication::removeTranslator(&m_translator); }
QVariantList Localization::languages() const {
    auto result = supportedLanguages();
    result.prepend(QVariantMap{{"code", "system"}, {"name", tr("System default")}});
    return result;
}
bool Localization::restartRequired() const {
    return resolve(m_language, QLocale::system().uiLanguages()) != m_activeLanguage;
}
void Localization::setLanguage(const QString &language) {
    if (language == m_language) return;
    bool supported = language == "system";
    for (const auto &item : supportedLanguages()) supported |= item.toMap().value("code").toString() == language;
    if (!supported) return;
    Settings settings;
    settings.setValue("General/language", language);
    if (settings.status() != QSettings::NoError) return;
    m_language = language;
    emit languageChanged();
}
