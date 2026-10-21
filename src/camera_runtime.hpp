#pragma once

#include "visca_packets.hpp"

#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVector>

#include <functional>

class QTcpSocket;

namespace simplecam {

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
	bool swapSlots(int pageA, int slotA, int pageB, int slotB);
	int allocatePhysicalPreset(int pageIndex, int slotIndex);
	void setCapacity(int capacity);

private:
	bool validSlot(int pageIndex, int slotIndex) const;
	void load();
	void save() const;

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
	};

	explicit ViscaTcpTransport(QObject *parent = nullptr);

	void setEndpoint(const QString &host, quint16 port, int cameraAddress);
	void enqueue(const QVector<FrameCommand> &commands, Completion completion = {});
	void testConnection(Completion completion);

private:
	struct Request {
		QVector<FrameCommand> commands;
		Completion completion;
		int commandIndex = 0;
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
};

class CameraRuntime final : public QObject {
public:
	using Completion = ViscaTcpTransport::Completion;

	explicit CameraRuntime(const QString &settingsPath, QObject *parent = nullptr);

	QString cameraName() const;
	QString host() const;
	quint16 port() const;
	int cameraAddress() const;
	int capacity() const;
	void setCameraName(const QString &name);
	void setHost(const QString &host);
	void setPort(quint16 port);
	void setCameraAddress(int address);
	void setCapacity(int capacity);

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
	void home(Completion completion = {});
	void backlight(bool enabled, Completion completion = {});
	void recallLogicalPreset(int logicalButton, int delayMs, Completion completion = {});
	void storeLogicalPreset(int pageIndex, int slotIndex, Completion completion = {});

private:
	void sendPacket(const ViscaPacket &packet, bool waitForCompletion, Completion completion);
	void writeCameraSettings();
	int resolvePhysicalPreset(int pageIndex, int slotIndex) const;

	QSettings settings_;
	int cameraAddress_ = 1;
	quint16 port_ = 1259;
	QString cameraName_ = QStringLiteral("Camera 1");
	QString host_;
	int capacity_ = 255;
	int activePage_ = 0;
	PresetCatalog presets_;
	ViscaTcpTransport transport_;
};

CameraRuntime *cameraRuntime();
void setCameraRuntime(CameraRuntime *runtime);

} // namespace simplecam
