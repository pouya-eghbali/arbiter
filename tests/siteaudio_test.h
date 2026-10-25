#pragma once
#include <QObject>
class SiteAudioTest : public QObject {
    Q_OBJECT
private slots:
    void init();
    void rememberedHostRules();
    void privateChangesStayTemporary();
#ifdef ARBITER_TEST_AUDIO_VIEWS
    void openViewsFollowHostAndPrivacyChanges();
#endif
};
