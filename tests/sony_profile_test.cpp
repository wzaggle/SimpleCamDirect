#include "camera_runtime.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <iostream>
using namespace simplecam;
int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    if (argc != 3) return 2;
    int checks=0, failures=0;
    auto check=[&](bool ok,const char *why) { ++checks; if (!ok) { ++failures; std::cerr<<why<<'\n'; } };
    QString error; CameraProfileDocument profile;
    check(profile.load(QString::fromLocal8Bit(argv[1]),&error),"Sony profile parses");
    check(profile.supportsSonyViscaUdp() && !profile.supportsRawViscaTcp(),"explicit UDP selection");
    check(profile.presetCapacity()==255,"maintainer-reported IP memory capacity");
    check(!profile.command("presetRecall").templates.isEmpty(),"recall template available for validation");
    check(!profile.command("movePanTilt").templates.isEmpty(),"pan tilt template available for validation");
    QByteArray packet; ViscaTemplateValues values;
    check(renderViscaTemplate(profile.command("zoomIn").templates.first(),values,&packet,&error) && packet==QByteArray::fromHex("8101040720ff"),"lowest speed TELE fixture");
    const std::pair<const char *,const char *> fixtures[] = {
        {"movePanTilt","8101060108080103ff"},{"stopPanTilt","8101060108080303ff"},{"home","81010604ff"},
        {"zoomOut","8101040730ff"},{"zoomStop","8101040700ff"},{"zoomAbsolute","8101044700000000ff"},
        {"focusNear","8101040803ff"},{"focusFar","8101040802ff"},{"focusStop","8101040800ff"},
        {"focusAuto","8101043802ff"},{"focusManual","8101043803ff"},{"whiteBalanceAuto","8101043500ff"},
        {"whiteBalanceManual","8101043505ff"},{"presetRecall","8101043f0200ff"},{"presetStore","8101043f0100ff"},
        {"powerOn","8101040002ff"},{"powerOff","8101040003ff"},{"backlightOn","8101043302ff"},
        {"backlightOff","8101043303ff"},{"exposureCompOn","8101043e02ff"},{"exposureCompOff","8101043e03ff"},
        {"positionInquiry","81090612ff"},{"zoomInquiry","81090447ff"},{"powerInquiry","81090400ff"}
    };
    for (const auto &fixture : fixtures) {
        check(renderViscaTemplate(profile.command(QString::fromLatin1(fixture.first)).templates.first(),values,&packet,&error) &&
              packet==QByteArray::fromHex(fixture.second),fixture.first);
    }
    values.physicalPreset=255;
    check(renderViscaTemplate(profile.command("presetRecall").templates.first(),values,&packet,&error) &&
        packet==QByteArray::fromHex("8101043f02feff"),"preset 255 uses native FE");
    values.zoomPosition=0x4000;
    check(renderViscaTemplate(profile.command("zoomAbsolute").templates.first(),values,&packet,&error) &&
        packet==QByteArray::fromHex("8101044704000000ff"),"absolute optical endpoint nibbles");
    const auto original=profile.root();
    for (const auto &field : {QStringLiteral("localPort"),QStringLiteral("minimumRequestGapMs")}) {
        auto root=original; auto connections=root["connections"].toArray(); auto c=connections[0].toObject(); auto udp=c["udp"].toObject();
        udp[field]=-1;c["udp"]=udp;connections[0]=c;root["connections"]=connections;
        CameraProfileDocument bad; check(!bad.loadBytes(QJsonDocument(root).toJson(),&error),"invalid UDP integer rejected");
    }
    {
        auto root=original;auto connections=root["connections"].toArray();connections.append(connections.first());root["connections"]=connections;
        CameraProfileDocument bad;check(!bad.loadBytes(QJsonDocument(root).toJson(),&error),"duplicate connection rejected");
    }
    QTemporaryDir dir; const auto path=dir.filePath("slot.ini");
    {
        CameraRuntime runtime(path,"Sony",QString::fromLocal8Bit(argv[1]));
        check(runtime.usesSonyUdp() && runtime.port()==52381,"fresh Sony slot defaults to UDP 52381");
        check(runtime.requestGapMs()==100 && runtime.replyTimeoutMs()==3000,"separate tested defaults");
        runtime.setPort(1234);runtime.setCameraAddress(7);
        check(runtime.port()==52381,"fixed UDP target port rejects slot edit");
        QSettings stale(path,QSettings::IniFormat);
        stale.setValue("camera/udpLocalPort",52382);stale.setValue("camera/requestGapMs",1);stale.setValue("camera/replyTimeoutMs",50);stale.sync();
    }
    {
        CameraRuntime runtime(path,"Sony",QString::fromLocal8Bit(argv[1]));
        check(runtime.udpLocalPort()==52381 && runtime.requestGapMs()==100 && runtime.replyTimeoutMs()==3000,"profile replaces stale v019 slot overrides");
    }
    {
        CameraRuntime runtime(dir.filePath("tcp.ini"),"AViPAS",QString::fromLocal8Bit(argv[2]));
        runtime.setPort(4321);
        check(runtime.setProfilePath(QString::fromLocal8Bit(argv[1]),&error) && runtime.usesSonyUdp() && runtime.port()==52381,"switch to Sony selects UDP default");
        check(runtime.setProfilePath(QString::fromLocal8Bit(argv[2]),&error) && runtime.port()==4321,"switch back restores TCP port");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures ? 1 : 0;
}
