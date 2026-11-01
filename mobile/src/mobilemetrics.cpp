#include "mobilemetrics.h"
#include <QGuiApplication>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QCoreApplication>
#endif
#ifdef Q_OS_IOS
extern qreal arbiterIOSFontScale();
#endif

MobileMetrics::MobileMetrics(QObject *parent) : QObject(parent) {
    refresh();
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (qGuiApp->applicationState() == Qt::ApplicationActive) refresh();
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this] { refresh(); });
    m_timer.start(1000);
}
int MobileMetrics::touchTarget() const {
#ifdef Q_OS_ANDROID
    return 48;
#else
    return 44;
#endif
}
void MobileMetrics::refresh() {
    qreal value = 1;
#ifdef Q_OS_IOS
    value = arbiterIOSFontScale();
#elif defined(Q_OS_ANDROID)
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    auto resources = context.callObjectMethod("getResources", "()Landroid/content/res/Resources;");
    auto configuration = resources.callObjectMethod("getConfiguration", "()Landroid/content/res/Configuration;");
    if (configuration.isValid()) value = configuration.getField<jfloat>("fontScale");
#endif
    // Preserve accessibility sizes; do not cap large text at a cosmetic limit.
    if (value > 0 && !qFuzzyCompare(value, m_scale)) {
        m_scale = value;
        emit changed();
    }
}
