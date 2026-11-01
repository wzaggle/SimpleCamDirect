#include "panasonic_aw.hpp"
#include <QCoreApplication>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QUrlQuery>
#include <algorithm>

namespace simplecam {
namespace {
QNetworkRequest request(const QString &host, quint16 port, const QByteArray &c)
{
    QNetworkRequest r(awRequestUrl(host, port, c));
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    r.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    r.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    r.setRawHeader("Connection", "close");
    return r;
}
}

PanasonicAwHttpTransport::PanasonicAwHttpTransport(QObject *parent) : QObject(parent)
{
    manager_.setProxy(QNetworkProxy::NoProxy);
    timer_.setSingleShot(true); timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_,&QTimer::timeout,this,[this]() { if(busy_) send(); else pump(); });
}
PanasonicAwHttpTransport::~PanasonicAwHttpTransport() { cancel(); emergencyStops(); }
void PanasonicAwHttpTransport::configure(const QString &host,quint16 port,int gap,int timeout)
{
    if(host_!=host || port_!=port) { const bool wasEnabled=enabled_; enabled_=false; cancel(); emergencyStops(); lastFinished_.invalidate(); enabled_=wasEnabled; }
    host_=host; port_=port; gapMs_=std::clamp(gap,130,2000); timeoutMs_=std::clamp(timeout,50,30000);
}
void PanasonicAwHttpTransport::setEnabled(bool enabled)
{
    if(enabled_ && !enabled) { enabled_=false; cancel(); emergencyStops(); }
    enabled_=enabled;
}
void PanasonicAwHttpTransport::cancel()
{
    ++generation_; timer_.stop();
    if(reply_) { reply_->disconnect(this); reply_->abort(); reply_->deleteLater(); reply_.clear(); }
    auto pending=queue_; queue_.clear();
    auto active=std::move(current_); current_={}; const bool wasBusy=busy_; busy_=false;
    if(wasBusy && active.completion) active.completion(false,QStringLiteral("HTTP request cancelled"));
    while(!pending.isEmpty()) { auto r=pending.dequeue(); if(r.completion) r.completion(false,QStringLiteral("HTTP request cancelled")); }
}
void PanasonicAwHttpTransport::emergencyStops()
{
    // Best effort on disable/endpoint change/unload. Only Qt-owned objects and Qt
    // slots survive this function; no plugin callbacks outlive module unload.
    const auto stops=activeStops_; activeStops_.clear();
    for(const auto &c:stops) {
        const auto u=awRequestUrl(host_,port_,c); if(!u.isValid() || u.isEmpty()) continue;
        auto *m=new QNetworkAccessManager(QCoreApplication::instance()); m->setProxy(QNetworkProxy::NoProxy);
        auto req=request(host_,port_,c); req.setTransferTimeout(1500);
        auto *r=m->get(req); QObject::connect(r,&QNetworkReply::finished,m,&QObject::deleteLater);
    }
}
void PanasonicAwHttpTransport::enqueue(const QVector<ViscaTcpTransport::FrameCommand> &commands,
    ViscaTcpTransport::Completion completion,int delay,bool priority)
{
    if(!enabled_ || commands.isEmpty()) { if(completion) completion(false,QStringLiteral("HTTP transport disabled or empty request")); return; }
    for(const auto &c:commands) if(!validateAwCommand(c.bytes) || awRequestUrl(host_,port_,c.bytes).isEmpty()) {
        if(completion) completion(false,QStringLiteral("HTTP requires a numeric camera IP, valid port and supported AW command"));
        return;
    }
    Request r{commands,std::move(completion),0,std::max(0,delay)};
    if(priority) {
        // STOP may overtake queued manual drives, but never replay them afterward.
        QQueue<Request> keep;
        while(!queue_.isEmpty()) { auto q=queue_.dequeue(); bool drive=false;
            for(const auto &c:q.commands) drive=drive || (awStopFor(c.bytes)==commands.first().bytes && !isAwStop(c.bytes));
            if(drive) { if(q.completion) QTimer::singleShot(0,this,[q]() { q.completion(false,QStringLiteral("Movement superseded by STOP")); }); } else keep.enqueue(q);
        }
        queue_=std::move(keep); queue_.prepend(std::move(r));
        if(busy_) {
            ++generation_; timer_.stop();
            if(reply_) { reply_->disconnect(this); reply_->abort(); reply_->deleteLater(); reply_.clear(); lastFinished_.restart(); }
            auto previous=std::move(current_.completion); current_={}; busy_=false;
            if(previous) QTimer::singleShot(0,this,[previous]() { previous(false,QStringLiteral("Request interrupted by priority STOP")); });
        }
    } else queue_.enqueue(std::move(r));
    pump();
}
void PanasonicAwHttpTransport::pump()
{
    if(busy_ || !enabled_ || queue_.isEmpty()) return;
    busy_=true; current_=queue_.dequeue();
    const int gap=lastFinished_.isValid() ? std::max(0,gapMs_-int(lastFinished_.elapsed())) : 0;
    timer_.start(std::max(gap,current_.delay));
}
void PanasonicAwHttpTransport::send()
{
    if(!busy_ || !enabled_) return;
    if(current_.index>=current_.commands.size()) { finish(true,QStringLiteral("AW reply validated; acceptance does not prove motion completion")); return; }
    const auto c=current_.commands.at(current_.index);
    if(c.canSend && !c.canSend()) { finish(false,QStringLiteral("Preset name or mapping changed before transmission")); return; }
    const auto stop=awStopFor(c.bytes); if(!stop.isEmpty() && !isAwStop(c.bytes)) activeStops_.insert(stop);
    if(trace) trace(QStringLiteral("HTTP GET %1").arg(awRequestUrl(host_,port_,c.bytes).toString(QUrl::FullyEncoded)));
    auto req=request(host_,port_,c.bytes); req.setTransferTimeout(timeoutMs_);
    lastFinished_.restart();
    reply_=manager_.get(req); auto *r=reply_.data(); const auto generation=generation_;
    // Limit retained response bytes; no page/HTML or oversized response is accepted.
    connect(r,&QIODevice::readyRead,this,[r]() { if(r->bytesAvailable()>4096) r->abort(); });
    connect(r,&QNetworkReply::finished,this,[this,r,c,stop,generation]() {
        if(generation!=generation_) return;
        reply_.clear(); const auto body=r->isOpen() ? r->read(4097).trimmed() : QByteArray(); const int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error; bool ok=r->error()==QNetworkReply::NoError && status==200 && body.size()<=4096;
        if(ok) ok=validateAwReply(c.bytes,body,&error);
        else error=status==401 || status==403 ? QStringLiteral("HTTP authentication/access denied. Credential entry is not implemented in this experimental driver.") :
            QStringLiteral("HTTP request failed (status %1, network error %2)").arg(status).arg(int(r->error()));
        if(trace) trace(QStringLiteral("HTTP %1 RX %2").arg(status).arg(QString::fromLatin1(body.left(256))));
        r->deleteLater(); lastFinished_.restart();
        if(!ok) { finish(false,error); return; }
        if(isAwStop(c.bytes)) activeStops_.remove(stop);
        if(c.onReply) c.onReply(body);
        ++current_.index;
        if(current_.index>=current_.commands.size()) finish(true,QStringLiteral("AW reply validated; acceptance does not prove motion completion"));
        else timer_.start(gapMs_);
    });
}
void PanasonicAwHttpTransport::finish(bool ok,const QString &message)
{
    auto completion=std::move(current_.completion); current_={}; busy_=false;
    if(!ok && !activeStops_.isEmpty()) emergencyStops();
    QPointer<PanasonicAwHttpTransport> self(this);
    if(completion) completion(ok,message);
    if(self) pump();
}
} // namespace simplecam
