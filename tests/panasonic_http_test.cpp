#include "panasonic_aw.hpp"
#include "profile_test_tool.hpp"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <cstdio>
#include <cstdlib>

namespace simplecam {
class ProfileToolTest {
    static inline int checks=0;
    static void check(bool ok,const char *why) { ++checks; if(!ok) { std::fprintf(stderr,"FAIL: %s\n",why); std::exit(1); } }
    static bool until(const std::function<bool()> &f,int ms=5000) {
        QElapsedTimer clock; clock.start();
        while(!f() && clock.elapsed()<ms) QCoreApplication::processEvents(QEventLoop::AllEvents,5);
        return f();
    }
public:
    static int run(const QString &path) {
        CameraProfileDocument profile; QString error; check(profile.load(path,&error),qPrintable(error));
        check(profile.supportsPanasonicHttp() && profile.supportsRuntimeConnection(),"AW runtime method");
        check(profile.presetCapacity()==100 && profile.root().value("support").toObject().value("status")=="draft","draft and capacity");
        ViscaTemplateValues v; v.zoomPosition=0x555; QVector<QByteArray> frames;
        check(profile.renderTemplates({"ptz:#M{AW_PI}"},v,&frames,&error) && frames.first()=="ptz:#M00","first preset decimal mapping");
        v.physicalPreset=100;
        check(profile.renderTemplates({"ptz:#R{AW_PI}"},v,&frames,&error) && frames.first()=="ptz:#R99","last preset decimal mapping");
        v.physicalPreset=101; check(!profile.renderTemplates({"ptz:#R{AW_PI}"},v,&frames,&error),"out of range preset rejected"); v.physicalPreset=1;
        v.panSpeed=24; v.tiltSpeed=20; v.panDirection=1; v.tiltDirection=1;
        check(profile.renderTemplates({"ptz:#PTS{AW_P}{AW_T}"},v,&frames,&error) && frames.first()=="ptz:#PTS0199","left up max mapping");
        v.panDirection=2; v.tiltDirection=2;
        check(profile.renderTemplates({"ptz:#PTS{AW_P}{AW_T}"},v,&frames,&error) && frames.first()=="ptz:#PTS9901","right down max mapping");
        check(awRequestUrl("127.0.0.1",80,"ptz:#O").toEncoded().contains("cmd=%23O&res=1"),"hash URL encoded, not fragment");
        for(const auto &bad:{"http://evil/","ptz:#O1&other=1","ptz:#Z00","ptz:#AXZ000","ptz:#R100","ptz:#O\r\nX:y"})
            check(!validateAwCommand(bad),"unsafe or out-of-range command rejected");
        check(awRequestUrl("user:secret@127.0.0.1",80,"ptz:#O").isEmpty(),"userinfo host rejected");
        bool on=false; int z=0;
        check(decodeAwPower("p1",&on) && on && decodeAwPower("p0",&on) && !on,"power decode");
        check(!decodeAwPower("p3",&on),"transition not on or standby");
        check(decodeAwZoom("gz555",&z) && z==0x555 && decodeAwZoom("gzFFF",&z) && z==0xFFF,"zoom native range");
        check(!decodeAwZoom("gz000",&z) && !decodeAwZoom("gzFFFjunk",&z),"invalid zoom rejected");
        check(validateAwReply("ptz:#R11","s11") && !validateAwReply("ptz:#R11","s12"),"preset exact acknowledgement");
        check(!validateAwReply("ptz:#O","<html>login</html>") && !validateAwReply("ptz:#O","ER3"),"HTML and AW error rejected");

        QTemporaryDir dir; QTcpServer server; check(server.listen(QHostAddress::LocalHost),"HTTP simulator listen");
        QVector<QByteArray> requests; QVector<qint64> times; QElapsedTimer clock; clock.start();
        int status=200; bool drop=false, wrong=false; QByteArray zoom="555";
        QObject::connect(&server,&QTcpServer::newConnection,&server,[&]() {
            while(server.hasPendingConnections()) {
                auto *s=server.nextPendingConnection(); auto buffer=std::make_shared<QByteArray>();
                QObject::connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
                QObject::connect(s,&QTcpSocket::readyRead,s,[&,s,buffer]() {
                    buffer->append(s->readAll()); if(!buffer->contains("\r\n\r\n")) return;
                    const auto line=buffer->left(buffer->indexOf("\r\n")); const auto uri=line.split(' ').at(1);
                    requests.append(uri); times.append(clock.elapsed());
                    check(buffer->contains("Connection: close"),"HTTP close per command");
                    if(drop) return;
                    const auto c=QUrlQuery(QUrl::fromEncoded(uri)).queryItemValue("cmd").toLatin1(); QByteArray body;
                    if(c=="#O") body="p1";
                    else if(c=="#GZ") body="gz"+zoom;
                    else if(c.startsWith("#AXZ")) { zoom=c.mid(4); body="axz"+zoom; }
                    else if(c.startsWith("#PTS")) body="pTS"+c.mid(4);
                    else if(c.startsWith("#R") || c.startsWith("#M")) body="s"+c.mid(2);
                    else if(c.startsWith("#Z")) body="zS"+c.mid(2);
                    else if(c.startsWith("#F")) body="fS"+c.mid(2);
                    else if(c.startsWith("#D1")) body="d1"+c.mid(3);
                    else body="ER1";
                    if(wrong) body="<html>login</html>";
                    const QByteArray head="HTTP/1.1 "+QByteArray::number(status)+" Test\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n";
                    s->write(head+body); s->disconnectFromHost();
                });
            }
        });
        PanasonicAwHttpTransport transport; transport.configure("127.0.0.1",server.serverPort(),130,200);
        bool done=false, ok=false; auto finish=[&](bool success,const QString &) { ok=success; done=true; };
        transport.enqueue({{"ptz:#O",true},{"ptz:#GZ",true}},finish);
        check(until([&]{return done;}) && ok,"two HTTP requests complete");
        check(times.size()==2 && times[1]-times[0]>=125,"serialized minimum command gap");
        status=401; done=false; transport.enqueue({{"ptz:#O",true}},finish);
        check(until([&]{return done;}) && !ok,"authentication failure not success");
        status=200; wrong=true; done=false; transport.enqueue({{"ptz:#O",true}},finish);
        check(until([&]{return done;}) && !ok,"HTTP 200 HTML not success"); wrong=false;
        drop=true; done=false; transport.enqueue({{"ptz:#O",true}},finish);
        check(until([&]{return done;}) && !ok,"missing reply timeout"); drop=false;
        done=false; ViscaTcpTransport::FrameCommand guarded{"ptz:#R00",true}; guarded.canSend=[] { return false; };
        const int before=requests.size(); transport.enqueue({guarded},finish);
        check(until([&]{return done;}) && !ok && requests.size()==before,"cancelled recall not transmitted");

        // Release a drive before its delayed start: STOP must not be followed by stale movement.
        bool driveDone=false, stopDone=false; const int stopStart=requests.size();
        transport.enqueue({{"ptz:#PTS6050",true}},[&](bool,const QString &) { driveDone=true; },1000);
        transport.enqueue({{"ptz:#PTS5050",true}},[&](bool success,const QString &) { ok=success; stopDone=true; },0,true);
        check(until([&]{return driveDone && stopDone;}) && ok,"priority STOP interrupts delayed drive");
        check(requests.size()==stopStart+1 && requests.last().contains("PTS5050"),"no stale drive after STOP");
        status=302; done=false; transport.enqueue({{"ptz:#O",true}},finish);
        check(until([&]{return done;}) && !ok,"redirect not followed"); status=200;
        CameraProfileDocument invalid=profile;
        check(!invalid.setCommandTemplates("powerInquiry",{"ptz:#O1"},&error),"read-only inquiry cannot bind power write");
        auto invalidRoot=profile.root(); auto invalidConnections=invalidRoot.value("connections").toArray();
        auto invalidConnection=invalidConnections[0].toObject(); invalidConnection["tuning"]=QJsonObject{{"minimumRequestGapMs",129}};
        invalidConnections[0]=invalidConnection; invalidRoot["connections"]=invalidConnections;
        check(!invalid.loadBytes(QJsonDocument(invalidRoot).toJson(),&error),"unsafe gap rejected on load");

        CameraRuntime runtime(dir.filePath("camera.ini"),"HTTP test",path);
        runtime.setHost("127.0.0.1"); // Set profile port without modifying any distributed profile.
        QFile source(path); check(source.open(QIODevice::ReadOnly),"profile read"); auto document=QJsonDocument::fromJson(source.readAll()).object();
        auto connections=document.value("connections").toArray(); auto connection=connections[0].toObject(); connection["defaultPort"]=server.serverPort(); connections[0]=connection; document["connections"]=connections;
        const auto testPath=dir.filePath("aw.json"); QFile file(testPath); check(file.open(QIODevice::WriteOnly),"temp profile write"); file.write(QJsonDocument(document).toJson()); file.close();
        check(runtime.setProfilePath(testPath,&error),"HTTP profile selectable");
        done=false; runtime.testConnection(finish); check(until([&]{return done;}) && ok,"runtime HTTP inquiry");
        done=false; runtime.setZoomPosition(0,finish); check(until([&]{return done;}) && ok && runtime.lastZoomSliderValue()==0,"runtime wide offset/readback");
        done=false; runtime.setZoomPosition(200,finish); check(until([&]{return done;}) && ok && runtime.lastZoomSliderValue()==200,"runtime tele offset/readback");
        runtime.presets().clearLabel(0,0); done=false; runtime.recallLogicalPreset(1,0,finish); check(done && !ok,"blank name blocks HTTP recall");
        runtime.presets().setLabel(0,0,"Test"); done=false; runtime.recallLogicalPreset(1,200,finish); runtime.presets().clearLabel(0,0);
        check(until([&]{return done;}) && !ok,"delayed HTTP recall rechecks blank name");

        setCameraRuntime(0,&runtime);
        CameraProfileTestDialog dialog(testPath);
        check(dialog.httpMethod() && !dialog.testAllButton_->isEnabled(),"HTTP method loaded; Test All disabled");
        check(!dialog.discoverZoomButton_->isEnabled() && !dialog.address_->isEnabled(),"AW discovery/address disabled");
        check(dialog.requestGap_->minimum()==130 && dialog.zoomPosition_->minimum()==0x555,"AW UI gap and range");
        dialog.host_->setText("127.0.0.1"); dialog.testConnection();
        check(until([&]{return !dialog.ioBusy_;}) && dialog.trace_->toPlainText().contains("HTTP 200 RX p1"),"Profiler HTTP query and readable trace");
        check(runtime.profileTestActive(),"Profiler borrows normal runtime");
        check(dialog.applyEditsToProfile(),"AW edits validate");
        const auto saved=dir.filePath("saved.json"); check(dialog.profile_->saveAs(saved,&error),"AW Save As");
        CameraProfileDocument reload; check(reload.load(saved,&error) && reload.supportsPanasonicHttp(),"saved AW profile reload");
        dialog.releaseUdp(); check(!runtime.profileTestActive(),"Profiler releases normal runtime"); setCameraRuntime(0,nullptr);
        std::printf("Panasonic AW protocol, HTTP, runtime and Profiler: %d checks passed\n",checks);
        return 0;
    }
};
}
int main(int argc,char **argv) { QApplication app(argc,argv); if(argc!=2) return 2; return simplecam::ProfileToolTest::run(QString::fromLocal8Bit(argv[1])); }
