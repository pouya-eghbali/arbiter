#include "nativeshare.h"
#include <QMenu>
#include <QDesktopServices>
#include <QUrlQuery>
#include <QTimer>
#ifdef ARBITER_KDE_PURPOSE
#include <Purpose/Menu>
#include <Purpose/AlternativesModel>
#include <QJsonObject>
#include <QJsonArray>
#endif
namespace {
struct LinuxShare { QPointer<NativeShare> controller; QMenu *menu=nullptr; };
}
void *createNativeShare(NativeShare *controller,QString *) {
    auto *state=new LinuxShare; state->controller=controller;
    const quint64 generation=controller->generation();
#ifdef ARBITER_KDE_PURPOSE
    auto *menu=new Purpose::Menu;
    menu->model()->setPluginType(QStringLiteral("ShareUrl"));
    menu->model()->setInputData(QJsonObject{{"urls",QJsonArray{controller->link().toString()}},{"title",controller->title()}});
    menu->reload(); state->menu=menu;
    QObject::connect(menu,&Purpose::Menu::aboutToShare,controller,[menu] { menu->setProperty("sharing",true); });
    QObject::connect(menu,&Purpose::Menu::finished,controller,[controller,state,generation](const QJsonObject &,int,const QString &) { controller->finished(state,generation); });
#else
    // There is no universal XDG share sheet. Use desktop mail handling and
    // clipboard actions when Purpose's installed share providers are absent.
    state->menu=new QMenu;
    const QUrl link=controller->link(); const QString title=controller->title();
    QObject::connect(state->menu->addAction(QObject::tr("Email link…")),&QAction::triggered,controller,[controller,link,title] {
        if (!controller->canShare()) return;
        QUrl email("mailto:");
        email.setQuery("subject="+QString::fromLatin1(QUrl::toPercentEncoding(title))+"&body="+QString::fromLatin1(QUrl::toPercentEncoding(link.toString(QUrl::FullyEncoded))),QUrl::StrictMode);
        QDesktopServices::openUrl(email);
    });
#endif
    auto *copy=new QAction(QObject::tr("Copy link"),state->menu);
    state->menu->insertAction(state->menu->actions().isEmpty() ? nullptr : state->menu->actions().first(),copy);
    QObject::connect(copy,&QAction::triggered,controller,[controller,generation] { controller->copyLink(generation); });
    QObject::connect(state->menu,&QMenu::aboutToHide,controller,[controller,state,generation,menu=QPointer<QMenu>(state->menu)] {
        QTimer::singleShot(0,controller,[controller,state,generation,menu] { if (menu && !menu->property("sharing").toBool()) controller->finished(state,generation); });
    });
    return state;
}
bool presentNativeShare(void *raw,const QPointF &anchor,QString *) {
    auto *state=static_cast<LinuxShare *>(raw);
    if (!state->controller || !state->controller->canShare()) return false;
    state->menu->popup(state->controller->host()->mapToGlobal(anchor.toPoint())); return true;
}
void destroyNativeShare(void *raw) { auto *state=static_cast<LinuxShare *>(raw); state->menu->close(); state->menu->deleteLater(); delete state; }
