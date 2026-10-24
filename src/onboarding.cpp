#include "onboarding.h"
#include "appearance.h"
#include <algorithm>

Onboarding::Onboarding(Appearance *appearance,QObject *parent):QObject(parent),m_appearance(appearance) {
    m_completed=m_settings.value("onboarding/version",0).toInt()>=1;
    m_tutorialPending=m_settings.value("onboarding/tutorialPending",false).toBool();
    m_step=std::clamp(m_settings.value("onboarding/step",0).toInt(),0,5);
    connect(appearance,&Appearance::navigationPillChanged,this,&Onboarding::changed);
    connect(appearance,&Appearance::windowDecorationsChanged,this,&Onboarding::changed);
}
bool Onboarding::save(const QString &key,const QVariant &value) {
    m_settings.setValue(key,value);
    m_error=m_settings.status()==QSettings::NoError ? QString() : tr("Could not save setup. Check that your TOML settings file is valid and writable, then restart Arbiter.");
    emit changed();
    return m_error.isEmpty();
}
QString Onboarding::preset() const {
    if(m_appearance->windowDecorations()) return m_appearance->navigationPill() ? QStringLiteral("classic") : QString();
    return m_appearance->navigationPill() ? QStringLiteral("glacier") : QStringLiteral("minimal");
}
void Onboarding::next() {
    if(m_step<5 && save("onboarding/step",m_step+1)) { ++m_step; emit changed(); }
}
void Onboarding::back() {
    if(m_step>0 && save("onboarding/step",m_step-1)) { --m_step; emit changed(); }
}
void Onboarding::choosePreset(const QString &preset) {
    if(preset!="minimal" && preset!="glacier" && preset!="classic") return;
    m_appearance->setNavigationPill(preset!="minimal");
    m_appearance->setWindowDecorations(preset=="classic");
}
void Onboarding::finish(bool tutorial) {
    if(m_step!=5 || m_completed) return;
    if(!save("onboarding/tutorialPending",tutorial) || !save("onboarding/version",1)) return;
    m_completed=true;
    m_tutorialPending=tutorial;
    emit changed();
    emit finished();
}
bool Onboarding::tutorialPending() const { return m_tutorialPending; }
void Onboarding::dismissTutorial() { m_tutorialPending=false; save("onboarding/tutorialPending",false); }
void Onboarding::replayTutorial() { m_tutorialPending=true; save("onboarding/tutorialPending",true); }
