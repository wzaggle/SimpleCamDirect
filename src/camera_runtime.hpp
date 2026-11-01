#pragma once

#include "camera_profile.hpp"

#include <QObject>
#include <QPointer>
#include <QElapsedTimer>
#include <QByteArray>
#include <QQueue>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVector>

#include <functional>
#include <cstdint>
#include <array>

class QTcpSocket;

namespace simplecam {

class CameraRuntime;
class ViscaUdpTransport;
inline constexpr int CameraSlotCount = 5;
using CameraRuntimeArray = std::array<CameraRuntime *, CameraSlotCount>;

struct PresetSlot {
	QString label;
	int physicalPreset = -1;
};

struct PresetPage {
	QString description;
	QVector<PresetSlot> slots;
};

class PresetCatalog final {
public:
	static constexpr int SlotsPerPage = 14;
	static constexpr int MinimumPages = 5;

	PresetCatalog(QSettings &settings, int capacity);

	~PresetCatalog();
	void shareGroupsWith(PresetCatalog &owner, bool migrateLegacySlots);
	bool canRemovePage(int pageIndex) const;
	int pageCount() const;
	int maxPageCount() const;
	int capacity() const;
	const PresetPage &page(int pageIndex) const;
	const PresetSlot &slot(int pageIndex, int slotIndex) const;
	bool hasUsablePreset(int pageIndex, int slotIndex) const;
	QString presetLabel(int pageIndex, int slotIndex) const;

	bool setPageDescription(int pageIndex, const QString &description);
	bool addPage();
	bool removePageIfBlank(int pageIndex);
	bool setLabel(int pageIndex, int slotIndex, const QString &label);
	bool clearLabel(int pageIndex, int slotIndex);
	bool clearPageLabels(int pageIndex);
	bool clearPageLabelsForAllCameras(int pageIndex);
	bool swapSlots(int pageA, int slotA, int pageB, int slotB);
	int allocatePhysicalPreset(int pageIndex, int slotIndex, int allowedCapacity = 0);

private:
	bool validSlot(int pageIndex, int slotIndex) const;
	void load();
	void save() const;
	void synchronizeGroups();
	PresetCatalog *groupOwner_ = nullptr;
	QVector<PresetCatalog *> groupMembers_;

	QSettings &settings_;
	QVector<PresetPage> pages_;
	int capacity_ = 255;
};

class ViscaTcpTransport final : public QObject {
public:
	using Completion = std::function<void(bool, const QString &)>;

	struct FrameCommand {
		QByteArray bytes;
		bool waitForCompletion = false;
		std::function<void(const QByteArray &)> onReply;
		int completionTimeoutMs = 2500;
        std::function<bool()> canSend;
	};

	explicit ViscaTcpTransport(QObject *parent = nullptr);

	void setEnabled(bool enabled);
	void setEndpoint(const QString &host, quint16 port, int cameraAddress);
    void setRequestGap(int gapMs) { requestGapMs_ = gapMs; }
	void enqueue(const QVector<FrameCommand> &commands, Completion completion = {}, int delayBeforeMs = 0);
	void testConnection(Completion completion);

private:
	struct Request {
		QVector<FrameCommand> commands;
		Completion completion;
		int commandIndex = 0;
		int delayBeforeMs = 0;
	};

	void startNextRequest();
	void startCurrentCommand();
	void sendCurrentCommand();
	void consumeReplies();
	void handleReply(const QByteArray &frame);
	void finishCurrentCommand();
	void finishCurrentRequest(bool success, const QString &message);
	void closeSocket();

	QString host_;
	quint16 port_ = 1259;
	int cameraAddress_ = 1;
	QQueue<Request> queue_;
	Request current_;
	QTcpSocket *socket_ = nullptr;
	QTimer timeout_;
	QByteArray inputBuffer_;
	bool busy_ = false;
	bool enabled_ = true;
	unsigned long long requestGeneration_ = 0;
    int requestGapMs_ = 0;
    QElapsedTimer lastReply_;
};

class CameraRuntime final : public QObject {
public:
	using Completion = ViscaTcpTransport::Completion;
	enum class PresetRecallOrigin { CameraControl, ObsSource };
	using PresetRecallObserver = std::function<void(int logicalButton, PresetRecallOrigin origin)>;
	using PresetRecallResultObserver =
		std::function<void(int logicalButton, int pageIndex, PresetRecallOrigin origin, bool success)>;
	using ZoomPositionObserver = std::function<void(int sliderValue)>;

	explicit CameraRuntime(const QString &settingsPath, const QString &defaultName = QStringLiteral("Camera 1"),
			const QString &defaultProfilePath = {}, QObject *parent = nullptr);

	bool isEnabled() const;
	void setEnabled(bool enabled);
	QString cameraName() const;
	QString cameraFeedSourceUuid() const;
	QString host() const;
	quint16 port() const;
	int cameraAddress() const;
    bool acquireProfileTest(QObject *owner);
    void releaseProfileTest(QObject *owner);
    bool profileTestActive() const { return !profileTestOwner_.isNull(); }
    bool usesSonyUdp() const;
    bool usesUdp() const;
    bool migrateLegacyConnection(const QString &directory, QString *error=nullptr);
    QString connectionSummary() const;
    bool profileOwnsConnection() const { return profile_.profileOwnsConnection(); }
    quint16 udpLocalPort() const;
    int requestGapMs() const;
    int replyTimeoutMs() const;
    bool supportsCommand(const QString &id) const;
	int capacity() const;
	bool hasUsablePreset(int pageIndex, int slotIndex) const;
    bool canRecallPreset(int pageIndex,int slotIndex) const;
	QString profilePath() const;
	QString profileId() const;
	QString profileDisplayName() const;
	QString profileLoadError() const;
	bool hasLoadedProfile() const;
	int zoomAbsoluteTemplateCount() const;
	bool canSetZoomPosition() const;
	void setCameraName(const QString &name);
	void setCameraFeedSourceUuid(const QString &uuid);
	void setHost(const QString &host);
	void setPort(quint16 port);
	void setCameraAddress(int address);
	bool setProfilePath(const QString &path, QString *error = nullptr);
	void setSharedPresetOwner(CameraRuntime &owner);

	int activePage() const;
	void setActivePage(int pageIndex);
	PresetCatalog &presets();
	const PresetCatalog &presets() const;

	void testConnection(Completion completion);
	void panTilt(int panSpeed, int tiltSpeed, std::uint8_t panDirection, std::uint8_t tiltDirection,
		     Completion completion = {});
	void stopPanTilt(int panSpeed, int tiltSpeed, Completion completion = {});
	void zoom(bool tele, Completion completion = {});
	void stopZoom(Completion completion = {});
	void setZoomPosition(int sliderValue, Completion completion = {});
	void home(Completion completion = {});
	void backlight(bool enabled, Completion completion = {});
	void setManualFocus(bool manual, Completion completion = {});
	void focusDrive(bool far, Completion completion = {});
	void stopFocus(Completion completion = {});
	void exposureCompensation(bool enabled, Completion completion = {});
	void setCameraPower(bool on, Completion completion = {});
	void recallLogicalPreset(int logicalButton, int delayMs, Completion completion = {},
				 PresetRecallOrigin origin = PresetRecallOrigin::CameraControl);
	void storeLogicalPreset(int pageIndex, int slotIndex, Completion completion = {});
	void setPresetRecallObserver(PresetRecallObserver observer);
	void setPresetRecallResultObserver(PresetRecallResultObserver observer);
	void setZoomPositionObserver(ZoomPositionObserver observer);
	int lastZoomSliderValue() const;
	void initializeZoomPosition();

private:
	ViscaTemplateValues templateValues(int physicalPreset = 1, int panSpeed = 8, int tiltSpeed = 8,
				    std::uint8_t panDirection = 1, std::uint8_t tiltDirection = 3,
				    int zoomSpeed = 8, int zoomPosition = 0) const;
	bool renderCommandPackets(const QString &commandId, const ViscaTemplateValues &values,
				  QVector<QByteArray> *packets, QString *error = nullptr) const;
	void sendProfileCommand(const QString &commandId, const ViscaTemplateValues &values,
				Completion completion, int delayBeforeMs = 0);
	void appendZoomInquiry(QVector<ViscaTcpTransport::FrameCommand> &commands);
	void sendPresetRecallAndZoomInquiry(int physicalPreset, int delayMs, Completion completion, std::function<bool()> canSend = {});
	bool handleZoomInquiryReply(const QByteArray &reply);
	void scheduleZoomInitialization();
	void queryZoomPosition(Completion completion);
    void configureTransport();
    void enqueueCommands(const QVector<ViscaTcpTransport::FrameCommand> &commands, Completion completion = {},
                         int delayBeforeMs = 0, bool priorityStop = false);
	void writeCameraSettings();
	int resolvePhysicalPreset(int pageIndex, int slotIndex) const;

	QSettings settings_;
	bool enabled_ = true;
	QString profilePath_;
	CameraProfileDocument profile_;
	QString profileLoadError_;
	int cameraAddress_ = 1;
	quint16 port_ = 1259;
	QString cameraName_ = QStringLiteral("Camera 1");
	QString cameraFeedSourceUuid_;
	QString host_;
	int capacity_ = 255;
	int activePage_ = 0;
	PresetRecallObserver presetRecallObserver_;
	PresetRecallResultObserver presetRecallResultObserver_;
	ZoomPositionObserver zoomPositionObserver_;
	int lastZoomSliderValue_ = -1;
	bool zoomInitializationStarted_ = false;
	quint64 zoomConfigurationGeneration_ = 0;
	QTimer zoomInitializationTimer_;
	PresetCatalog presets_;
	CameraRuntime *sharedPresetOwner_ = nullptr;
	ViscaTcpTransport transport_;
    ViscaUdpTransport *udpTransport_ = nullptr;
    QPointer<QObject> profileTestOwner_;
    QMetaObject::Connection profileTestDestroyed_;
};

CameraRuntime *cameraRuntime(int cameraIndex = 0);
void setCameraRuntime(int cameraIndex, CameraRuntime *runtime);

} // namespace simplecam

