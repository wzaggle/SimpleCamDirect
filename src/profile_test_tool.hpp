#pragma once

#include "camera_profile.hpp"

#include <QDialog>

#include <QByteArray>
#include <QPointer>
#include <QString>
#include <QVector>

#include <functional>

class QComboBox;
class QCloseEvent;
class QEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QWidget;

namespace simplecam {

class CameraProfileDocument;

class CameraProfileTestDialog final : public QDialog {
public:
	explicit CameraProfileTestDialog(const QString &defaultProfilePath, QWidget *parent = nullptr);
	~CameraProfileTestDialog() override;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	void buildUi();
	void loadProfile(const QString &path);
	void populateCommands();
	void selectSlot(int index);
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
	void applyEditsToProfile();
	void saveAs();
	void runPacketSequence(const QVector<QByteArray> &packets, const QString &commandId,
			       std::function<void(bool, const QVector<QByteArray> &, const QString &)> completion);
	void sendOnePacket(const QByteArray &packet, bool waitForCompletion, bool inquiry,
			   std::function<void(bool, const QByteArray &, const QString &)> completion, int timeoutMs = 5000);
	void queryCameraPower(std::function<void(bool, bool, const QString &)> completion);
	void waitForCameraOn(int attemptsRemaining, std::function<void(bool, const QString &)> completion);
	void wakeCamera(std::function<void(bool, const QString &)> completion);
	void startZoomDiscovery();
	void pollZoomDiscovery();
	void queryZoom(std::function<void(bool, int, const QString &)> completion);
	void finishZoomDiscovery(bool cancelled = false, bool timedOut = false, bool noProgress = false);
	void completeZoomDiscoveryUi();

	QString defaultProfilePath_;
	CameraProfileDocument *profile_ = nullptr;
	QComboBox *cameraSlot_ = nullptr;
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
