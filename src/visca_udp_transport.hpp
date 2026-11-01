#pragma once
#include "camera_runtime.hpp"
#include <QElapsedTimer>
#include <QHostAddress>

class QUdpSocket;
namespace simplecam {
// Raw VISCA or Sony VISCA/IP envelope over unconnected UDP. Socket sharing is local to this
// module and routes cameras by IP; no camera reply source-port assumption.
class ViscaUdpTransport final : public QObject {
public:
    using Completion = ViscaTcpTransport::Completion;
    using FrameCommand = ViscaTcpTransport::FrameCommand;
    explicit ViscaUdpTransport(QObject *parent = nullptr);
    ~ViscaUdpTransport() override;
    void configure(const QString &host, quint16 port, quint16 localPort, int gapMs, int timeoutMs, int resetSetupMs = 100, bool sonyFraming = true, int address = 1);
    void setEnabled(bool enabled);
    void enqueue(const QVector<FrameCommand> &commands, Completion completion = {}, int delayMs = 0, bool priorityStop = false);
    void receive(const QByteArray &datagram, quint16 senderPort = 0);
    std::function<void(const QString &)> trace;
    QHostAddress address() const { return address_; }
private:
    struct Request { QVector<FrameCommand> commands; Completion completion; int index = 0; int delayMs = 0; bool stop = false; };
    bool attach(QString *error);
    void detach();
    void cancel(const QString &reason);
    void next();
    void advance();
    void transmit();
    void finish(bool ok, const QString &message);
    void resetSession();
    void stopActiveMovement();
    QVector<QByteArray> activeStops_;
    bool write(unsigned type, quint32 sequence, const QByteArray &payload);
    QHostAddress address_;
    quint16 port_ = 52381, localPort_ = 52381;
    int gapMs_ = 100, replyTimeoutMs_ = 3000, resetSetupMs_ = 100;
    bool resetSettling_ = false;
    bool sonyFraming_ = true;
    int cameraAddress_ = 1;
    bool enabled_ = true, busy_ = false, resetting_ = false, ready_ = false, awaiting_ = false;
    quint32 sequence_ = 0;
    quint64 generation_ = 0;
    QQueue<Request> queue_;
    Request current_;
    QUdpSocket *socket_ = nullptr;
    QTimer timeout_, pacing_;
    QElapsedTimer lastReply_;
};
} // namespace simplecam
