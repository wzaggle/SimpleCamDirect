#pragma once

#include "camera_runtime.hpp"
#include "panasonic_aw_protocol.hpp"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QSet>

namespace simplecam {

class PanasonicAwHttpTransport final : public QObject {
public:
    explicit PanasonicAwHttpTransport(QObject *parent = nullptr);
    ~PanasonicAwHttpTransport() override;
    void configure(const QString &host, quint16 port, int gapMs, int timeoutMs);
    void setEnabled(bool enabled);
    void enqueue(const QVector<ViscaTcpTransport::FrameCommand> &, ViscaTcpTransport::Completion = {},
                 int delayBeforeMs = 0, bool priorityStop = false);
    std::function<void(const QString &)> trace;
private:
    struct Request {
        QVector<ViscaTcpTransport::FrameCommand> commands;
        ViscaTcpTransport::Completion completion;
        int index = 0;
        int delay = 0;
    };
    void pump();
    void send();
    void finish(bool ok, const QString &message);
    void cancel();
    void emergencyStops();
    QNetworkAccessManager manager_;
    QPointer<QNetworkReply> reply_;
    QTimer timer_;
    QElapsedTimer lastFinished_;
    QQueue<Request> queue_;
    Request current_;
    QSet<QByteArray> activeStops_;
    QString host_;
    quint16 port_ = 80;
    int gapMs_ = 130;
    int timeoutMs_ = 3000;
    bool enabled_ = true;
    bool busy_ = false;
    quint64 generation_ = 0;
};
} // namespace simplecam
