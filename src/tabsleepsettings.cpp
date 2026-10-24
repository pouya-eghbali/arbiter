#include "tabsleepsettings.h"
#include "settings.h"
#include <algorithm>

TabSleepSettings::TabSleepSettings(QObject *parent) : QObject(parent) {
    bool valid=false;
    const int saved=Settings().value(QStringLiteral("Tabs/sleepTimeout"),900).toInt(&valid);
    if(valid) m_timeout=std::clamp(saved,0,86400);
}
void TabSleepSettings::setTimeoutSeconds(int seconds) {
    seconds=std::clamp(seconds,0,86400);
    if(seconds==m_timeout) return;
    m_timeout=seconds;
    Settings().setValue(QStringLiteral("Tabs/sleepTimeout"),seconds);
    emit timeoutChanged();
}
