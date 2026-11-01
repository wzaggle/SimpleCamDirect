#pragma once

#include "camera_profile.hpp"

#include <QDialog>
#include <QElapsedTimer>

#include <QByteArray>
#include <QPointer>
#include <QString>
#include <QVector>

#include <functional>

class QComboBox;
class QCheckBox;
class QCloseEvent;
class QEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTcpSocket;
class QWidget;

namespace simplecam {

class CameraProfileDocument;
class CameraRuntime;
class ViscaUdpTransport;
class PanasonicAwHttpTransport;

class CameraProfileTestDialog final : public QDialog {
public:
	explicit CameraProfileTestDialog(const QString &defaultProfilePath, QWidget *parent = nullptr);
	~CameraProfileTestDialog() override;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
    void done(int result) override;

private:
    friend class ProfileToolTest;
    bool httpMethod() const;
    bool sonyUdp() const;
    bool udpMethod() const;
    void newProfile();
    void stopTcpMovement();
    bool supportedTransport() const;
    void updateConnectionUi();
    bool prepareUdp(QString *error);
    void releaseUdp();
    void setTestBusy(bool busy);
	void buildUi();
	void loadProfile(const QString &path);
	void populateCommands();

	void setStatus(const QString &message, bool error = false);
	ViscaTemplateValues templateValues() const;
	QStringList selectedTemplates() const;
	QVector<QByteArray> renderSelected(QString *error = nullptr) const;
	void renderCurrentCommand();
	void sendCurrentCommand();
	void startTestAll();
	void runTestAllCommand(int index);
	void finishTestAll();
	void setCommandResult(const QString &commandId, const QString &result, const QString &details = {});
	void setCommandInteractionEnabled(bool enabled);
	void testConnection();
	void insertToken(const QString &token);
	bool applyEditsToProfile();
	void saveAs();
	void runPacketSequence(const QVector<QByteArray> &packets, const QString &commandId,
			       std::function<void(bool, const QVector<QByteArray> &, const QString &)> completion);
	void sendOnePacket(const QByteArray &packet, bool waitForCompletion, bool inquiry,
			   std::function<void(bool, const QByteArray &, const QString &)> completion, int timeoutMs = 5000);
    void sendTcpPacket(const QByteArray &packet, bool waitForCompletion, bool inquiry,
        std::function<void(bool, const QByteArray &, const QString &)> completion, int timeoutMs);
	void queryCameraPower(std::function<void(bool, bool, const QString &)> completion);
	void startZoomDiscovery();
	void pollZoomDiscovery();
	void queryZoom(std::function<void(bool, int, const QString &)> completion);
	void finishZoomDiscovery(bool cancelled = false, bool timedOut = false, bool noProgress = false);
	void completeZoomDiscoveryUi();

    PanasonicAwHttpTransport *http_ = nullptr;
    ViscaUdpTransport *udp_ = nullptr;
    QVector<QPointer<CameraRuntime>> borrowed_;
    QString borrowedHost_;
    quint16 borrowedPort_ = 0;
    QWidget *udpControls_ = nullptr;
    QSpinBox *localPort_ = nullptr;
    QSpinBox *requestGap_ = nullptr;
    QSpinBox *resetDelay_ = nullptr;
    QSpinBox *replyTimeout_ = nullptr;
    QLabel *connectionNote_ = nullptr;
    QPushButton *releaseButton_ = nullptr;
    bool sessionActive_ = false;
    quint64 sessionGeneration_ = 0;
    QVector<QPointer<QTcpSocket>> tcpSockets_;
    QVector<QByteArray> tcpStops_;
    QElapsedTimer lastTcpReply_;
	QString defaultProfilePath_;
	CameraProfileDocument *profile_ = nullptr;
    QLineEdit *profileName_ = nullptr;
    QLineEdit *profileVersion_ = nullptr;
    QLabel *profileInfo_ = nullptr;
    QCheckBox *sameLocalPort_ = nullptr;
    QWidget *localPortRow_ = nullptr;
    QWidget *resetDelayRow_ = nullptr;
    QLineEdit *manufacturer_ = nullptr;
    QLineEdit *model_ = nullptr;
	QComboBox *connectionVariant_ = nullptr;
	QComboBox *digitalZoomMode_ = nullptr;
	QLineEdit *profilePath_ = nullptr;
	QLineEdit *host_ = nullptr;
	QSpinBox *port_ = nullptr;
	QSpinBox *address_ = nullptr;
	QSpinBox *panSpeed_ = nullptr;
	QSpinBox *tiltSpeed_ = nullptr;
	QSpinBox *panDirection_ = nullptr;
	QSpinBox *tiltDirection_ = nullptr;
	QSpinBox *zoomSpeed_ = nullptr;
	QSpinBox *zoomPosition_ = nullptr;
	QSpinBox *presetNumber_ = nullptr;
	QTableWidget *commands_ = nullptr;
	QPlainTextEdit *trace_ = nullptr;
	QPlainTextEdit *preview_ = nullptr;
	QLabel *status_ = nullptr;
	QWidget *profileControls_ = nullptr;
	QWidget *connectionControls_ = nullptr;
	QWidget *valueControls_ = nullptr;
	QWidget *tokenControls_ = nullptr;
	QPushButton *discoverZoomButton_ = nullptr;
	QPushButton *testConnectionButton_ = nullptr;
	QPushButton *testAllButton_ = nullptr;
	QPointer<QWidget> activeEditor_;
	bool ioBusy_ = false;
	bool testAllActive_ = false;
	bool cameraInStandby_ = false;
	bool zoomDiscoveryActive_ = false;
	bool zoomDiscoveryCancelRequested_ = false;
	bool zoomDiscoveryFinishing_ = false;
	int zoomDiscoveryInitial_ = -1;
	int zoomDiscoveryLast_ = -1;
	int zoomDiscoveryStable_ = 0;
	int zoomDiscoveryPolls_ = 0;
	int zoomDiscoveryMaximum_ = 0;
	int zoomDiscoveryNoProgressPolls_ = 0;
	bool zoomDiscoveryAdvanced_ = false;
};

} // namespace simplecam
