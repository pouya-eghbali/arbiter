#include "nativeshare.h"
#include <QImage>
#include <memory>
#include <QPainter>
#include <QBuffer>
#include <QCoreApplication>
#include <windows.h>
#include <roapi.h>
#include <shobjidl_core.h>
#include <shlwapi.h>
#include <shcore.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.h>
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Storage::Streams;
namespace {
struct WindowsShare {
    QPointer<NativeShare> controller;
    winrt::com_ptr<IDataTransferManagerInterop> interop;
    DataTransferManager manager{nullptr};
    DataTransferManager::DataRequested_revoker data;
    DataTransferManager::ShareProvidersRequested_revoker providers;
    bool apartmentInitialized=false;
    ~WindowsShare() {
        providers.revoke(); data.revoke(); manager=nullptr; interop=nullptr;
        if (apartmentInitialized) RoUninitialize();
    }
    // Keep the current session registered until a new share/navigation or window
    // closure: Windows owns sheet dismissal and exposes no dismissal event.
};
RandomAccessStreamReference copyIcon() {
    QImage image(32,32,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    QPainter painter(&image); painter.setPen(QPen(Qt::white,2)); painter.drawRoundedRect(7,9,15,18,2,2); painter.drawPolyline(QPolygon{QPoint(12,8),QPoint(12,4),QPoint(27,4),QPoint(27,22),QPoint(24,22)}); painter.end();
    QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG");
    winrt::com_ptr<IStream> stream; stream.attach(SHCreateMemStream(reinterpret_cast<const BYTE *>(png.constData()),UINT(png.size())));
    if (!stream) winrt::throw_hresult(E_OUTOFMEMORY);
    IRandomAccessStream random{nullptr};
    winrt::check_hresult(CreateRandomAccessStreamOverStream(stream.get(),BSOS_DEFAULT,winrt::guid_of<IRandomAccessStream>(),winrt::put_abi(random)));
    return RandomAccessStreamReference::CreateFromStream(random);
}
}
void *createNativeShare(NativeShare *controller,QString *error) {
    try {
        auto state=std::make_unique<WindowsShare>(); state->controller=controller;
        const HRESULT initialized=RoInitialize(RO_INIT_SINGLETHREADED);
        if (FAILED(initialized) && initialized!=RPC_E_CHANGED_MODE) winrt::throw_hresult(initialized);
        state->apartmentInitialized=SUCCEEDED(initialized);
        if (!DataTransferManager::IsSupported()) { *error=QObject::tr("Windows sharing is unavailable on this device."); return nullptr; }
        state->interop=winrt::get_activation_factory<DataTransferManager,IDataTransferManagerInterop>();
        winrt::check_hresult(state->interop->GetForWindow(reinterpret_cast<HWND>(controller->host()->winId()),winrt::guid_of<winrt::Windows::ApplicationModel::DataTransfer::IDataTransferManager>(),winrt::put_abi(state->manager)));
        QPointer<NativeShare> guard(controller);
        const quint64 generation=controller->generation();
        const QUrl link=controller->link(); const QString title=controller->title().isEmpty() ? link.host() : controller->title();
        state->data=state->manager.DataRequested(winrt::auto_revoke,[guard,link,title,generation](auto const &,DataRequestedEventArgs const &args) {
            if (!guard || guard->generation()!=generation || !guard->canShare()) { args.Request().FailWithDisplayText(L"This page is no longer available for sharing."); return; }
            const auto data=args.Request().Data();
            data.Properties().Title(winrt::hstring(title.toStdWString()));
            data.SetText(winrt::hstring(link.toString(QUrl::FullyEncoded).toStdWString()));
            if (link.scheme()=="https" || link.scheme()=="http") data.SetWebLink(winrt::Windows::Foundation::Uri(winrt::hstring(link.toString(QUrl::FullyEncoded).toStdWString())));
        });
        const auto icon=copyIcon();
        state->providers=state->manager.ShareProvidersRequested(winrt::auto_revoke,[guard,icon,generation](auto const &,ShareProvidersRequestedEventArgs const &args) {
            if (!guard || guard->generation()!=generation || !guard->canShare()) return;
            args.Providers().Append(ShareProvider(winrt::hstring(QObject::tr("Copy link").toStdWString()),icon,winrt::Windows::UI::Color{255,40,104,184},[guard,generation](ShareProviderOperation const &operation) {
                QMetaObject::invokeMethod(QCoreApplication::instance(),[guard,operation,generation] {
                    if (guard) guard->copyLink(generation);
                    try { operation.ReportCompleted(); } catch (const winrt::hresult_error &) {}
                },Qt::QueuedConnection);
            }));
        });
        return state.release();
    } catch (const winrt::hresult_error &failure) { *error=QString::fromStdWString(std::wstring(failure.message())); return nullptr; }
}
bool presentNativeShare(void *raw,const QPointF &,QString *error) {
    auto *state=static_cast<WindowsShare *>(raw);
    try {
        if (!state->controller || !state->controller->canShare()) return false;
        winrt::check_hresult(state->interop->ShowShareUIForWindow(reinterpret_cast<HWND>(state->controller->host()->winId())));
        return true;
    } catch (const winrt::hresult_error &failure) { *error=QString::fromStdWString(std::wstring(failure.message())); return false; }
}
void destroyNativeShare(void *state) { delete static_cast<WindowsShare *>(state); }
