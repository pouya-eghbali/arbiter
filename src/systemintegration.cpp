#include "systemintegration.h"
#include <QApplication>
#include <QStyle>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QProcess>
#include <QTimer>
#include <QPointer>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlwapi.h>
#endif
namespace {
#ifndef Q_OS_MACOS
#ifdef Q_OS_WIN
QString windowsCommand() { return QStringLiteral("\"%1\" \"%2\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),QStringLiteral("%1")); }
QString registerBrowser() {
    QSettings classes("HKEY_CURRENT_USER\\Software\\Classes\\ArbiterHTML",QSettings::NativeFormat);
    classes.setValue(".","Arbiter web link"); classes.setValue("URL Protocol","");
    classes.setValue("shell/open/command/.",windowsCommand());
    const QString base="Software\\Clients\\StartMenuInternet\\Arbiter";
    QSettings client("HKEY_CURRENT_USER\\"+base,QSettings::NativeFormat);
    client.setValue(".","Arbiter");
    client.setValue("shell/open/command/.",QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    client.setValue("Capabilities/ApplicationName","Arbiter");
    client.setValue("Capabilities/ApplicationDescription","A distraction-free web browser");
    for(const auto &scheme:{"http","https"}) client.setValue(QString("Capabilities/URLAssociations/")+scheme,"ArbiterHTML");

    QSettings apps("HKEY_CURRENT_USER\\Software\\RegisteredApplications",QSettings::NativeFormat);
    apps.setValue("Arbiter",base+"\\Capabilities");
    classes.sync(); client.sync(); apps.sync();
    return classes.status()==QSettings::NoError && client.status()==QSettings::NoError && apps.status()==QSettings::NoError ? QString() : QObject::tr("Could not register Arbiter. You can choose a default browser later in Windows Settings.");
}
bool windowsIsDefault() {
    for(const auto &scheme:{L"http",L"https"}) {
        wchar_t executable[32768]; DWORD size=32768;
        if(FAILED(AssocQueryStringW(ASSOCF_IS_PROTOCOL,ASSOCSTR_EXECUTABLE,scheme,L"open",executable,&size))
           || QFileInfo(QString::fromWCharArray(executable)).canonicalFilePath().compare(QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath(),Qt::CaseInsensitive)!=0) return false;
    }
    return true;
}
#else
QString desktopEntry() {
    auto data=qEnvironmentVariable("XDG_DATA_HOME");
    if(!QDir::isAbsolutePath(data)) data=QDir::homePath()+"/.local/share";
    return data+"/applications/com.arbiter.browser.desktop";
}
QString registerBrowser() {
    const auto file=desktopEntry();
    if(!QDir().mkpath(QFileInfo(file).absolutePath())) return QObject::tr("Could not create Arbiter's desktop entry.");
    QString executable=QCoreApplication::applicationFilePath();
    for(const auto character:executable) if(character.unicode()<32) return QObject::tr("This application path cannot be used in a desktop entry.");
    executable.replace("\\","\\\\").replace("\"","\\\"").replace("`","\\`").replace("$","\\$").replace("%","%%");
    // Desktop-entry strings and Exec arguments have separate escape layers.
    executable.replace("\\","\\\\");
    const QByteArray bytes=(QStringLiteral("[Desktop Entry]\nType=Application\nName=Arbiter\nComment=A distraction-free web browser\nExec=\"%1\" %u\nTerminal=false\nCategories=Network;WebBrowser;\nMimeType=x-scheme-handler/http;x-scheme-handler/https;\n").arg(executable)).toUtf8();
    QSaveFile output(file);
    if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit()) return QObject::tr("Could not register Arbiter's desktop entry.");
    return {};
}
#endif
#endif
}
SystemIntegration::SystemIntegration(QObject *parent):QObject(parent) {
}
bool SystemIntegration::reducedMotion() const {
#ifdef Q_OS_MACOS
    return macReducedMotion();
#elif defined(Q_OS_WIN)
    BOOL enabled=TRUE;
    return SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0) && !enabled;
#else
    return !qApp->style()->styleHint(QStyle::SH_Widget_Animate);
#endif
}
void SystemIntegration::finishRequest(const QString &error) {
    m_busy=false;
    m_message=error;
    emit changed();
    refreshDefault();
}
void SystemIntegration::requestDefault() {
    if(m_busy || m_default) return;
    m_busy=true; m_message.clear(); emit changed();
#ifdef Q_OS_MACOS
    QPointer<SystemIntegration> guard(this);
    macRequestDefaultBrowser([guard](const QString &error) {
        if(guard) QMetaObject::invokeMethod(guard,[guard,error] { if(guard) guard->finishRequest(error); },Qt::QueuedConnection);
    });
#else
    const auto error=registerBrowser();
    if(!error.isEmpty()) { finishRequest(error); return; }
#ifdef Q_OS_WIN
    finishRequest(QDesktopServices::openUrl(QUrl("ms-settings:defaultapps?registeredAppUser=Arbiter"))
        ? tr("Choose Arbiter in Windows Settings, then return here to continue.") : tr("Could not open Windows Settings. You can set a default browser there later."));
#else
    auto *process=new QProcess(this);
    auto *timeout=new QTimer(process); timeout->setSingleShot(true); timeout->setInterval(15000);
    connect(timeout,&QTimer::timeout,process,&QProcess::kill);
    connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error) {
        if(error==QProcess::FailedToStart) { process->deleteLater(); finishRequest(tr("xdg-settings is unavailable. Choose Arbiter in your desktop's default application settings.")); }
    });
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process](int code,QProcess::ExitStatus status) {
        process->deleteLater(); finishRequest(code==0 && status==QProcess::NormalExit ? QString() : tr("Your desktop could not change the default browser. Choose Arbiter in its default application settings."));
    });
    process->start("xdg-settings",{"set","default-web-browser","com.arbiter.browser.desktop"}); timeout->start();
#endif
#endif
}
void SystemIntegration::refreshDefault() {
    if(m_busy) return;
    if(m_refreshingDefault) { m_refreshAgain=true; return; }
#ifdef Q_OS_MACOS
    m_default=macIsDefaultBrowser(); emit changed();
#elif defined(Q_OS_WIN)
    m_default=windowsIsDefault(); emit changed();
#else
    // Read only on the default-browser page or after returning from system UI.
    m_refreshingDefault=true;
    auto *process=new QProcess(this);
    auto *timeout=new QTimer(process); timeout->setSingleShot(true); timeout->setInterval(3000);
    connect(timeout,&QTimer::timeout,process,&QProcess::kill);
    connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error) {
        if(error==QProcess::FailedToStart) { m_refreshingDefault=false; m_refreshAgain=false; process->deleteLater(); }
    });
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process](int code,QProcess::ExitStatus status) {
        m_refreshingDefault=false;
        const bool value=code==0 && status==QProcess::NormalExit && process->readAllStandardOutput().trimmed()=="com.arbiter.browser.desktop";
        process->deleteLater();
        if(m_refreshAgain) { m_refreshAgain=false; refreshDefault(); return; }
        if(!m_busy && m_default!=value) { m_default=value; emit changed(); }
    });
    process->start("xdg-settings",{"get","default-web-browser"}); timeout->start();
#endif
}
