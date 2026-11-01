#include "panasonic_aw_protocol.hpp"
#include <QRegularExpression>
#include <QUrlQuery>
#include <QMap>
#include <algorithm>
namespace simplecam {
namespace {
void fail(QString *e,const QString &s) { if(e) *e=s; }
QString dec(int v) { return QStringLiteral("%1").arg(v,2,10,QLatin1Char('0')); }
QString commandText(const QByteArray &c) { return QString::fromLatin1(c); }
}
bool validateAwCommand(const QByteArray &c, QString *error)
{
    // Explicit supported operation grammar; no arbitrary paths, hosts, query strings or headers.
    static const QRegularExpression grammar(QStringLiteral(
        R"(^ptz:(?:#O[01]?|#PTS[0-9]{4}|#APC(?:[0-9A-F]{8})?|#Z[0-9]{2}|#F[0-9]{2}|#D1[01]?|#AXZ(?:[0-9A-F]{3})?|#GZ|#[RM][0-9]{2})$)"));
    const QString s = commandText(c);
    bool valid = grammar.match(s).hasMatch();
    if (s.startsWith(QStringLiteral("ptz:#PTS"))) {
        const int p=s.mid(8,2).toInt(), t=s.mid(10,2).toInt();
        valid = valid && p>=1 && p<=99 && t>=1 && t<=99;
    }
    if (s.startsWith(QStringLiteral("ptz:#Z")) || s.startsWith(QStringLiteral("ptz:#F"))) {
        const int speed=s.right(2).toInt(); valid = valid && speed>=1 && speed<=99;
    }
    if (s.startsWith(QStringLiteral("ptz:#AXZ")) && s.size()>8) {
        bool ok=false; const int z=s.mid(8).toInt(&ok,16); valid = valid && ok && z>=0x555 && z<=0xFFF;
    }
    if (!valid) fail(error,QStringLiteral("Unsupported Panasonic AW command or value. Use the documented ptz: command grammar."));
    return valid;
}

bool renderAwTemplates(const QStringList &templates, const ViscaTemplateValues &v, QVector<QByteArray> *out, QString *error)
{
    out->clear();
    if (templates.isEmpty() || templates.size()>16) { fail(error,QStringLiteral("AW template sequence must contain 1–16 steps")); return false; }
    auto speed = [](int level,int maximum) { return std::clamp(qRound(49.0*level/maximum),1,49); };
    for (QString s : templates) {
        if (v.panDirection<1 || v.panDirection>3 || v.tiltDirection<1 || v.tiltDirection>3 ||
            v.panSpeed<1 || v.panSpeed>24 || v.tiltSpeed<1 || v.tiltSpeed>20 || v.zoomSpeed<0 || v.zoomSpeed>15) {
            fail(error,QStringLiteral("Invalid normalized AW movement values")); return false;
        }
        const int p=50+(v.panDirection==3 ? 0 : (v.panDirection==1 ? -1 : 1)*speed(v.panSpeed,24));
        const int t=50+(v.tiltDirection==3 ? 0 : (v.tiltDirection==1 ? 1 : -1)*speed(v.tiltSpeed,20));
        if (s.contains(QStringLiteral("{AW_PI}")) && (v.physicalPreset<1 || v.physicalPreset>100)) {
            fail(error,QStringLiteral("AW-HE130 preset must be 1–100")); return false;
        }
        if (s.contains(QStringLiteral("{AW_ZP}")) && (v.zoomPosition<0x555 || v.zoomPosition>0xFFF)) {
            fail(error,QStringLiteral("AW zoom position must be 0x555–0xFFF")); return false;
        }
        s.replace(QStringLiteral("{AW_P}"),dec(p)); s.replace(QStringLiteral("{AW_T}"),dec(t));
        s.replace(QStringLiteral("{AW_PI}"),dec(v.physicalPreset-1));
        s.replace(QStringLiteral("{AW_ZP}"),QStringLiteral("%1").arg(v.zoomPosition,3,16,QLatin1Char('0')).toUpper());
        s.replace(QStringLiteral("{AW_ZIN}"),dec(50+speed(v.zoomSpeed,15)));
        s.replace(QStringLiteral("{AW_ZOUT}"),dec(50-speed(v.zoomSpeed,15)));
        const QByteArray c=s.trimmed().toLatin1();
        if (!validateAwCommand(c,error)) { out->clear(); return false; }
        out->append(c);
    }
    return true;
}

bool awBindingMatches(const QString &id,const QVector<QByteArray> &commands)
{
    static const QMap<QString,QString> allowed={
        {"powerInquiry","^ptz:#O$"},{"powerOn","^ptz:#O1$"},{"powerOff","^ptz:#O0$"},
        {"movePanTilt","^ptz:#PTS[0-9]{4}$"},{"stopPanTilt","^ptz:#PTS5050$"},
        {"home","^ptz:#APC80008000$"},{"positionInquiry","^ptz:#APC$"},
        {"zoomIn","^ptz:#Z(?:5[1-9]|[6-9][0-9])$"},{"zoomOut","^ptz:#Z(?:0[1-9]|[1-4][0-9])$"},
        {"zoomStop","^ptz:#Z50$"},{"zoomAbsolute","^ptz:#AXZ[0-9A-F]{3}$"},{"zoomInquiry","^ptz:#(?:GZ|AXZ)$"},
        {"focusManual","^ptz:#D10$"},{"focusAuto","^ptz:#D11$"},
        {"focusNear","^ptz:#F(?:0[1-9]|[1-4][0-9])$"},{"focusFar","^ptz:#F(?:5[1-9]|[6-9][0-9])$"},
        {"focusStop","^ptz:#F50$"},{"presetRecall","^ptz:#R[0-9]{2}$"},{"presetStore","^ptz:#M[0-9]{2}$"}
    };
    if(!allowed.contains(id) || commands.size()!=1) return false;
    return QRegularExpression(allowed.value(id)).match(QString::fromLatin1(commands.first())).hasMatch();
}

QUrl awRequestUrl(const QString &host, quint16 port, const QByteArray &command)
{
    // Numeric IPv4 only in this first HTTP adapter; reject userinfo, paths and queries.
    const auto octets=host.split(QLatin1Char('.'));
    if(octets.size()!=4 || !port || !validateAwCommand(command)) return {};
    for(const auto &o:octets) {
        bool ok=false; const int n=o.toInt(&ok);
        if(!ok || n<0 || n>255 || QString::number(n)!=o) return {};
    }
    if(host==QStringLiteral("0.0.0.0") || host==QStringLiteral("255.255.255.255")) return {};
    QUrl u; u.setScheme(QStringLiteral("http")); u.setHost(host); u.setPort(port);
    u.setPath(QStringLiteral("/cgi-bin/aw_ptz"));
    QUrlQuery q; q.addQueryItem(QStringLiteral("cmd"),commandText(command.mid(4)));
    q.addQueryItem(QStringLiteral("res"),QStringLiteral("1")); u.setQuery(q);
    return u;
}

bool decodeAwPower(const QByteArray &r, bool *on)
{
    const auto s=r.trimmed(); if(s!="p0" && s!="p1") return false;
    *on=s=="p1"; return true; // p3 is transitioning, never reported as ON.
}
bool decodeAwZoom(const QByteArray &r, int *position)
{
    static const QRegularExpression re(QStringLiteral(R"(^(?:gz|axz)([0-9A-Fa-f]{3})$)"));
    const auto m=re.match(QString::fromLatin1(r.trimmed())); if(!m.hasMatch()) return false;
    const int n=m.captured(1).toInt(nullptr,16); if(n<0x555 || n>0xFFF) return false;
    *position=n; return true;
}
bool validateAwReply(const QByteArray &command, const QByteArray &reply, QString *error)
{
    const auto c=command.mid(4), r=reply.trimmed();
    bool valid=false;
    if(c=="#O") valid=r=="p0" || r=="p1" || r=="p3";
    else if(c=="#O0" || c=="#O1") valid=r=="p"+c.right(1);
    else if(c=="#GZ" || c=="#AXZ") { int n; valid=decodeAwZoom(r,&n); }
    else if(c.startsWith("#AXZ")) valid=r=="axz"+c.mid(4);
    else if(c=="#APC") { static const QRegularExpression re(QStringLiteral("^aPC[0-9A-Fa-f]{8}$")); valid=re.match(QString::fromLatin1(r)).hasMatch(); }
    else if(c.startsWith("#APC")) valid=r=="aPC"+c.mid(4);
    else if(c.startsWith("#PTS")) valid=r=="pTS"+c.mid(4);
    else if(c.startsWith("#Z")) valid=r=="zS"+c.mid(2);
    else if(c.startsWith("#F")) valid=r=="fS"+c.mid(2);
    else if(c=="#D1") valid=r=="d10" || r=="d11";
    else if(c.startsWith("#D1")) valid=r=="d1"+c.mid(3);
    else if(c.startsWith("#R") || c.startsWith("#M")) valid=r=="s"+c.mid(2);
    if (!valid) fail(error,r.startsWith("ER") ? QStringLiteral("Panasonic rejected command: %1").arg(QString::fromLatin1(r.left(32))) :
        QStringLiteral("Unexpected AW reply (not an acknowledgement of this command)"));
    return valid;
}
bool isAwStop(const QByteArray &c) { return c=="ptz:#PTS5050" || c=="ptz:#Z50" || c=="ptz:#F50"; }
QByteArray awStopFor(const QByteArray &c)
{
    if(c.startsWith("ptz:#PTS")) return "ptz:#PTS5050";
    if(c.startsWith("ptz:#Z")) return "ptz:#Z50";
    if(c.startsWith("ptz:#F")) return "ptz:#F50";
    return {};
}

}
