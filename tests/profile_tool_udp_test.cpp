#include "profile_test_tool.hpp"
#include "camera_runtime.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QJsonArray>
#include <QAbstractButton>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QHostAddress>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkDatagram>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <cstdio>
#include <cstdlib>

namespace simplecam {
class ProfileToolTest {
    static inline int checks = 0;
    static void check(bool ok, const char *label) {
        ++checks;
        if (!ok) { std::fprintf(stderr,"FAIL: %s\n",label); std::exit(1); }
    }
    static bool until(const std::function<bool()> &condition, int milliseconds = 4000) {
        QElapsedTimer timer; timer.start();
        while (!condition() && timer.elapsed() < milliseconds) QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        return condition();
    }
public:
    static int run(const QString &sonyPath, const QString &tcpPath) {
        QTemporaryDir dir;
        check(dir.isValid(), "temporary files");
        QUdpSocket server, replies, freePort;
        check(server.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)), "camera server");
        check(replies.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)), "alternate reply source port");
        check(freePort.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)), "listener allocation");
        const quint16 local = freePort.localPort(); freePort.close();
        bool drop = false, standby = false, reject = false;
        QVector<QByteArray> sent;
        QVector<qint64> times;
        QElapsedTimer clock; clock.start();
        QObject::connect(&server, &QUdpSocket::readyRead, &server, [&]() {
            while (server.hasPendingDatagrams()) {
                const auto data = server.receiveDatagram(); const QByteArray wire = data.data();
                sent.append(wire); times.append(clock.elapsed());
                if (drop) continue;
                QByteArray response = wire.left(8);
                const auto payload = wire.mid(8);
                if (wire.left(2) == QByteArray::fromHex("0200")) {
                    response[0] = 2; response[1] = 1; response[2] = 0; response[3] = 1;
                    response += char(1);
                } else {
                    response[0] = 1; response[1] = 0x11;
                    QByteArray value;
                    if (reject) value = QByteArray::fromHex("906002ff");
                    else if (payload == QByteArray::fromHex("81090400ff")) value = QByteArray::fromHex(standby ? "905003ff" : "905002ff");
                    else if (payload == QByteArray::fromHex("81090447ff")) value = QByteArray::fromHex("905000010203ff");
                    else if (payload == QByteArray::fromHex("81090612ff")) value = QByteArray::fromHex("90500000000000000000ff");
                    else {
                        value = QByteArray::fromHex("9051ff");
                        if (payload == QByteArray::fromHex("8101040002ff")) standby = false;
                        if (payload == QByteArray::fromHex("8101040003ff")) standby = true;
                    }
                    response[2] = 0; response[3] = char(value.size()); response += value;
                }
                // Stale/wrong-type packet first; it must not complete the request.
                QByteArray stale = response; stale[7] = char(0x7f);
                if (wire.left(2) != QByteArray::fromHex("0200")) replies.writeDatagram(stale, data.senderAddress(), data.senderPort());
                replies.writeDatagram(response, data.senderAddress(), data.senderPort());
            }
        });
        CameraProfileDocument profile; QString error;
        check(profile.load(sonyPath,&error), "Sony profile loads");
        const auto original = profile.root();
        check(!profile.setSonyUdpSettings(0,local,100,100,3000,&error) && profile.root() == original, "invalid timing/port edit is atomic");
        check(!profile.setSonyUdpSettings(52381,local,2001,100,3000,&error), "request spacing bound");
        check(!profile.setSonyUdpSettings(52381,local,100,2001,3000,&error), "reset delay bound");
        check(!profile.setSonyUdpSettings(52381,local,100,100,49,&error), "reply timeout bound");
        check(profile.setSonyUdpSettings(server.localPort(),local,65,110,200,&error), "valid draft timing edit");
        const QString variant = dir.filePath("sony-test.camera.cfg");
        check(profile.saveAs(variant,&error), "profile variant saved");
        CameraProfileDocument restored;
        check(restored.load(variant,&error) && restored.id() != QStringLiteral("sony.srg-300h.visca-udp"), "saved copy has new identity");
        check(restored.sonyUdpConnection().value("udp").toObject().value("minimumRequestGapMs").toInt() == 65, "timings round-trip");
        CameraRuntime runtime(dir.filePath("slot.ini"),QStringLiteral("Camera"),variant);
        runtime.setHost(QStringLiteral("127.0.0.1")); setCameraRuntime(0,&runtime);
        bool dockDone=false, dockOk=false;
        runtime.testConnection([&](bool ok,const QString &) { dockDone=true; dockOk=ok; });
        check(until([&] { return dockDone; }) && dockOk, "normal runtime acquires listener before tool");
        CameraProfileTestDialog tool(variant);
        tool.host_->setText(QStringLiteral("127.0.0.1"));
        check(tool.sonyUdp() && tool.supportedTransport(), "Sony UDP selectable in tool");
        check(tool.localPort_->value()==local && tool.requestGap_->value()==65 && tool.resetDelay_->value()==110 && tool.replyTimeout_->value()==200, "draft widgets populated");
        check(tool.zoomSpeed_->maximum()==7 && tool.zoomPosition_->maximum()==16384 && tool.presetNumber_->maximum()==255, "Sony token ranges");
        const QString screenshot = qEnvironmentVariable("PROFILE_TOOL_SCREENSHOT");
        if (!screenshot.isEmpty()) { tool.show(); QCoreApplication::processEvents(); tool.grab().save(screenshot); tool.hide(); }
        sent.clear(); times.clear();
        tool.testConnection();
        check(until([&] { return !tool.ioBusy_; }) && tool.status_->text().contains("ON"), "Test UDP uses power inquiry");
        check(sent.size()==2 && sent[0].left(2)==QByteArray::fromHex("0200") && sent[1].mid(8)==QByteArray::fromHex("81090400ff"), "reset then power only; no power change");
        check(times[1]-times[0]>=100, "profile reset setup delay honored");
        check(runtime.profileTestActive() && tool.releaseButton_->isEnabled(), "camera borrowed and release available");
        check(tool.trace_->toPlainText().contains(QStringLiteral(":%1").arg(replies.localPort())) && tool.trace_->toPlainText().contains("seq=1"), "trace includes alternate reply port and sequence");
        {
            CameraProfileTestDialog second(variant);
            second.host_->setText(QStringLiteral("127.0.0.1"));
            second.testConnection();
            check(until([&]{return !second.ioBusy_;}) && second.status_->text().contains("reserved"), "second tool cannot borrow owned camera");
            check(runtime.profileTestActive(), "failed second lease does not release first owner");
        }
        bool pausedDone=false, pausedOk=true;
        runtime.testConnection([&](bool ok,const QString &) {pausedDone=true;pausedOk=ok;});
        check(until([&] { return pausedDone; }) && !pausedOk, "dock commands rejected while testing");
        bool done=false, ok=false; QVector<QByteArray> frames;
        tool.runPacketSequence({QByteArray::fromHex("81090447ff")},QStringLiteral("zoomInquiry"),[&](bool good,const QVector<QByteArray>&r,const QString &){done=true;ok=good;frames=r;});
        check(until([&]{return done;}) && ok && frames.size()==1 && frames[0]==QByteArray::fromHex("905000010203ff"), "zoom inquiry framing and decoded reply");
        check(times[2]-times[1]>=55 && sent[2].left(8)==QByteArray::fromHex("0110000500000002"), "request gap and sequential envelope");
        check(sent[2].mid(8)==QByteArray::fromHex("81090447ff"), "stale datagram did not corrupt request");
        tool.requestGap_->setValue(90); tool.resetDelay_->setValue(125); tool.replyTimeout_->setValue(175);
        check(tool.applyEditsToProfile(), "UI settings apply to draft");
        check(tool.profile_->saveAs(dir.filePath("edited.camera.cfg"),&error), "edited draft saves");
        CameraProfileDocument saved;
        check(saved.load(dir.filePath("edited.camera.cfg"),&error) && saved.sonyUdpConnection().value("udp").toObject().value("resetSetupMs").toInt()==125, "UI edits persisted to new profile");
        check(runtime.requestGapMs()==65 && runtime.replyTimeoutMs()==200, "draft edits leave slot profile unchanged");
        // Existing Test All confirmation remains; execute the real UI path.
        QTimer::singleShot(0,&tool,[]() { for (QWidget *widget : QApplication::topLevelWidgets()) if (auto *box=qobject_cast<QMessageBox *>(widget)) box->button(QMessageBox::Yes)->click(); });
        tool.startTestAll();
        check(until([&]{return !tool.ioBusy_;},12000), "Test All finishes over UDP");
        int failed=0,passed=0;
        for(int row=0;row<tool.commands_->rowCount();++row) { auto *result=tool.commands_->item(row,4); if(result && result->text()=="FAIL") ++failed; if(result && result->text()=="PASS") ++passed; }
        if (failed || passed != tool.commands_->rowCount()) std::fprintf(stderr,"%s\n", qPrintable(tool.trace_->toPlainText()));
        check(failed==0 && passed==tool.commands_->rowCount(), "all Sony profile rows pass simulated replies");
        // Invalid command drafts must not partially commit timing changes.
        auto *editor = qobject_cast<QLineEdit *>(tool.commands_->cellWidget(0,1));
        const QString oldText = editor->text(); const auto beforeInvalid = tool.profile_->root();
        editor->setText(QStringLiteral("this is not VISCA")); tool.requestGap_->setValue(190);
        check(!tool.applyEditsToProfile() && tool.profile_->root()==beforeInvalid, "invalid command edit blocks atomic draft update");
        editor->setText(oldText); tool.requestGap_->setValue(90);
        // Explicit cancellation during a missing reply must immediately release the camera.
        drop=true; tool.testConnection();
        check(tool.ioBusy_ && tool.releaseButton_->isEnabled(), "Stop & Release remains usable while waiting");
        tool.releaseButton_->click();
        check(!tool.ioBusy_ && !runtime.profileTestActive() && !tool.udp_, "cancel returns camera without altering enabled state");
        drop=false; standby=false; dockDone=false;
        runtime.testConnection([&](bool good,const QString &){dockDone=true;dockOk=good;});
        check(until([&]{return dockDone;}) && dockOk && runtime.isEnabled(), "runtime resets and resumes after release");
        // Failure must report failure, then a later explicit test can reset and recover.
        drop=true; tool.testConnection();
        check(until([&]{return !tool.ioBusy_;}) && tool.status_->text().contains("timed out"), "profile timeout reports failure");
        drop=false; reject=true; tool.testConnection();
        check(until([&]{return !tool.ioBusy_;}) && tool.status_->text().contains("error"), "VISCA rejection reports failure");
        reject=false; standby=true; tool.testConnection();
        check(until([&]{return !tool.ioBusy_;}) && tool.status_->text().contains("STANDBY"), "standby detected without waking camera");
        int zoomRow = -1, stopRow = -1;
        for(int row=0;row<tool.commands_->rowCount();++row) {
            const auto id=tool.commands_->item(row,0)->data(Qt::UserRole).toString();
            if(id==QStringLiteral("zoomInquiry")) zoomRow=row;
            if(id==QStringLiteral("zoomStop")) stopRow=row;
        }
        const int beforeStandby = sent.size();
        tool.commands_->setCurrentCell(zoomRow,0); tool.sendCurrentCommand();
        check(until([&]{return !tool.ioBusy_;}) && tool.status_->text().contains("STANDBY"), "individual Sony test refuses auto-wake");
        check(sent.size()==beforeStandby+1 && sent.last().mid(8)==QByteArray::fromHex("81090400ff"), "standby test sends only power inquiry");
        const int beforeStop=sent.size();
        tool.commands_->setCurrentCell(stopRow,0); tool.sendCurrentCommand();
        check(until([&]{return !tool.ioBusy_;}) && sent.size()==beforeStop+1 && sent.last().mid(8)==QByteArray::fromHex("8101040700ff"), "explicit STOP bypasses power preflight");
        standby=false; done=false;
        tool.runPacketSequence({QByteArray::fromHex("8101040720ff")},QStringLiteral("zoomIn"),[&](bool good,const QVector<QByteArray>&,const QString &){done=true;ok=good;});
        check(until([&]{return done;}) && ok, "continuous zoom can start in test session");
        const int stopStart = sent.size();
        tool.done(QDialog::Rejected);
        check(until([&]{ for(int i=stopStart;i<sent.size();++i) if(sent[i].mid(8)==QByteArray::fromHex("8101040700ff")) return true; return false; }), "closing test sends STOP for active zoom");
        check(!runtime.profileTestActive() && !tool.udp_, "Escape/reject releases lease");
        // TCP path still uses a raw socket, without UDP controls or envelope.
        QTcpServer tcp;
        check(tcp.listen(QHostAddress::LocalHost,0), "TCP simulator listening");
        QByteArray raw; QVector<QByteArray> tcpPackets; QVector<qint64> tcpTimes; bool tcpDrop=false;
        QObject::connect(&tcp,&QTcpServer::newConnection,&tcp,[&]() {
            while(tcp.hasPendingConnections()) {
                auto *socket=tcp.nextPendingConnection();
                QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket]() { const auto bytes=socket->readAll(); raw+=bytes; tcpPackets.append(bytes); tcpTimes.append(clock.elapsed()); if(!tcpDrop) socket->write(QByteArray::fromHex("905002ff")); });
                QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            }
        });
        tool.loadProfile(tcpPath); tool.host_->setText(QStringLiteral("127.0.0.1")); tool.port_->setValue(tcp.serverPort());
        check(!tool.sonyUdp() && tool.supportedTransport() && !tool.udpControls_->isHidden(), "TCP selectable with separate shared tuning fields");
        check(tool.zoomSpeed_->maximum()==15 && tool.zoomPosition_->maximum()==65535, "TCP token range restored");
        tool.testConnection();
        check(until([&]{return !tool.ioBusy_;}) && tool.status_->text().contains("TCP connected"), "TCP connection check retained");
        done=false;
        tool.runPacketSequence({QByteArray::fromHex("81090400ff")},QStringLiteral("powerInquiry"),[&](bool good,const QVector<QByteArray>&,const QString &){done=true;ok=good;});
        check(until([&]{return done;}) && ok && raw==QByteArray::fromHex("81090400ff"), "TCP command uses raw VISCA bytes");
        // New independent Profiler workflow: raw UDP, persisted name and normal runtime parity.
        QUdpSocket rawCamera;
        check(rawCamera.bind(QHostAddress(QHostAddress::LocalHost),quint16(0)), "raw UDP camera bound");
        QVector<QByteArray> rawPackets; QVector<qint64> rawTimes;
        bool rawDrop=false;
        QObject::connect(&rawCamera,&QUdpSocket::readyRead,&rawCamera,[&]() {
            while(rawCamera.hasPendingDatagrams()) {
                const auto datagram=rawCamera.receiveDatagram(); const auto bytes=datagram.data();
                rawPackets.append(bytes); rawTimes.append(clock.elapsed());
                if(rawDrop) continue;
                QByteArray value;
                if(bytes.mid(1)==QByteArray::fromHex("090400ff")) value=QByteArray::fromHex("a05002ff");
                else if(bytes.mid(1)==QByteArray::fromHex("090447ff")) value=QByteArray::fromHex("a05000000102ff");
                else value=QByteArray::fromHex("a051ff");
                replies.writeDatagram(value,datagram.senderAddress(),datagram.senderPort());
            }
        });
        check(tool.connectionVariant_->count()==3 && tool.windowTitle().contains("Camera Profiler"), "three independent methods and renamed title");
        tool.connectionVariant_->setCurrentIndex(tool.connectionVariant_->findData(QStringLiteral("raw-visca-udp")));
        check(!tool.sonyUdp() && tool.udpMethod() && tool.resetDelay_->isEnabled()==false, "raw UDP has no reset setup operation");
        tool.profileName_->setText(QStringLiteral("Community Test Camera"));
        tool.manufacturer_->setText(QStringLiteral("Example Maker")); tool.model_->setText(QStringLiteral("Model 123"));
        tool.port_->setValue(rawCamera.localPort()); tool.localPort_->setValue(local);
        tool.requestGap_->setValue(80); tool.resetDelay_->setValue(900); tool.replyTimeout_->setValue(180); tool.address_->setValue(2);
        check(tool.applyEditsToProfile(), "raw UDP draft settings accepted");
        const QString rawPath=dir.filePath("different-filename.camera.cfg");
        check(tool.profile_->saveAs(rawPath,&error), "raw UDP profile saves");
        CameraProfileDocument rawDoc;
        check(rawDoc.load(rawPath,&error) && rawDoc.displayName()==QStringLiteral("Community Test Camera"), "custom profile name survives a different filename");
        check(rawDoc.supportsRawViscaUdp() && rawDoc.supportsRuntimeConnection() && rawDoc.profileOwnsConnection(), "saved raw UDP eligible for Cameras Tool");
        check(rawDoc.root().value("identity").toObject().value("manufacturer").toString()==QStringLiteral("Example Maker"), "manufacturer preserved");
        check(!QJsonDocument(rawDoc.root()).toJson().contains("127.0.0.1"), "test IP excluded from saved profile");
        tool.testConnection();
        check(until([&]{return !tool.ioBusy_;}) && tool.status_->text().contains("ON"), "Test UDP accepts raw response from alternate port");
        check(rawPackets.size()==1 && rawPackets[0]==QByteArray::fromHex("82090400ff"), "raw UDP sends only VISCA bytes with profile address, no reset/header");
        done=false;
        tool.runPacketSequence({QByteArray::fromHex("82090447ff")},QStringLiteral("zoomInquiry"),[&](bool good,const QVector<QByteArray>&r,const QString &){done=true;ok=good;frames=r;});
        check(until([&]{return done;}) && ok && frames.last()==QByteArray::fromHex("905000000102ff"), "raw UDP zoom reply decoded");
        check(rawTimes[1]-rawTimes[0]>=75 && rawTimes[0]-clock.elapsed()<900, "raw UDP gap honored without Sony reset delay");
        rawDrop=true; tool.testConnection();
        check(until([&]{return !tool.ioBusy_;}) && tool.status_->text().contains("timed out"), "raw UDP profile timeout");
        rawDrop=false;
        tool.releaseButton_->click();
        CameraRuntime rawRuntime(dir.filePath("raw-slot.ini"),QStringLiteral("Raw"),rawPath);
        rawRuntime.setHost(QStringLiteral("127.0.0.1"));
        const auto selectedPort=rawRuntime.port(); rawRuntime.setPort(1);
        check(rawRuntime.usesUdp() && !rawRuntime.usesSonyUdp() && rawRuntime.port()==selectedPort, "runtime selects raw UDP and profile owns port");
        dockDone=false; rawRuntime.testConnection([&](bool good,const QString&){dockDone=true;dockOk=good;});
        check(until([&]{return dockDone;}) && dockOk, "saved raw UDP works in normal runtime");
        dockDone=false; const int runtimeStart=rawTimes.size();
        rawRuntime.testConnection([&](bool good,const QString&){dockDone=true;dockOk=good;});
        check(until([&]{return dockDone;}) && dockOk && rawTimes[runtimeStart]-rawTimes[runtimeStart-1]>=75, "runtime preserves raw UDP profile spacing");
        rawRuntime.setEnabled(false);
        done=false;
        tool.runPacketSequence({QByteArray::fromHex("8201040720ff")},QStringLiteral("zoomIn"),[&](bool good,const QVector<QByteArray>&,const QString&){done=true;ok=good;});
        check(until([&]{return done;}) && ok, "raw UDP continuous drive accepted");
        const int rawBeforeRelease=rawPackets.size(); tool.releaseButton_->click();
        check(until([&]{return rawPackets.size()>rawBeforeRelease;}) && rawPackets.last()==QByteArray::fromHex("8201040700ff"), "raw UDP release stops the configured address");
        tool.newProfile();
        check(tool.profile_->path().isEmpty() && tool.profileName_->text()==QStringLiteral("New camera profile"), "New Profile creates unsaved draft");
        check(tool.manufacturer_->text()==QStringLiteral("Custom") && tool.model_->text()==QStringLiteral("Unspecified"), "new model does not inherit manufacturer identity");
        check(tool.profile_->root().value("support").toObject().value("status").toString()==QStringLiteral("draft"), "new profile is not certified");
        check(!tool.profile_->commandIds().isEmpty(), "new draft contains editable starter command rows");
        tool.profileName_->clear(); const auto invalidRoot=tool.profile_->root();
        check(!tool.applyEditsToProfile() && tool.profile_->root()==invalidRoot, "blank profile name rejected atomically");
        tool.profileName_->setText(QStringLiteral("My TCP camera"));
        tool.connectionVariant_->setCurrentIndex(tool.connectionVariant_->findData(QStringLiteral("raw-visca-tcp")));
        tool.port_->setValue(tcp.serverPort()); tool.requestGap_->setValue(85); tool.address_->setValue(1);
        check(tool.applyEditsToProfile(), "TCP tuning accepted independently of method");
        const QString tunedTcp=dir.filePath("tcp-profile.camera.cfg");
        check(tool.profile_->saveAs(tunedTcp,&error), "TCP profile with tuning saved");
        CameraRuntime tcpRuntime(dir.filePath("tcp-slot.ini"),QStringLiteral("TCP"),tunedTcp);
        tcpRuntime.setHost(QStringLiteral("127.0.0.1"));
        check(tcpRuntime.profileOwnsConnection() && tcpRuntime.requestGapMs()==85 && tcpRuntime.replyTimeoutMs()==180, "normal TCP runtime loads profile tuning");
        check(!tool.localPort_->isEnabled() && !tool.resetDelay_->isEnabled() && tool.requestGap_->isEnabled(), "irrelevant tuning disabled without changing active method");
        int tcpCompleted=0;
        const int tcpStart=tcpTimes.size();
        tcpRuntime.setCameraPower(true,[&](bool good,const QString&){check(good,"runtime TCP first command completed");++tcpCompleted;});
        tcpRuntime.setCameraPower(false,[&](bool good,const QString&){check(good,"runtime TCP second command completed");++tcpCompleted;});
        check(until([&]{return tcpCompleted==2;}) && tcpTimes.size()==tcpStart+2 && tcpTimes[tcpStart+1]-tcpTimes[tcpStart]>=80, "normal TCP runtime enforces profile spacing across queued requests");
        tool.host_->setText(QStringLiteral("127.0.0.1")); done=false;
        tool.runPacketSequence({QByteArray::fromHex("81090400ff"),QByteArray::fromHex("81090400ff")},QStringLiteral("powerInquiry"),[&](bool good,const QVector<QByteArray>&,const QString&){done=true;ok=good;});
        const int toolTcpStart=tcpTimes.size();
        check(until([&]{return done;}) && ok && tcpTimes.size()==toolTcpStart+2 && tcpTimes.last()-tcpTimes[tcpTimes.size()-2]>=80, "Profiler TCP enforces profile spacing between steps");
        done=false;
        tool.runPacketSequence({QByteArray::fromHex("81090400ff")},QStringLiteral("powerInquiry"),[&](bool good,const QVector<QByteArray>&,const QString&){done=true;ok=good;});
        const int beforeCancel=tcpPackets.size(); tool.releaseButton_->click();
        QElapsedTimer cancelTime; cancelTime.start(); until([&]{return cancelTime.elapsed()>=150;},300);
        check(tcpPackets.size()==beforeCancel && !done, "release cancels a paced TCP send without a late command");
        done=false;
        tool.runPacketSequence({QByteArray::fromHex("8101040720ff")},QStringLiteral("zoomIn"),[&](bool good,const QVector<QByteArray>&,const QString&){done=true;ok=good;});
        check(until([&]{return done;}) && ok, "Profiler TCP continuous drive accepted");
        const int tcpBeforeRelease=tcpPackets.size(); tool.releaseButton_->click();
        check(until([&]{return tcpPackets.size()>tcpBeforeRelease;}) && tcpPackets.last()==QByteArray::fromHex("8101040700ff"), "TCP release attempts active zoom STOP");
        tcpDrop=true; tool.testConnection();
        check(until([&]{return !tool.ioBusy_;}) && tool.status_->text().contains("TCP connected"), "TCP connection test does not require a camera reply");
        done=false;
        tool.runPacketSequence({QByteArray::fromHex("81090400ff")},QStringLiteral("powerInquiry"),[&](bool good,const QVector<QByteArray>&,const QString&){done=true;ok=good;});
        check(until([&]{return done;}) && !ok, "Profiler TCP uses profile reply timeout");
        tool.releaseButton_->click(); tcpDrop=false;
        const int tuningPort=tool.port_->value(), tuningGap=tool.requestGap_->value();
        tool.connectionVariant_->setCurrentIndex(tool.connectionVariant_->findData(QStringLiteral("sony-visca-udp")));
        check(tool.port_->value()==tuningPort && tool.requestGap_->value()==tuningGap, "choosing a method does not overwrite profile tuning");
        check(tool.applyEditsToProfile() && tool.profile_->supportsSonyViscaUdp(), "Sony framing can be selected for a non-Sony model profile");
        check(tool.profile_->root().value("capabilities").toObject().value("limits").toObject().value("zoomSpeed").toObject().value("maximum").toInt()==15, "framing method does not impose SRG300H model speed range");
        const QString framedPath=dir.filePath("framed-profile.camera.cfg");
        check(tool.profile_->saveAs(framedPath,&error), "third method profile saves independently");
        check(!tool.profile_->root().value("support").toObject().contains("hardwareTest"), "custom variants do not inherit hardware certification");
        tool.connectionVariant_->setCurrentIndex(tool.connectionVariant_->findData(QStringLiteral("raw-visca-tcp")));
        tool.port_->setValue(52381);
        check(tool.connectionVariant_->currentData().toString()==QStringLiteral("raw-visca-tcp"), "port 52381 does not infer Sony or UDP method");
        QTemporaryDir uiSaveDir;
        tool.newProfile(); tool.profileName_->setText(QStringLiteral("UI Name Independent Of File"));
        const QString uiSaved=uiSaveDir.filePath("ui-save.camera.cfg");
        QTimer::singleShot(0,&tool,[uiSaved]() {
            for(QWidget *widget : QApplication::topLevelWidgets()) if(auto *dialog=qobject_cast<QFileDialog *>(widget)) {
                dialog->setDirectory(QFileInfo(uiSaved).absolutePath());
                dialog->selectFile(QFileInfo(uiSaved).fileName());
                static_cast<QDialog *>(dialog)->accept();
            }
        });
        tool.saveAs();
        CameraProfileDocument uiCopy;
        check(uiCopy.load(uiSaved,&error) && uiCopy.displayName()==QStringLiteral("UI Name Independent Of File"), "real New Profile / Save As UI saves a new nonexistent file with its custom name");
        check(tool.profile_->path()==uiSaved, "Save As UI selects the saved custom profile document");
        // Test profile persistence is complete and schema-checkable outside this process.
        if(!qEnvironmentVariable("PROFILER_TEST_EXPORT").isEmpty()) {
            const QDir out(qEnvironmentVariable("PROFILER_TEST_EXPORT")); QDir().mkpath(out.path());
            QFile::remove(out.filePath("raw-udp.camera.cfg")); QFile::remove(out.filePath("raw-tcp.camera.cfg")); QFile::remove(out.filePath("framed-udp.camera.cfg"));
            QFile::copy(rawPath,out.filePath("raw-udp.camera.cfg")); QFile::copy(tunedTcp,out.filePath("raw-tcp.camera.cfg")); QFile::copy(framedPath,out.filePath("framed-udp.camera.cfg"));
        }

        // v0.1.13: Profile/test ownership, versioning, local receive-port policy and legacy migration.
        check(tool.profileControls_->isAncestorOf(tool.profileName_) && tool.profileControls_->isAncestorOf(tool.port_) &&
            tool.profileControls_->isAncestorOf(tool.address_) && tool.profileControls_->isAncestorOf(tool.requestGap_), "all reusable settings live in upper Profile area");
        check(tool.connectionControls_->findChildren<QLineEdit *>().size()==1 &&
            tool.connectionControls_->findChildren<QSpinBox *>().isEmpty(), "Test Connection exposes only IP as editable connection value");
        tool.connectionVariant_->setCurrentIndex(tool.connectionVariant_->findData(QStringLiteral("raw-visca-udp")));
        tool.sameLocalPort_->setChecked(true); tool.port_->setValue(52381);
        check(tool.localPort_->value()==52381 && !tool.localPort_->isEnabled(), "same-port binding follows camera port");
        tool.port_->setValue(52382);
        check(tool.localPort_->value()==52382 && tool.connectionVariant_->currentData().toString()==QStringLiteral("raw-visca-udp"), "same-port synchronization does not choose method");
        tool.profileVersion_->setText(QStringLiteral("2.1.3-draft"));
        check(tool.applyEditsToProfile() && tool.profile_->profileVersion()==QStringLiteral("2.1.3-draft"), "explicit profile revision accepted separately from plugin/schema");
        const auto rootWithPolicy=tool.profile_->root();
        check(rootWithPolicy.value("connections").toArray()[0].toObject().value("udp").toObject().value("localPortMode").toString()==QStringLiteral("same-as-camera"), "same-port policy stored in profile");
        const QString policyPath=uiSaveDir.filePath("receive-policy.camera.cfg");
        check(tool.profile_->saveAs(policyPath,&error), "receive policy profile saved");
        CameraProfileDocument policyDoc;
        check(policyDoc.load(policyPath,&error) && policyDoc.profileVersion()==QStringLiteral("2.1.3-draft"), "profile revision round-trips");
        tool.profileVersion_->setText(QStringLiteral("invalid revision"));
        check(!tool.applyEditsToProfile() && tool.profile_->root()==policyDoc.root(), "invalid revision rejected atomically");
        tool.profileVersion_->setText(QStringLiteral("2.1.4"));
        tool.sameLocalPort_->setChecked(false); tool.localPort_->setValue(52383);
        check(tool.localPort_->isEnabled() && tool.applyEditsToProfile() &&
            tool.profile_->networkConnection().value("udp").toObject().value("localPortMode").toString()==QStringLiteral("fixed"), "distinct receive port remains editable profile tuning");
        tool.connectionVariant_->setCurrentIndex(tool.connectionVariant_->findData(QStringLiteral("raw-visca-tcp")));
        check(tool.localPortRow_->isHidden() && tool.resetDelayRow_->isHidden(), "TCP hides irrelevant receive/reset fields");
        tool.connectionVariant_->setCurrentIndex(tool.connectionVariant_->findData(QStringLiteral("raw-visca-udp")));
        check(!tool.localPortRow_->isHidden() && tool.resetDelayRow_->isHidden(), "raw UDP shows receive setting but no Sony reset delay");
        tool.connectionVariant_->setCurrentIndex(tool.connectionVariant_->findData(QStringLiteral("sony-visca-udp")));
        check(!tool.localPortRow_->isHidden() && !tool.resetDelayRow_->isHidden(), "Sony method shows reset and receive tuning");
        CameraRuntime policyRuntime(uiSaveDir.filePath("policy-slot.ini"),QStringLiteral("Policy"),policyPath);
        check(policyRuntime.udpLocalPort()==policyRuntime.port() && policyRuntime.port()==52382, "runtime resolves receive port from same-as-camera policy");
        auto invalidPolicy=policyDoc.root(); auto invalidConnections=invalidPolicy.value("connections").toArray();
        auto invalidConnection=invalidConnections[0].toObject(); auto invalidUdp=invalidConnection.value("udp").toObject();
        invalidUdp.insert(QStringLiteral("localPortMode"),QStringLiteral("guess")); invalidConnection.insert(QStringLiteral("udp"),invalidUdp);
        invalidConnections[0]=invalidConnection; invalidPolicy.insert(QStringLiteral("connections"),invalidConnections);
        CameraProfileDocument invalidDocument;
        check(!invalidDocument.loadBytes(QJsonDocument(invalidPolicy).toJson(),&error), "unknown local-port policy rejected");
        QTemporaryDir migrationDir;
        const QString legacySettings=migrationDir.filePath("production.ini");
        { QSettings legacy(legacySettings,QSettings::IniFormat); legacy.setValue("camera/port",1259); legacy.setValue("camera/address",3);
          legacy.setValue("camera/host","192.168.50.4"); legacy.setValue("camera/name","Production Left"); legacy.setValue("camera/feedSourceUuid","video-uuid"); }
        QString migratedPath;
        const QString migrationProfiles=migrationDir.filePath("profiles");
        {
            CameraRuntime legacy(legacySettings,QStringLiteral("Legacy"),tcpPath);
            legacy.presets().setLabel(0,0,QStringLiteral("Keep this preset"));
            check(legacy.migrateLegacyConnection(migrationProfiles,&error), "legacy slot migrates to reusable profile");
            migratedPath=legacy.profilePath();
            check(legacy.profileOwnsConnection() && legacy.port()==1259 && legacy.connectionSummary().contains("VISCA 3"), "migration preserves working port/address in profile");
            check(legacy.host()==QStringLiteral("192.168.50.4") && legacy.cameraName()==QStringLiteral("Production Left") &&
                legacy.cameraFeedSourceUuid()==QStringLiteral("video-uuid") && legacy.presets().presetLabel(0,0)==QStringLiteral("Keep this preset"), "migration preserves installation and preset data");
            CameraProfileDocument migrated;
            check(migrated.load(migratedPath,&error) && migrated.networkConnection().value("defaultPort").toInt()==1259 &&
                migrated.networkConnection().value("addressing").toObject().value("default").toInt()==3, "migrated endpoint stored in profile, not just cached slot fields");
            check(!QJsonDocument(migrated.root()).toJson().contains("192.168.50.4"), "migration does not export installation IP");
            const int profileCount=QDir(migrationProfiles).entryList({"*.camera.cfg"},QDir::Files).size();
            check(legacy.migrateLegacyConnection(migrationProfiles,&error) && QDir(migrationProfiles).entryList({"*.camera.cfg"},QDir::Files).size()==profileCount, "migration is idempotent");
        }
        {
            CameraRuntime restarted(legacySettings,QStringLiteral("Legacy"),tcpPath);
            check(restarted.profilePath()==migratedPath && restarted.port()==1259 && restarted.profileOwnsConnection(), "restart retains selected migration profile");
            check(restarted.migrateLegacyConnection(migrationProfiles,&error) && restarted.presets().presetLabel(0,0)==QStringLiteral("Keep this preset"), "restart migration preserves preset catalog");
        }
        {
            const QString another=migrationDir.filePath("another.ini"); QSettings settings(another,QSettings::IniFormat);
            settings.setValue("camera/port",1259); settings.setValue("camera/address",3); settings.sync();
            CameraRuntime shared(another,QStringLiteral("Other"),tcpPath);
            check(shared.migrateLegacyConnection(migrationProfiles,&error) && shared.profilePath()==migratedPath, "same legacy endpoint reuses existing profile across slots");
            const QString unusual=migrationDir.filePath("custom-port.ini"); QSettings custom(unusual,QSettings::IniFormat);
            custom.setValue("camera/port",4321);custom.setValue("camera/address",2);custom.sync();
            CameraRuntime customRuntime(unusual,QStringLiteral("Custom"),tcpPath);
            check(customRuntime.migrateLegacyConnection(migrationProfiles,&error) && customRuntime.port()==4321 &&
                customRuntime.profilePath()!=migratedPath && customRuntime.connectionSummary().contains("VISCA 2"), "migration preserves nonstandard endpoints in separate profile");
        }
        {
            QFile blocked(migrationDir.filePath("not-a-directory")); check(blocked.open(QIODevice::WriteOnly), "migration failure fixture"); blocked.close();
            CameraRuntime legacy(migrationDir.filePath("failure.ini"),QStringLiteral("Legacy"),tcpPath); legacy.setPort(4455);
            const QString originalPath=legacy.profilePath();
            check(!legacy.migrateLegacyConnection(blocked.fileName(),&error) && legacy.port()==4455 && legacy.profilePath()==originalPath,
                "failed profile write leaves legacy working endpoint untouched");
        }
        if(!qEnvironmentVariable("PROFILER_TEST_EXPORT").isEmpty()) {
            const QDir out(qEnvironmentVariable("PROFILER_TEST_EXPORT"));
            QFile::remove(out.filePath("receive-policy.camera.cfg")); QFile::copy(policyPath,out.filePath("receive-policy.camera.cfg"));
            QFile::remove(out.filePath("migrated.camera.cfg")); QFile::copy(migratedPath,out.filePath("migrated.camera.cfg"));
        }

        // Blank labels block dock and OBS-source recalls without disabling Store.
        runtime.setEnabled(false); // Release the initial Sony listener for isolated guard tests.
        CameraRuntime recallGuard(dir.filePath("blank-recall.ini"),QStringLiteral("Guard"),tcpPath);
        recallGuard.setHost(QStringLiteral("127.0.0.1")); recallGuard.setPort(tcp.serverPort());
        recallGuard.presets().clearLabel(0,0);
        check(recallGuard.hasUsablePreset(0,0) && !recallGuard.canRecallPreset(0,0), "mapping exists but unnamed preset is not recallable");
        int prevented=0; const int blankStart=tcpPackets.size();
        recallGuard.recallLogicalPreset(1,0,[&](bool good,const QString &){if(!good) ++prevented;});
        recallGuard.recallLogicalPreset(1,0,[&](bool good,const QString &){if(!good) ++prevented;},CameraRuntime::PresetRecallOrigin::ObsSource);
        check(prevented==2 && tcpPackets.size()==blankStart, "blank dock and source recalls rejected before any packet");
        recallGuard.presets().setLabel(0,0,QStringLiteral("  \t  "));
        check(!recallGuard.canRecallPreset(0,0), "whitespace labels remain inactive");
        done=false;
        recallGuard.storeLogicalPreset(0,0,[&](bool good,const QString&){done=true;ok=good;});
        check(until([&]{return done;}) && ok && tcpPackets.last().startsWith(QByteArray::fromHex("8101043f01")), "Store still sends preset-set command for blank label");
        check(!recallGuard.canRecallPreset(0,0) && recallGuard.presets().presetLabel(0,0).isEmpty(), "Store does not enable recall until named");
        check(recallGuard.presets().addPage(), "new empty page for unassigned Store test"); done=false;
        recallGuard.storeLogicalPreset(5,0,[&](bool good,const QString&){done=true;ok=good;});
        check(until([&]{return done;}) && ok && recallGuard.presets().slot(5,0).physicalPreset>0 &&
            recallGuard.presets().presetLabel(5,0).isEmpty() && !recallGuard.canRecallPreset(5,0), "newly allocated Store also stays blank and inactive until renamed");
        recallGuard.presets().setLabel(0,0,QStringLiteral("Named preset"));
        check(recallGuard.canRecallPreset(0,0), "nonblank name enables mapped recall");
        done=false;
        recallGuard.recallLogicalPreset(1,0,[&](bool good,const QString&){done=true;ok=good;},CameraRuntime::PresetRecallOrigin::ObsSource);
        check(until([&]{return done;}) && ok, "named OBS-source recall still executes");
        const int oldMapping=recallGuard.presets().slot(0,0).physicalPreset;
        const int delayedStart=tcpPackets.size(); done=false;
        recallGuard.recallLogicalPreset(1,100,[&](bool good,const QString&){done=true;ok=good;},CameraRuntime::PresetRecallOrigin::ObsSource);
        recallGuard.presets().clearLabel(0,0);
        check(until([&]{return done;}) && !ok && tcpPackets.size()==delayedStart, "clearing label cancels delayed TCP recall before wire send");
        check(recallGuard.presets().slot(0,0).physicalPreset==oldMapping, "recall protection does not erase physical mapping");
        {
            CameraRuntime restartedBlank(dir.filePath("blank-recall.ini"),QStringLiteral("Guard"),tcpPath);
            check(!restartedBlank.canRecallPreset(0,0) && restartedBlank.presets().presetLabel(5,0).isEmpty() &&
                restartedBlank.presets().slot(5,0).physicalPreset>0 && !restartedBlank.canRecallPreset(5,0), "restart preserves cleared and newly stored unnamed presets as inactive");
        }
        recallGuard.presets().setLabel(0,0,QStringLiteral("Before swap")); recallGuard.presets().setLabel(0,1,QStringLiteral("After swap"));
        const int beforeSwap=tcpPackets.size(); done=false;
        recallGuard.recallLogicalPreset(1,100,[&](bool good,const QString&){done=true;ok=good;});
        recallGuard.presets().swapSlots(0,0,0,1);
        check(until([&]{return done;}) && !ok && tcpPackets.size()==beforeSwap, "queued recall canceled if its logical button is remapped");
        rawRuntime.setEnabled(true); rawRuntime.presets().setLabel(0,0,QStringLiteral("Raw queued preset"));
        const int rawGuardStart=rawPackets.size(); done=false;
        rawRuntime.recallLogicalPreset(1,100,[&](bool good,const QString&){done=true;ok=good;});
        rawRuntime.presets().clearLabel(0,0);
        check(until([&]{return done;}) && !ok && rawPackets.size()==rawGuardStart, "clearing label cancels delayed raw UDP recall");
        rawRuntime.setEnabled(false);
        CameraRuntime sonyGuard(dir.filePath("sony-guard.ini"),QStringLiteral("Sony guard"),variant);
        sonyGuard.setHost(QStringLiteral("127.0.0.1")); sonyGuard.presets().setLabel(0,0,QStringLiteral("Sony queued preset"));
        const int sonyGuardStart=sent.size(); done=false;
        sonyGuard.recallLogicalPreset(1,100,[&](bool good,const QString&){done=true;ok=good;},CameraRuntime::PresetRecallOrigin::ObsSource);
        sonyGuard.presets().clearLabel(0,0);
        check(until([&]{return done;}) && !ok, "clearing label cancels delayed Sony-framed recall");
        bool sonyRecall=false;
        for(int i=sonyGuardStart;i<sent.size();++i) if(sent[i].mid(8).startsWith(QByteArray::fromHex("8101043f02"))) sonyRecall=true;
        check(!sonyRecall, "Sony session may reset but sends no cleared preset recall");
        sonyGuard.setEnabled(false);
        check(tool.sameLocalPort_->text()==QStringLiteral("Local receive port same as camera port"), "receive-port option is one standard checkbox caption");

        setCameraRuntime(0,nullptr);
        std::printf("Profile tool: %d checks passed\n",checks);
        return 0;
    }
};
}
int main(int argc,char **argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);
    if(argc!=3) return 2;
    return simplecam::ProfileToolTest::run(QString::fromLocal8Bit(argv[1]),QString::fromLocal8Bit(argv[2]));
}
