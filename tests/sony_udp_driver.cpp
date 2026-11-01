#include "visca_udp_transport.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <memory>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
using namespace simplecam;
int main(int argc, char **argv)
{
    QCoreApplication app(argc,argv);
    if (argc < 5) return 3;
    const QString mode = QString::fromLocal8Bit(argv[1]);
    const quint16 port = QString::fromLocal8Bit(argv[2]).toUShort();
    const quint16 local = QString::fromLocal8Bit(argv[3]).toUShort();
    const QString profile = QString::fromLocal8Bit(argv[4]);
    QTemporaryDir dir;
    QFile file(profile); if (!file.open(QIODevice::ReadOnly)) return 3;
    auto root = QJsonDocument::fromJson(file.readAll()).object();
    auto connections = root["connections"].toArray(); auto connection = connections[0].toObject();
    connection["defaultPort"] = int(port);
    auto udpDefaults = connection["udp"].toObject(); udpDefaults["localPort"] = int(local); connection["udp"] = udpDefaults;
    auto response = connection["response"].toObject(); response["timeoutMs"] = 150; connection["response"] = response;
    connections[0] = connection; root["connections"] = connections;
    const auto testProfile = dir.filePath("sony.json");
    QFile output(testProfile); if (!output.open(QIODevice::WriteOnly)) return 3;
    output.write(QJsonDocument(root).toJson()); output.close();
    CameraRuntime runtime(dir.filePath("camera.ini"),"Sony",testProfile);
    runtime.setHost("127.0.0.1");
    ViscaUdpTransport udp;
    udp.configure("127.0.0.1",port,local,100,150);
    auto done = [&](bool ok,const QString &message) {
        std::cout << (ok ? "PASS " : "FAIL ") << message.toStdString() << std::endl;
        app.exit(ok ? 0 : 1);
    };
    QTimer::singleShot(5000,&app,[&]() { done(false,"Harness watchdog"); });
    ViscaUdpTransport second;
    QTimer::singleShot(0,&app,[&]() {
        if (mode == "multi") {
            second.configure("127.0.0.2",port,local,100,150);
            auto count = std::make_shared<int>(0);
            auto both = [&,count](bool ok,const QString &message) {
                if (!ok) { done(false,message); return; }
                if (++*count == 2) done(true,"two cameras share one listener");
            };
            udp.enqueue({{QByteArray::fromHex("81090400ff"),true}},both);
            second.enqueue({{QByteArray::fromHex("81090447ff"),true}},both);
            return;
        }
        if (mode == "duplicate") {
            udp.enqueue({{QByteArray::fromHex("81090400ff"),true}});
            second.configure("127.0.0.1",port,local,100,150);
            second.enqueue({{QByteArray::fromHex("81090447ff"),true}},done); return;
        }
        if (mode == "disable" || mode == "disable-focus" || mode == "disable-pan") {
            const auto start = mode == "disable-focus" ? QByteArray::fromHex("8101040802ff") :
                mode == "disable-pan" ? QByteArray::fromHex("8101060108080103ff") : QByteArray::fromHex("8101040720ff");
            udp.enqueue({{start,false}},[&](bool ok,const QString &message) {
                if (!ok) { done(false,message); return; }
                udp.setEnabled(false); done(true,"disabled");
            }); return;
        }
        if (mode == "runtime-actions") {
            runtime.presets().setLabel(0,0,"Disposable test preset");
            using Call = std::function<void(CameraRuntime::Completion)>;
            auto actions = std::make_shared<QVector<Call>>();
            actions->append([&](auto cb) { runtime.setCameraPower(true,cb); });
            actions->append([&](auto cb) { runtime.panTilt(8,8,1,3,cb); });
            actions->append([&](auto cb) { runtime.stopPanTilt(8,8,cb); });
            actions->append([&](auto cb) { runtime.home(cb); });
            actions->append([&](auto cb) { runtime.setZoomPosition(100,cb); });
            actions->append([&](auto cb) { runtime.setManualFocus(true,cb); });
            actions->append([&](auto cb) { runtime.focusDrive(true,cb); });
            actions->append([&](auto cb) { runtime.stopFocus(cb); });
            actions->append([&](auto cb) { runtime.setManualFocus(false,cb); });
            actions->append([&](auto cb) { runtime.backlight(true,cb); });
            actions->append([&](auto cb) { runtime.backlight(false,cb); });
            actions->append([&](auto cb) { runtime.exposureCompensation(true,cb); });
            actions->append([&](auto cb) { runtime.exposureCompensation(false,cb); });
            actions->append([&](auto cb) { runtime.storeLogicalPreset(0,0,cb); });
            actions->append([&](auto cb) { runtime.recallLogicalPreset(1,0,cb); });
            actions->append([&](auto cb) { runtime.recallLogicalPreset(1,0,cb); });
            actions->append([&](auto cb) { runtime.setCameraPower(false,cb); });
            auto index = std::make_shared<int>(0);
            auto next = std::make_shared<CameraRuntime::Completion>();
            *next = [&,actions,index,next](bool ok,const QString &message) {
                if (!ok || *index == actions->size()) {
                    auto finish = done; *next = {}; finish(ok,message); return;
                }
                const auto call = actions->at((*index)++);
                call(*next);
            };
            (*next)(true,"start"); return;
        }
        if (mode == "runtime-init") {
            runtime.setZoomPositionObserver([&](int value) { done(value >= 0,"startup readback"); });
            runtime.initializeZoomPosition(); return;
        }
        if (mode == "runtime-zoom") {
            runtime.testConnection([&](bool ok,const QString &msg) {
                if (!ok) { done(ok,msg); return; }
                runtime.zoom(true,[&](bool accepted,const QString &message) {
                    if (!accepted) { done(false,message); return; }
                    QTimer::singleShot(250,&app,[&]() { runtime.stopZoom(done); });
                });
            }); return;
        }
        if (mode == "priority") {
            udp.enqueue({{QByteArray::fromHex("8101040720ff"),false}},[&](bool,const QString &) {});
            QTimer::singleShot(150,&app,[&]() {
                // Poll never answers: STOP must preempt the active inquiry.
                udp.enqueue({{QByteArray::fromHex("81090447ff"),true}},[&](bool,const QString &) {});
            });
            QTimer::singleShot(180,&app,[&]() {
                udp.enqueue({{QByteArray::fromHex("8101040700ff"),true}},done,0,true);
            }); return;
        }
        QVector<ViscaTcpTransport::FrameCommand> commands;
        for (int i=0;i<4;++i) commands.append({QByteArray::fromHex(i%2 ? "81090447ff" : "81090400ff"),true});
        udp.enqueue(commands,done);
    });
    return app.exec();
}
