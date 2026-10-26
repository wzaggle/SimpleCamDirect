#include "camera_runtime.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QPointer>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <memory>

// Two loopback cameras with explicit ACK/completion gates. No real camera
// commands leave localhost. The test controls protocol progress, not sleeps.
struct FakeCamera : QTcpServer {
    explicit FakeCamera(QObject *parent = nullptr) : QTcpServer(parent)
    {
        listen(QHostAddress::LocalHost, 0);
        QObject::connect(this, &QTcpServer::newConnection, this, [this]() {
            while (hasPendingConnections()) {
                QTcpSocket *client = nextPendingConnection();
                auto bytes = std::make_shared<QByteArray>();
                QObject::connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);
                QObject::connect(client, &QTcpSocket::readyRead, client, [this, client, bytes]() {
                    bytes->append(client->readAll());
                    if (!bytes->contains(char(0xff))) return;
                    const QByteArray frame = *bytes;
                    bytes->clear();
                    if (frame.startsWith(QByteArray::fromHex("8101043f02"))) {
                        recalls.append(frame);
                        held = client;
                        client->write(QByteArray::fromHex("9041ff")); // ACK: stay busy
                        client->flush();
                        if (onRecall) onRecall();
                    } else if (frame == QByteArray::fromHex("81090447ff")) {
                        ++inquiries;
                        if (holdInquiries) pendingInquiries.append(client);
                        else { client->write(zoomReply); client->flush(); }
                    } else {
                        ++otherCommands;
                        client->write(QByteArray::fromHex("906002ff"));
                        client->flush();
                    }
                });
            }
        });
    }
    void completeRecall()
    {
        if (held) {
            held->write(QByteArray::fromHex("9051ff"));
            held->flush();
            held = nullptr;
        }
    }
    void completeInquiries()
    {
        const auto pending = pendingInquiries;
        pendingInquiries.clear();
        for (const auto &client : pending) {
            if (client) { client->write(zoomReply); client->flush(); }
        }
    }
    QByteArray zoomReply = QByteArray::fromHex("905002000000ff");
    bool holdInquiries = false;
    QVector<QPointer<QTcpSocket>> pendingInquiries;
    int otherCommands = 0;
    QPointer<QTcpSocket> held;
    QVector<QByteArray> recalls;
    int inquiries = 0;
    std::function<void()> onRecall;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc < 2) return 2;
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *message) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    QTemporaryDir temp;
    if (!temp.isValid()) return 2;
    FakeCamera serverA, serverB;
    check(serverA.isListening() && serverB.isListening(), "both localhost cameras listening");
    simplecam::CameraRuntime a(temp.filePath("a.ini"), "A", QString::fromLocal8Bit(argv[1]));
    simplecam::CameraRuntime b(temp.filePath("b.ini"), "B", QString::fromLocal8Bit(argv[1]));
    a.setHost("127.0.0.1"); a.setPort(serverA.serverPort());
    b.setHost("127.0.0.1"); b.setPort(serverB.serverPort());
    b.setSharedPresetOwner(a);
    a.presets().setLabel(0, 0, "A keep");
    b.presets().setLabel(0, 0, "B clear");
    b.presets().swapSlots(0, 0, 0, 13);
    const int physicalBefore = b.presets().slot(0, 0).physicalPreset;
    check(b.presets().clearPageLabels(0), "clear selected camera group");
    check(a.presets().presetLabel(0, 0) == "A keep", "other camera group unchanged");
    bool blank = true;
    for (int i = 0; i < 14; ++i) blank = blank && b.presets().slot(0, i).label.isEmpty();
    check(blank, "all fourteen labels blanked");
    check(b.presets().slot(0, 0).physicalPreset == physicalBefore, "clear retained swapped physical mapping");
    check(!b.presets().clearPageLabels(-1), "invalid group rejected");
    check(a.lastZoomSliderValue() == -1 && b.lastZoomSliderValue() == -1, "no invented zoom before inquiry");
    // Group two must resolve to physical preset 15 on both cameras.
    a.setActivePage(1);
    bool firstA = false, secondA = false, doneB = false, sawConcurrent = false;
    int aZoom = -1, bZoom = -1;
    a.setZoomPositionObserver([&](int value) { aZoom = value; });
    b.setZoomPositionObserver([&](int value) { bZoom = value; });
    QEventLoop loop;
    serverA.onRecall = [&]() {
        if (serverA.recalls.size() == 1 && doneB) serverA.completeRecall();
        else if (serverA.recalls.size() == 2) {
            check(serverA.inquiries == 1 && firstA, "second same-camera recall waits for completion and zoom");
            serverA.completeRecall();
        }
    };
    serverB.onRecall = [&]() { serverB.completeRecall(); };
    a.recallLogicalPreset(1, 0, [&](bool ok, const QString &) { firstA = ok; });
    a.recallLogicalPreset(2, 0, [&](bool ok, const QString &) { secondA = ok; loop.quit(); });
    b.recallLogicalPreset(1, 0, [&](bool ok, const QString &) {
        doneB = ok;
        sawConcurrent = !firstA && !secondA && serverA.inquiries == 0;
        check(sawConcurrent, "camera B completes while camera A is waiting");
        check(aZoom == -1, "ACK did not trigger camera A zoom inquiry");
        if (!serverA.recalls.isEmpty()) serverA.completeRecall();
    });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    watchdog.start(5000); // failure bound only; protocol gates determine order
    loop.exec();
    check(firstA && secondA && doneB && sawConcurrent, "all independently queued recalls completed");
    check(serverA.recalls.size() == 2 && serverB.recalls.size() == 1, "all requests sent without coalescing");
    check(serverA.inquiries == 2 && serverB.inquiries == 1, "each recall queried final zoom exactly once");
    check(aZoom == 100 && bZoom == 100, "zoom position 0x2000 mapped onto optical slider");
    check(a.lastZoomSliderValue() == aZoom && b.lastZoomSliderValue() == bZoom, "per-camera zoom cache matches replies");
    if (!serverA.recalls.isEmpty())
        check(serverA.recalls.first() == QByteArray::fromHex("8101043f020eff"), "shared group two resolves to wire preset fourteen");
    // Startup queries are explicit (constructors stay quiet), read-only and
    // independent per camera. Hold C's reply to prove D can initialize first.
    {
        FakeCamera serverC, serverD;
        serverC.zoomReply = QByteArray::fromHex("905001000000ff");
        serverD.zoomReply = QByteArray::fromHex("905003000000ff");
        serverC.holdInquiries = true;
        const QString cPath = temp.filePath("startup-c.ini");
        const QString dPath = temp.filePath("startup-d.ini");
        const QString ePath = temp.filePath("startup-disabled.ini");
        for (const QString &path : {cPath, dPath, ePath}) {
            QSettings config(path, QSettings::IniFormat);
            config.setValue("camera/host", "127.0.0.1");
            config.setValue("camera/port", path == dPath ? serverD.serverPort() : serverC.serverPort());
            config.setValue("camera/enabled", path != ePath);
        }
        simplecam::CameraRuntime c(cPath, "C", QString::fromLocal8Bit(argv[1]));
        simplecam::CameraRuntime d(dPath, "D", QString::fromLocal8Bit(argv[1]));
        simplecam::CameraRuntime e(ePath, "E", QString::fromLocal8Bit(argv[1]));
        QVector<int> cUpdates;
        c.setZoomPositionObserver([&](int value) { cUpdates.append(value); });
        auto until = [&](const std::function<bool()> &condition) {
            if (condition()) return true;
            QEventLoop waiting;
            QTimer poll, deadline;
            QObject::connect(&poll, &QTimer::timeout, &waiting, [&]() { if (condition()) waiting.quit(); });
            deadline.setSingleShot(true);
            QObject::connect(&deadline, &QTimer::timeout, &waiting, &QEventLoop::quit);
            poll.start(2); deadline.start(1500); waiting.exec();
            return condition();
        };
        check(c.lastZoomSliderValue() == -1 && d.lastZoomSliderValue() == -1, "startup cache initially unknown");
        c.initializeZoomPosition(); c.initializeZoomPosition();
        d.initializeZoomPosition(); e.initializeZoomPosition();
        check(until([&]() { return serverC.inquiries == 1 && d.lastZoomSliderValue() == 150; }),
              "second camera initializes while first camera reply is held");
        check(c.lastZoomSliderValue() == -1, "held startup reply cannot invent a position");
        check(serverC.inquiries == 1, "startup is idempotent and disabled slot sends no inquiry");
        serverC.completeInquiries();
        check(until([&]() { return c.lastZoomSliderValue() == 50; }), "startup reply populates first camera cache");
        check(cUpdates == QVector<int>{50}, "startup notifies slider observer once");

        QString profileError;
        check(c.setProfilePath(QString::fromLocal8Bit(argv[1]), &profileError), "profile reload accepted");
        check(until([&]() { return serverC.inquiries == 2; }), "profile change refreshes camera zoom");
        const int beforeChange = cUpdates.size();
        c.setPort(serverD.serverPort());
        check(c.lastZoomSliderValue() == -1, "endpoint change invalidates old cache");
        serverC.completeInquiries();
        check(until([&]() { return c.lastZoomSliderValue() == 150; }), "changed endpoint supplies new zoom");
        check(cUpdates.size() == beforeChange + 1 && cUpdates.last() == 150, "late old-endpoint reply is ignored");
        check(serverD.inquiries == 2, "endpoint change coalesces to one new inquiry");

        e.setEnabled(true);
        check(until([&]() { return serverC.inquiries == 3; }), "enabling initialized slot queries its camera");
        serverC.completeInquiries();
        check(until([&]() { return e.lastZoomSliderValue() == 50; }), "enabled slot receives current position");

        serverD.zoomReply = QByteArray::fromHex("906041ff");
        c.setProfilePath(QString::fromLocal8Bit(argv[1]), &profileError);
        check(until([&]() { return serverD.inquiries == 3; }), "camera rejection test inquiry sent");
        serverD.zoomReply = QByteArray::fromHex("905003000000ff");
        c.setProfilePath(QString::fromLocal8Bit(argv[1]), &profileError);
        check(until([&]() { return c.lastZoomSliderValue() == 150; }), "queue recovers after failed initialization");
        check(c.isEnabled() && c.canSetZoomPosition(), "query failure leaves camera controls available");
        check(serverC.recalls.isEmpty() && serverD.recalls.isEmpty() && serverC.otherCommands == 0 && serverD.otherCommands == 0,
              "initialization and refresh only send inquiries, never movement");
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
