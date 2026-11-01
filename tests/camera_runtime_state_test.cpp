#include "camera_runtime.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int checks = 0;
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    if (argc < 2) return 2;
    QTemporaryDir temp;
    if (!temp.isValid()) return 2;
    const QString aPath = temp.filePath("a.ini"), bPath = temp.filePath("b.ini");
    {
        simplecam::CameraRuntime a(aPath, "Camera A", QString::fromLocal8Bit(argv[1]));
        simplecam::CameraRuntime b(bPath, "Camera B", QString::fromLocal8Bit(argv[1]));
        check(a.isEnabled() && b.isEnabled(), "existing slots default enabled");
        a.presets().setLabel(0, 0, "Legacy shared name");
        b.setSharedPresetOwner(a);
        check(b.presets().presetLabel(0, 0) == "Legacy shared name", "legacy shared view preserved on migration");
        a.presets().setLabel(0, 0, "Camera A name");
        b.presets().setLabel(0, 0, "Camera B name");
        check(a.presets().presetLabel(0, 0) == "Camera A name", "camera A rename isolated");
        check(b.presets().presetLabel(0, 0) == "Camera B name", "camera B rename isolated");
        const int aPhysical = a.presets().slot(0, 0).physicalPreset;
        const int bLastPhysical = b.presets().slot(0, 13).physicalPreset;
        b.presets().swapSlots(0, 0, 0, 13);
        check(a.presets().slot(0, 0).physicalPreset == aPhysical, "camera A swap mapping unaffected");
        check(b.presets().slot(0, 0).physicalPreset == bLastPhysical, "camera B swap mapping changed locally");
        b.presets().setPageDescription(0, "Shared Group");
        check(a.presets().page(0).description == "Shared Group", "group rename remains shared");
        b.setActivePage(1);
        check(a.activePage() == 1 && b.activePage() == 1, "group selection remains shared");
        check(b.presets().addPage(), "shared page added through camera B");
        check(a.presets().pageCount() == 6 && b.presets().pageCount() == 6, "page count shared");
        b.presets().setLabel(5, 0, "Keep this page");
        check(!a.presets().removePageIfBlank(5), "nonblank camera B prevents global page removal");
        a.presets().setLabel(5, 1, "A group six");
        b.presets().swapSlots(5, 0, 0, 1);
        const int bMapBeforeClear = b.presets().slot(5, 0).physicalPreset;
        b.setEnabled(false);
        check(!b.presets().clearPageLabelsForAllCameras(-1), "negative all-camera group rejected");
        check(!b.presets().clearPageLabelsForAllCameras(6), "past-end all-camera group rejected");
        check(a.presets().presetLabel(5, 1) == "A group six", "invalid clear leaves names unchanged");
        check(b.presets().clearPageLabelsForAllCameras(5), "all-camera clear delegates from group member");
        bool allBlank = true;
        for (int slot = 0; slot < simplecam::PresetCatalog::SlotsPerPage; ++slot)
            allBlank = allBlank && a.presets().presetLabel(5, slot).isEmpty() && b.presets().presetLabel(5, slot).isEmpty();
        check(allBlank, "all fourteen labels cleared on enabled and disabled cameras");
        check(b.presets().slot(5, 0).physicalPreset == bMapBeforeClear, "all-camera clear preserves physical mapping");
        check(a.presets().presetLabel(0, 0) == "Camera A name", "all-camera clear preserves other groups");
        check(a.presets().removePageIfBlank(5), "clear-all permits blank extra page deletion");
        check(a.presets().pageCount() == 5 && b.presets().pageCount() == 5, "group deletion shared across cameras");
        check(!a.presets().removePageIfBlank(0), "first five groups remain protected");
        check(!a.presets().removePageIfBlank(5), "cannot delete below five groups");
        check(a.presets().clearPageLabelsForAllCameras(1), "all-camera clear also works through owner");
        check(!b.isEnabled(), "clearing names does not enable a disabled camera");
        bool flashed = false, recalledDisabled = false, storedDisabled = false, connectionDisabled = false;
        b.setPresetRecallObserver([&](int, simplecam::CameraRuntime::PresetRecallOrigin) { flashed = true; });
        b.setEnabled(false);
        b.recallLogicalPreset(1, 0, [&](bool ok, const QString &) { recalledDisabled = !ok; });
        b.storeLogicalPreset(0, 0, [&](bool ok, const QString &) { storedDisabled = !ok; });
        b.testConnection([&](bool ok, const QString &) { connectionDisabled = !ok; });
        check(recalledDisabled && storedDisabled && connectionDisabled && !flashed, "disabled commands rejected before flash or network");
        QSettings saved(bPath, QSettings::IniFormat);
        check(!saved.value("camera/enabled", true).toBool(), "disabled setting persisted");
        check(saved.value("camera/presetCatalogVersion").toInt() == 2, "migration version persisted");
        check(saved.contains("legacyLocalPresetPages/count"), "ignored legacy per-camera data backed up");
    }
    {
        simplecam::CameraRuntime a(aPath, "Camera A", QString::fromLocal8Bit(argv[1]));
        simplecam::CameraRuntime b(bPath, "Camera B", QString::fromLocal8Bit(argv[1]));
        b.setSharedPresetOwner(a);
        check(!b.isEnabled(), "disabled state survives restart");
        check(a.presets().presetLabel(0, 0) == "Camera A name", "camera A catalog survives restart");
        check(b.presets().presetLabel(0, 13) == "Camera B name", "camera B catalog not overwritten on second migration");
        b.setEnabled(true);
        check(b.isEnabled(), "camera can be re-enabled");
    }
    {
        QTcpServer server;
        check(server.listen(QHostAddress::LocalHost, 0), "loopback test server available");
        simplecam::ViscaTcpTransport transport;
        transport.setEndpoint("127.0.0.1", server.serverPort(), 1);
        bool cancelled = false;
        QVector<simplecam::ViscaTcpTransport::FrameCommand> commands;
        commands.append({QByteArray::fromHex("81090400ff"), true, {}});
        transport.enqueue(commands, [&](bool ok, const QString &) { cancelled = !ok; }, 50);
        transport.setEnabled(false);
        transport.setEnabled(true);
        QEventLoop loop;
        QTimer::singleShot(120, &loop, &QEventLoop::quit);
        loop.exec();
        check(cancelled, "pending delayed request cancelled on disable");
        check(!server.hasPendingConnections(), "old delay cannot reconnect after re-enable");
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}

