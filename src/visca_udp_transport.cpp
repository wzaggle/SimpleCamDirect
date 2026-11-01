#include "visca_udp_transport.hpp"
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QPointer>
#include <QMap>
#include <QThread>
#include <algorithm>

namespace simplecam {
namespace {
struct Listener { QUdpSocket *socket = nullptr; QVector<QPointer<ViscaUdpTransport>> clients; };
QMap<quint16, Listener> listeners;
QByteArray envelope(unsigned type, quint32 seq, const QByteArray &payload)
{
    QByteArray bytes;
    bytes.append(char(type >> 8)); bytes.append(char(type));
    bytes.append(char(payload.size() >> 8)); bytes.append(char(payload.size()));
    for (int shift : {24,16,8,0}) bytes.append(char(seq >> shift));
    return bytes + payload;
}
unsigned byte(const QByteArray &b, int i) { return static_cast<unsigned char>(b.at(i)); }
bool continuousForStop(const QByteArray &start, const QByteArray &stop)
{
    if (start.size() < 6 || stop.size() < 6) return false;
    return start.left(4) == stop.left(4) && byte(start,1) == 1;
}
}
ViscaUdpTransport::ViscaUdpTransport(QObject *parent) : QObject(parent)
{
    timeout_.setSingleShot(true); pacing_.setSingleShot(true);
    timeout_.setTimerType(Qt::PreciseTimer); pacing_.setTimerType(Qt::PreciseTimer);
    connect(&timeout_, &QTimer::timeout, this, [this]() {
        ready_ = false;
        finish(false, QStringLiteral("VISCA UDP reply timed out (sequence %1)").arg(sequence_));
    });
    connect(&pacing_, &QTimer::timeout, this, [this]() { transmit(); });
}
ViscaUdpTransport::~ViscaUdpTransport()
{
    timeout_.stop(); pacing_.stop();
    stopActiveMovement();
    detach();
}
void ViscaUdpTransport::configure(const QString &host, quint16 port, quint16 localPort, int gapMs, int timeoutMs, int resetSetupMs, bool sonyFraming, int cameraAddress)
{
    const QHostAddress address(host.trimmed());
    resetSetupMs = std::clamp(resetSetupMs,0,2000);
    gapMs = std::clamp(gapMs, 0, 2000); timeoutMs = std::clamp(timeoutMs, 50, 30000);
    if (address == address_ && port == port_ && localPort == localPort_ && gapMs == gapMs_ && timeoutMs == replyTimeoutMs_ && resetSetupMs == resetSetupMs_ && sonyFraming == sonyFraming_ && cameraAddress == cameraAddress_) return;
    cancel(QStringLiteral("Camera UDP configuration changed"));
    detach(); address_ = address; port_ = port; localPort_ = localPort;
    gapMs_ = gapMs; replyTimeoutMs_ = timeoutMs; resetSetupMs_ = resetSetupMs;
    sonyFraming_ = sonyFraming; cameraAddress_ = sonyFraming ? 1 : std::clamp(cameraAddress,1,7);
}
void ViscaUdpTransport::setEnabled(bool enabled)
{
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    if (!enabled) { cancel(QStringLiteral("Camera slot disabled")); detach(); }
}
bool ViscaUdpTransport::attach(QString *error)
{
    if (socket_) return true;
    if (address_.protocol() != QAbstractSocket::IPv4Protocol || !port_ || !localPort_) {
        *error = QStringLiteral("VISCA UDP requires a numeric IPv4 address and nonzero remote/local ports"); return false;
    }
    for (auto it = listeners.cbegin(); it != listeners.cend(); ++it)
        for (const auto &client : it->clients)
            if (client && client != this && client->address() == address_) {
                *error = QStringLiteral("This camera IP is already assigned to another active UDP slot"); return false;
            }
    if (!listeners.contains(localPort_)) {
        auto *socket = new QUdpSocket;
        if (!socket->bind(QHostAddress::AnyIPv4, localPort_, QAbstractSocket::DontShareAddress)) {
            *error = QStringLiteral("Cannot bind UDP listening port %1: %2. Close the probe or other controller using this port.").arg(localPort_).arg(socket->errorString());
            delete socket; return false;
        }
        listeners.insert(localPort_, Listener{socket, {}});
        const quint16 port = localPort_;
        connect(socket, &QUdpSocket::readyRead, socket, [socket, port]() {
            while (socket->hasPendingDatagrams()) {
                const auto datagram = socket->receiveDatagram();
                const auto clients = listeners.value(port).clients;
                for (const auto &client : clients)
                    if (client && client->address() == datagram.senderAddress()) client->receive(datagram.data(), datagram.senderPort());
            }
        });
    }
    auto &listener = listeners[localPort_]; listener.clients.append(this); socket_ = listener.socket;
    return true;
}
void ViscaUdpTransport::detach()
{
    ready_ = false; resetting_ = false; awaiting_ = false; lastReply_.invalidate();
    if (!socket_) return;
    auto it = listeners.find(localPort_);
    socket_ = nullptr;
    if (it == listeners.end()) return;
    it->clients.removeAll(this);
    if (it->clients.isEmpty()) {
        auto *socket = it->socket; listeners.erase(it);
        socket->disconnect(); socket->close(); socket->deleteLater();
    }
}
void ViscaUdpTransport::cancel(const QString &reason)
{
    stopActiveMovement();
    ++generation_; timeout_.stop(); pacing_.stop(); ready_ = false; resetting_ = false; awaiting_ = false;
    QVector<Completion> callbacks;
    if (busy_ && current_.completion) callbacks.append(std::move(current_.completion));
    busy_ = false; current_ = {};
    while (!queue_.isEmpty()) { auto q = queue_.dequeue(); if (q.completion) callbacks.append(std::move(q.completion)); }
    for (auto callback : callbacks) QTimer::singleShot(0, this, [callback, reason]() { callback(false, reason); });
}
void ViscaUdpTransport::enqueue(const QVector<FrameCommand> &commands, Completion completion, int delayMs, bool priorityStop)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, commands, completion, delayMs, priorityStop]() {
            enqueue(commands, completion, delayMs, priorityStop);
        }, Qt::QueuedConnection); return;
    }
    if (!enabled_ || commands.isEmpty()) { if (completion) completion(false, QStringLiteral("UDP camera disabled or empty request")); return; }
    Request request{commands, std::move(completion), 0, std::clamp(delayMs,0,600000), priorityStop};
    if (priorityStop) {
        // Remove unsent starts for this axis so release cannot be followed by a
        // stale queued start. Other scene requests retain their relative order.
        for (int i = queue_.size()-1; i >= 0; --i) {
            if (continuousForStop(queue_[i].commands.first().bytes, commands.first().bytes)) {
                auto old = queue_.takeAt(i);
                if (old.completion) QTimer::singleShot(0, this, [old]() { old.completion(false, QStringLiteral("Movement superseded by STOP")); });
            }
        }
        queue_.prepend(std::move(request));
        if (busy_ && !resetting_) {
            ++generation_; pacing_.stop(); timeout_.stop(); awaiting_ = false;
            finish(false, QStringLiteral("Request interrupted by priority STOP"));
        }
    } else queue_.enqueue(std::move(request));
    next();
}
void ViscaUdpTransport::next()
{
    if (busy_ || !enabled_ || queue_.isEmpty()) return;
    current_ = queue_.dequeue(); busy_ = true;
    QString error;
    if (!attach(&error)) { finish(false, error); return; }
    if (!ready_ && sonyFraming_) { resetSession(); return; }
    if (!sonyFraming_) ready_ = true;
    const auto generation = ++generation_;
    QTimer::singleShot(current_.delayMs, this, [this,generation]() { if (busy_ && generation == generation_) advance(); });
}
bool ViscaUdpTransport::write(unsigned type, quint32 sequence, const QByteArray &payload)
{
    auto bytes = sonyFraming_ ? envelope(type, sequence, payload) : payload;
    if (!sonyFraming_ && !bytes.isEmpty()) bytes[0] = char(0x80 + cameraAddress_);
    const bool sent = socket_ && socket_->writeDatagram(bytes, address_, port_) == bytes.size();
    if (trace) trace(QStringLiteral("UDP TX %1:%2 seq=%3 %4 · %5")
        .arg(address_.toString()).arg(port_).arg(sequence)
        .arg(QString::fromLatin1(bytes.toHex(' ').toUpper()), sent ? QStringLiteral("sent") : QStringLiteral("send failed")));
    return sent;
}
void ViscaUdpTransport::resetSession()
{
    sequence_ = 0; resetting_ = true; awaiting_ = true;
    if (!write(0x0200, 0, QByteArray::fromHex("01"))) { finish(false, QStringLiteral("Sony UDP reset send failed")); return; }
    timeout_.start(replyTimeoutMs_);
}
void ViscaUdpTransport::advance()
{
    if (!busy_) return;
    if (current_.index >= current_.commands.size()) { finish(true, QStringLiteral("VISCA UDP command sequence completed")); return; }
    const int elapsed = lastReply_.isValid() ? int(std::min<qint64>(lastReply_.elapsed(),2000)) : gapMs_;
    const int gap = resetSettling_ ? std::max(0,resetSetupMs_-elapsed) :
        (current_.stop && current_.index == 0 ? 0 : std::max(0,gapMs_-elapsed));
    pacing_.start(gap);
}
void ViscaUdpTransport::transmit()
{
    if (!busy_ || resetting_ || current_.index >= current_.commands.size()) return;
    const auto &pending=current_.commands.at(current_.index);
    if(pending.canSend && !pending.canSend()) {
        finish(false,QStringLiteral("Preset recall canceled: button is blank or its mapping changed")); return;
    }
    QByteArray payload = pending.bytes;
    if (payload.size() < 3 || payload.size() > 16 || byte(payload,payload.size()-1) != 0xFF ||
        (byte(payload,1) != 0x01 && byte(payload,1) != 0x09)) {
        finish(false, QStringLiteral("Unsupported VISCA UDP payload")); return;
    }
    resetSettling_ = false;
    payload[0] = char(0x80 + cameraAddress_); ++sequence_; awaiting_ = true;
    QByteArray stop;
    if (payload.size() == 6 && payload.mid(1,3) == QByteArray::fromHex("010407") && byte(payload,4) != 0)
        stop = QByteArray::fromHex("8101040700ff");
    if (payload.size() == 6 && payload.mid(1,3) == QByteArray::fromHex("010408") && byte(payload,4) != 0)
        stop = QByteArray::fromHex("8101040800ff");
    if (payload.size() == 9 && payload.mid(1,3) == QByteArray::fromHex("010601") &&
        (byte(payload,6) != 3 || byte(payload,7) != 3))
        stop = QByteArray::fromHex("8101060101010303ff");
    if (!stop.isEmpty() && !activeStops_.contains(stop)) activeStops_.append(stop);
    const unsigned type = byte(payload,1) == 0x09 ? 0x0110 : 0x0100;
    if (!write(type, sequence_, payload)) { ready_ = false; finish(false, QStringLiteral("VISCA UDP datagram send failed")); return; }
    const int commandTimeout = current_.commands.at(current_.index).completionTimeoutMs;
    timeout_.start(commandTimeout > 2500 ? commandTimeout : replyTimeoutMs_);
}
void ViscaUdpTransport::receive(const QByteArray &data, quint16 senderPort)
{
    if (trace) trace(QStringLiteral("UDP RX %1:%2 %3 (validation follows)").arg(address_.toString()).arg(senderPort)
        .arg(QString::fromLatin1(data.toHex(' ').toUpper())));
    if (!busy_ || !awaiting_) return;
    QByteArray framed = data;
    if (!sonyFraming_) {
        if (data.size()<3 || data.size()>4096 || byte(data,0)!=unsigned((cameraAddress_+8)<<4) || byte(data,data.size()-1)!=0xff) return;
        QByteArray normalized=data; normalized[0]=char(0x90);
        framed=envelope(0x0111,sequence_,normalized);
    }
    if (framed.size()<9) return;
    const unsigned type = (byte(framed,0)<<8)|byte(framed,1), length = (byte(framed,2)<<8)|byte(framed,3);
    if (length < 1 || length > (sonyFraming_ ? 16u : 4096u) || framed.size() != int(length+8)) return;
    quint32 seq = 0; for (int i=4;i<8;++i) seq = (seq<<8)|byte(framed,i);
    const auto payload = framed.mid(8);
    if (resetting_) {
        if (type != 0x0201 || payload != QByteArray::fromHex("01")) return;
        timeout_.stop(); resetting_ = false; awaiting_ = false; ready_ = true; resetSettling_ = true; lastReply_.start();
        if (!queue_.isEmpty() && queue_.head().stop) {
            finish(false,QStringLiteral("Request interrupted by priority STOP")); return;
        }
        const auto generation = ++generation_;
        QTimer::singleShot(current_.delayMs, this, [this,generation]() { if (busy_ && generation == generation_) advance(); });
        return;
    }
    if (seq != sequence_) return;
    if (type == 0x0200 && length == 2 && byte(payload,0) == 0x0F) {
        ready_ = false; finish(false, QStringLiteral("Sony UDP control error: %1").arg(QString::fromLatin1(payload.toHex(' ')))); return;
    }
    if (type != 0x0111 || payload.size() < 3 || byte(payload,0) != 0x90 || byte(payload,payload.size()-1) != 0xFF) return;
    auto command = current_.commands.at(current_.index);
    command.bytes[0]=char(0x81); // Canonical address for response-shape and active-axis matching.
    const unsigned response = byte(payload,1) & 0xF0;
    if (response == 0x60) { finish(false, QStringLiteral("VISCA error over UDP: %1").arg(QString::fromLatin1(payload.toHex(' ')))); return; }
    if (response == 0x40) {
        if (payload.size() != 3) return;
        if (command.waitForCompletion || byte(command.bytes,1) == 0x09) return;
    } else if (response == 0x50) {
        if (byte(command.bytes,1) == 0x09 && byte(payload,1) != 0x50) return;
    } else return;
    if (byte(command.bytes,1) == 0x09) {
        if (command.bytes.mid(2) == QByteArray::fromHex("0400ff") &&
            payload != QByteArray::fromHex("905002ff") && payload != QByteArray::fromHex("905003ff")) return;
        if (command.bytes.mid(2) == QByteArray::fromHex("0447ff")) {
            if (payload.size() != 7) return;
            for (int i=2;i<6;++i) if (byte(payload,i)>15) return;
        }
    }
        if (command.bytes.size() == 9 && command.bytes.left(4) == QByteArray::fromHex("81010601") &&
        byte(command.bytes,6) == 3 && byte(command.bytes,7) == 3)
        activeStops_.removeAll(QByteArray::fromHex("8101060101010303ff"));
    else activeStops_.removeAll(command.bytes);
    if (command.onReply) command.onReply(payload);
    timeout_.stop(); awaiting_ = false; lastReply_.start(); ++current_.index;
    advance();
}
void ViscaUdpTransport::stopActiveMovement()
{
    if (socket_) for (const auto &stop : activeStops_) write(0x0100, ++sequence_, stop);
    activeStops_.clear();
}
void ViscaUdpTransport::finish(bool ok, const QString &message)
{
    if (!busy_) return;
    if (!ok && message != QStringLiteral("Request interrupted by priority STOP")) stopActiveMovement();
    timeout_.stop(); pacing_.stop(); awaiting_ = false; resetting_ = false;
    auto callback = std::move(current_.completion); current_ = {}; busy_ = false;
    if (callback) callback(ok, message);
    QTimer::singleShot(0, this, [this]() { next(); });
}
} // namespace simplecam
