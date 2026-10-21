#include "camera_runtime.hpp"

#include <plugin-support.h>
#include <util/base.h>

#include <QAbstractSocket>
#include <QMetaObject>
#include <QThread>
#include <QTcpSocket>

#include <memory>

#include <algorithm>
#include <utility>

namespace simplecam {
namespace {

constexpr int kViscaTimeoutMs = 2500;
constexpr int kConnectTimeoutMs = 3000;
constexpr int kMaxPresetCapacity = 255;

QString defaultPageDescription(int pageIndex)
{
	return QStringLiteral("Preset Group %1").arg(pageIndex + 1);
}

QString defaultPresetLabel(int physicalPreset)
{
	return QStringLiteral("Preset %1").arg(physicalPreset);
}

QByteArray toByteArray(const ViscaPacket &packet)
{
	return QByteArray(reinterpret_cast<const char *>(packet.data()), static_cast<qsizetype>(packet.size()));
}

} // namespace

PresetCatalog::PresetCatalog(QSettings &settings, int capacity) : settings_(settings), capacity_(capacity)
{
	capacity_ = std::clamp(capacity_, 0, kMaxPresetCapacity);
	load();
}

int PresetCatalog::pageCount() const
{
	return pages_.size();
}

int PresetCatalog::maxPageCount() const
{
	return std::max(MinimumPages, (capacity_ + SlotsPerPage - 1) / SlotsPerPage);
}

int PresetCatalog::capacity() const
{
	return capacity_;
}

const PresetPage &PresetCatalog::page(int pageIndex) const
{
	return pages_.at(pageIndex);
}

const PresetSlot &PresetCatalog::slot(int pageIndex, int slotIndex) const
{
	return pages_.at(pageIndex).slots.at(slotIndex);
}

bool PresetCatalog::hasUsablePreset(int pageIndex, int slotIndex) const
{
	if (!validSlot(pageIndex, slotIndex))
		return false;
	const int physical = pages_.at(pageIndex).slots.at(slotIndex).physicalPreset;
	return physical >= 1 && physical <= capacity_;
}

QString PresetCatalog::presetLabel(int pageIndex, int slotIndex) const
{
	if (!validSlot(pageIndex, slotIndex))
		return {};
	return pages_.at(pageIndex).slots.at(slotIndex).label;
}

bool PresetCatalog::setPageDescription(int pageIndex, const QString &description)
{
	if (pageIndex < 0 || pageIndex >= pages_.size())
		return false;
	const QString clean = description.trimmed().left(22);
	if (clean.isEmpty())
		return false;
	pages_[pageIndex].description = clean;
	save();
	return true;
}

bool PresetCatalog::addPage()
{
	if (pages_.size() >= maxPageCount())
		return false;

	PresetPage added;
	added.description = defaultPageDescription(pages_.size());
	added.slots.resize(SlotsPerPage);
	pages_.append(added);
	save();
	return true;
}

bool PresetCatalog::removePageIfBlank(int pageIndex)
{
	if (pages_.size() <= MinimumPages || pageIndex < MinimumPages || pageIndex >= pages_.size())
		return false;

	const auto &candidate = pages_.at(pageIndex);
	const bool allLabelsBlank = std::all_of(candidate.slots.cbegin(), candidate.slots.cend(),
						[](const PresetSlot &slot) { return slot.label.trimmed().isEmpty(); });
	if (!allLabelsBlank)
		return false;

	pages_.removeAt(pageIndex);
	save();
	return true;
}

bool PresetCatalog::setLabel(int pageIndex, int slotIndex, const QString &label)
{
	if (!validSlot(pageIndex, slotIndex))
		return false;
	pages_[pageIndex].slots[slotIndex].label = label.trimmed().left(32);
	save();
	return true;
}

bool PresetCatalog::clearLabel(int pageIndex, int slotIndex)
{
	return setLabel(pageIndex, slotIndex, QString());
}

bool PresetCatalog::swapSlots(int pageA, int slotA, int pageB, int slotB)
{
	if (!validSlot(pageA, slotA) || !validSlot(pageB, slotB) || (pageA == pageB && slotA == slotB))
		return false;
	std::swap(pages_[pageA].slots[slotA], pages_[pageB].slots[slotB]);
	save();
	return true;
}

int PresetCatalog::allocatePhysicalPreset(int pageIndex, int slotIndex)
{
	if (!validSlot(pageIndex, slotIndex))
		return -1;

	auto &selected = pages_[pageIndex].slots[slotIndex];
	if (selected.physicalPreset >= 1 && selected.physicalPreset <= capacity_)
		return selected.physicalPreset;

	QVector<bool> used(capacity_ + 1, false);
	for (const auto &page : pages_) {
		for (const auto &slot : page.slots) {
			if (slot.physicalPreset >= 1 && slot.physicalPreset <= capacity_)
				used[slot.physicalPreset] = true;
		}
	}

	for (int candidate = 1; candidate <= capacity_; ++candidate) {
		if (!used.at(candidate)) {
			selected.physicalPreset = candidate;
			if (selected.label.isEmpty())
				selected.label = defaultPresetLabel(candidate);
			save();
			return candidate;
		}
	}
	return -1;
}

void PresetCatalog::setCapacity(int capacity)
{
	capacity_ = std::clamp(capacity, 0, kMaxPresetCapacity);
	save();
}

bool PresetCatalog::validSlot(int pageIndex, int slotIndex) const
{
	return pageIndex >= 0 && pageIndex < pages_.size() && slotIndex >= 0 && slotIndex < SlotsPerPage;
}

void PresetCatalog::load()
{
	settings_.beginGroup(QStringLiteral("presetPages"));
	const int requestedCount = settings_.value(QStringLiteral("count"), MinimumPages).toInt();
	const int count = std::clamp(requestedCount, MinimumPages, maxPageCount());
	pages_.clear();
	pages_.reserve(count);

	for (int pageIndex = 0; pageIndex < count; ++pageIndex) {
		PresetPage loaded;
		settings_.beginGroup(QString::number(pageIndex));
		loaded.description = settings_.value(QStringLiteral("description"), defaultPageDescription(pageIndex)).toString();
		loaded.slots.reserve(SlotsPerPage);

		for (int slotIndex = 0; slotIndex < SlotsPerPage; ++slotIndex) {
			settings_.beginGroup(QStringLiteral("slot%1").arg(slotIndex));
			const int defaultPhysical = pageIndex * SlotsPerPage + slotIndex + 1;
			const int mappedPhysical = defaultPhysical <= capacity_ ? defaultPhysical : -1;
			PresetSlot loadedSlot;
			loadedSlot.physicalPreset = settings_.value(QStringLiteral("physicalPreset"), mappedPhysical).toInt();
			const QString fallbackLabel = loadedSlot.physicalPreset > 0 ? defaultPresetLabel(loadedSlot.physicalPreset) : QString();
			loadedSlot.label = settings_.value(QStringLiteral("label"), fallbackLabel).toString().left(32);
			loaded.slots.append(loadedSlot);
			settings_.endGroup();
		}
		settings_.endGroup();
		pages_.append(loaded);
	}
	settings_.endGroup();
	if (pages_.size() < MinimumPages) {
		while (pages_.size() < MinimumPages) {
			PresetPage page;
			page.description = defaultPageDescription(pages_.size());
			page.slots.resize(SlotsPerPage);
			pages_.append(page);
		}
	}
	save();
}

void PresetCatalog::save() const
{
	settings_.beginGroup(QStringLiteral("presetPages"));
	settings_.remove(QString());
	settings_.setValue(QStringLiteral("count"), pages_.size());
	for (int pageIndex = 0; pageIndex < pages_.size(); ++pageIndex) {
		settings_.beginGroup(QString::number(pageIndex));
		settings_.setValue(QStringLiteral("description"), pages_.at(pageIndex).description);
		for (int slotIndex = 0; slotIndex < SlotsPerPage; ++slotIndex) {
			settings_.beginGroup(QStringLiteral("slot%1").arg(slotIndex));
			settings_.setValue(QStringLiteral("physicalPreset"), pages_.at(pageIndex).slots.at(slotIndex).physicalPreset);
			settings_.setValue(QStringLiteral("label"), pages_.at(pageIndex).slots.at(slotIndex).label);
			settings_.endGroup();
		}
		settings_.endGroup();
	}
	settings_.endGroup();
	settings_.sync();
}

ViscaTcpTransport::ViscaTcpTransport(QObject *parent) : QObject(parent)
{
	QObject::connect(&timeout_, &QTimer::timeout, this, [this]() {
		finishCurrentRequest(false, QStringLiteral("VISCA reply timed out at %1:%2").arg(host_).arg(port_));
	});
}

void ViscaTcpTransport::setEndpoint(const QString &host, quint16 port, int cameraAddress)
{
	host_ = host.trimmed();
	port_ = port;
	cameraAddress_ = cameraAddress;
}

void ViscaTcpTransport::enqueue(const QVector<FrameCommand> &commands, Completion completion, int delayBeforeMs)
{
	if (commands.isEmpty()) {
		if (completion)
			completion(true, QStringLiteral("No VISCA command was needed"));
		return;
	}

	Request request{commands, std::move(completion), 0, std::clamp(delayBeforeMs, 0, 600000)};
	auto queueRequest = [this, request = std::move(request)]() mutable {
		if (host_.isEmpty() || port_ == 0) {
			if (request.completion)
				request.completion(false, QStringLiteral("Set the camera IP address and TCP port first"));
			return;
		}
		queue_.enqueue(std::move(request));
		startNextRequest();
	};
	if (QThread::currentThread() == thread())
		queueRequest();
	else
		QMetaObject::invokeMethod(this, std::move(queueRequest), Qt::QueuedConnection);
}

void ViscaTcpTransport::testConnection(Completion completion)
{
	const QString host = host_;
	const quint16 port = port_;
	if (host.isEmpty() || port == 0) {
		if (completion)
			completion(false, QStringLiteral("Enter a camera IP address and TCP port first"));
		return;
	}

	auto *socket = new QTcpSocket(this);
	auto *timer = new QTimer(socket);
	timer->setSingleShot(true);
	auto finished = std::make_shared<bool>(false);
	auto complete = [socket, timer, finished, completion](bool success, const QString &message) mutable {
		if (*finished)
			return;
		*finished = true;
		timer->stop();
		socket->abort();
		socket->deleteLater();
		if (completion)
			completion(success, message);
	};
	QObject::connect(timer, &QTimer::timeout, socket, [complete, host, port]() mutable {
		complete(false, QStringLiteral("TCP connection timed out at %1:%2").arg(host).arg(port));
	});
	QObject::connect(socket, &QTcpSocket::connected, socket, [complete, host, port]() mutable {
		complete(true, QStringLiteral("TCP connected to %1:%2; no camera command was sent").arg(host).arg(port));
	});
	QObject::connect(socket, &QTcpSocket::errorOccurred, socket,
			 [complete, socket](QAbstractSocket::SocketError) mutable {
				 complete(false, QStringLiteral("TCP connection failed: %1").arg(socket->errorString()));
			 });
	timer->start(kConnectTimeoutMs);
	socket->connectToHost(host, port);
}

void ViscaTcpTransport::startNextRequest()
{
	if (busy_ || queue_.isEmpty())
		return;
	current_ = queue_.dequeue();
	current_.commandIndex = 0;
	busy_ = true;
	if (current_.delayBeforeMs > 0) {
		QTimer::singleShot(current_.delayBeforeMs, this, [this]() { startCurrentCommand(); });
		return;
	}
	startCurrentCommand();
}

void ViscaTcpTransport::startCurrentCommand()
{
	if (!busy_)
		return;
	if (current_.commandIndex >= current_.commands.size()) {
		finishCurrentRequest(true, QStringLiteral("VISCA command sequence completed"));
		return;
	}

	inputBuffer_.clear();
	socket_ = new QTcpSocket(this);
	QObject::connect(socket_, &QTcpSocket::connected, this, [this]() { sendCurrentCommand(); });
	QObject::connect(socket_, &QTcpSocket::readyRead, this, [this]() { consumeReplies(); });
	QObject::connect(socket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
		if (socket_)
			finishCurrentRequest(false, QStringLiteral("VISCA TCP error: %1").arg(socket_->errorString()));
	});
	timeout_.start(kConnectTimeoutMs);
	socket_->connectToHost(host_, port_);
}

void ViscaTcpTransport::sendCurrentCommand()
{
	if (!socket_ || current_.commandIndex >= current_.commands.size())
		return;

	const auto &command = current_.commands.at(current_.commandIndex);
	QByteArray bytes = command.bytes;
	if (bytes.isEmpty()) {
		finishCurrentRequest(false, QStringLiteral("Empty VISCA command was rejected"));
		return;
	}
	bytes[0] = static_cast<char>(0x80 + cameraAddress_);
	const qint64 written = socket_->write(bytes);
	if (written != bytes.size()) {
		finishCurrentRequest(false, QStringLiteral("VISCA TCP write was incomplete"));
		return;
	}
	socket_->flush();
	timeout_.start(kViscaTimeoutMs);
}

void ViscaTcpTransport::consumeReplies()
{
	if (!socket_)
		return;
	QTcpSocket *const sourceSocket = socket_;
	inputBuffer_.append(sourceSocket->readAll());
	while (true) {
		const qsizetype end = inputBuffer_.indexOf(static_cast<char>(0xFF));
		if (end < 0)
			return;
		const QByteArray frame = inputBuffer_.left(end + 1);
		inputBuffer_.remove(0, end + 1);
		handleReply(frame);
		if (socket_ != sourceSocket)
			return;
	}
}

void ViscaTcpTransport::handleReply(const QByteArray &frame)
{
	if (frame.size() < 3)
		return;
	const auto responseType = static_cast<std::uint8_t>(frame.at(1)) & 0xF0;
	if (responseType == 0x40) {
		if (current_.commands.at(current_.commandIndex).waitForCompletion) {
			timeout_.start(kViscaTimeoutMs);
			return;
		}
		finishCurrentCommand();
	} else if (responseType == 0x50) {
		finishCurrentCommand();
	} else if (responseType == 0x60) {
		const int code = static_cast<unsigned char>(frame.at(frame.size() - 2));
		finishCurrentRequest(false, QStringLiteral("Camera rejected VISCA command (0x%1)").arg(code, 2, 16, QLatin1Char('0')));
	}
}

void ViscaTcpTransport::finishCurrentCommand()
{
	closeSocket();
	++current_.commandIndex;
	startCurrentCommand();
}

void ViscaTcpTransport::finishCurrentRequest(bool success, const QString &message)
{
	if (!busy_)
		return;
	closeSocket();
	const Completion callback = current_.completion;
	busy_ = false;
	if (!success)
		obs_log(LOG_WARNING, "%s", message.toUtf8().constData());
	if (callback)
		callback(success, message);
	QTimer::singleShot(0, this, [this]() { startNextRequest(); });
}

void ViscaTcpTransport::closeSocket()
{
	timeout_.stop();
	inputBuffer_.clear();
	if (!socket_)
		return;
	QTcpSocket *oldSocket = socket_;
	socket_ = nullptr;
	QObject::disconnect(oldSocket, nullptr, this, nullptr);
	oldSocket->abort();
	oldSocket->deleteLater();
}

CameraRuntime::CameraRuntime(const QString &settingsPath, QObject *parent)
	: QObject(parent), settings_(settingsPath, QSettings::IniFormat),
	  cameraAddress_(settings_.value(QStringLiteral("camera/address"), 1).toInt()),
	  port_(static_cast<quint16>(settings_.value(QStringLiteral("camera/port"), 1259).toUInt())),
	  cameraName_(settings_.value(QStringLiteral("camera/name"), QStringLiteral("Camera 1")).toString()),
	  host_(settings_.value(QStringLiteral("camera/host"), QString()).toString()),
	capacity_(settings_.value(QStringLiteral("camera/capacity"), 100).toInt()),
	  presets_(settings_, capacity_), transport_(this)
{
	if (cameraAddress_ < 1 || cameraAddress_ > 7)
		cameraAddress_ = 1;
	if (port_ == 0)
		port_ = 1259;
	capacity_ = std::clamp(capacity_, 0, kMaxPresetCapacity);
	transport_.setEndpoint(host_, port_, cameraAddress_);
	writeCameraSettings();
}

QString CameraRuntime::cameraName() const
{
	return cameraName_;
}

QString CameraRuntime::host() const
{
	return host_;
}

quint16 CameraRuntime::port() const
{
	return port_;
}

int CameraRuntime::cameraAddress() const
{
	return cameraAddress_;
}

int CameraRuntime::capacity() const
{
	return capacity_;
}

void CameraRuntime::setCameraName(const QString &name)
{
	cameraName_ = name.trimmed().left(40);
	writeCameraSettings();
}

void CameraRuntime::setHost(const QString &host)
{
	host_ = host.trimmed();
	transport_.setEndpoint(host_, port_, cameraAddress_);
	writeCameraSettings();
}

void CameraRuntime::setPort(quint16 port)
{
	port_ = port;
	transport_.setEndpoint(host_, port_, cameraAddress_);
	writeCameraSettings();
}

void CameraRuntime::setCameraAddress(int address)
{
	cameraAddress_ = std::clamp(address, 1, 7);
	transport_.setEndpoint(host_, port_, cameraAddress_);
	writeCameraSettings();
}

void CameraRuntime::setCapacity(int capacity)
{
	capacity_ = std::clamp(capacity, 1, kMaxPresetCapacity);
	presets_.setCapacity(capacity_);
	writeCameraSettings();
}

int CameraRuntime::activePage() const
{
	return activePage_;
}

void CameraRuntime::setActivePage(int pageIndex)
{
	if (presets_.pageCount() == 0) {
		activePage_ = 0;
		return;
	}
	activePage_ = std::clamp(pageIndex, 0, presets_.pageCount() - 1);
}

PresetCatalog &CameraRuntime::presets()
{
	return presets_;
}

const PresetCatalog &CameraRuntime::presets() const
{
	return presets_;
}

void CameraRuntime::testConnection(Completion completion)
{
	transport_.testConnection(std::move(completion));
}

void CameraRuntime::panTilt(int panSpeed, int tiltSpeed, std::uint8_t panDirection, std::uint8_t tiltDirection,
			    Completion completion)
{
	try {
		const auto packet = makePanTilt(cameraAddress_, panSpeed, tiltSpeed, panDirection, tiltDirection);
		sendPacket(packet, false, std::move(completion));
	} catch (const std::exception &e) {
		if (completion)
			completion(false, QString::fromUtf8(e.what()));
	}
}

void CameraRuntime::stopPanTilt(int panSpeed, int tiltSpeed, Completion completion)
{
	try {
		const auto packet = makeStopPanTilt(cameraAddress_, panSpeed, tiltSpeed);
		sendPacket(packet, false, std::move(completion));
	} catch (const std::exception &e) {
		if (completion)
			completion(false, QString::fromUtf8(e.what()));
	}
}

void CameraRuntime::zoom(bool tele, Completion completion)
{
	const auto packet = makeZoomDrive(cameraAddress_, tele);
	sendPacket(packet, false, std::move(completion));
}

void CameraRuntime::stopZoom(Completion completion)
{
	const auto packet = makeZoomStop(cameraAddress_);
	sendPacket(packet, false, std::move(completion));
}

void CameraRuntime::home(Completion completion)
{
	const auto packet = makeHome(cameraAddress_);
	sendPacket(packet, true, std::move(completion));
}

void CameraRuntime::backlight(bool enabled, Completion completion)
{
	const auto packet = makeBacklight(cameraAddress_, enabled);
	sendPacket(packet, true, std::move(completion));
}

void CameraRuntime::recallLogicalPreset(int logicalButton, int delayMs, Completion completion)
{
	if (logicalButton < 1 || logicalButton > PresetCatalog::SlotsPerPage) {
		if (completion)
			completion(false, QStringLiteral("Preset button must be 1 through 14"));
		return;
	}

	const int physical = resolvePhysicalPreset(activePage_, logicalButton - 1);
	if (physical < 1) {
		if (completion)
			completion(false, QStringLiteral("This button has no usable camera preset mapping"));
		return;
	}
	try {
		sendPacket(makePresetRecall(cameraAddress_, physical), true, std::move(completion), delayMs);
	} catch (const std::exception &e) {
		if (completion)
			completion(false, QString::fromUtf8(e.what()));
	}
}

void CameraRuntime::storeLogicalPreset(int pageIndex, int slotIndex, Completion completion)
{
	const int physical = presets_.allocatePhysicalPreset(pageIndex, slotIndex);
	if (physical < 1) {
		if (completion)
			completion(false, QStringLiteral("No unused physical preset slots remain on this camera"));
		return;
	}
	try {
		QVector<ViscaTcpTransport::FrameCommand> commands;
		for (const auto &packet : makePresetStoreSequence(cameraAddress_, physical)) {
			commands.append({toByteArray(packet), commands.size() < 2});
		}
		transport_.enqueue(commands, std::move(completion));
	} catch (const std::exception &e) {
		if (completion)
			completion(false, QString::fromUtf8(e.what()));
	}
}

void CameraRuntime::sendPacket(const ViscaPacket &packet, bool waitForCompletion, Completion completion, int delayBeforeMs)
{
	transport_.enqueue({ViscaTcpTransport::FrameCommand{toByteArray(packet), waitForCompletion}}, std::move(completion),
			    delayBeforeMs);
}

void CameraRuntime::writeCameraSettings()
{
	settings_.setValue(QStringLiteral("camera/name"), cameraName_);
	settings_.setValue(QStringLiteral("camera/host"), host_);
	settings_.setValue(QStringLiteral("camera/port"), port_);
	settings_.setValue(QStringLiteral("camera/address"), cameraAddress_);
	settings_.setValue(QStringLiteral("camera/capacity"), capacity_);
	settings_.sync();
}

int CameraRuntime::resolvePhysicalPreset(int pageIndex, int slotIndex) const
{
	if (!presets_.hasUsablePreset(pageIndex, slotIndex))
		return -1;
	return presets_.slot(pageIndex, slotIndex).physicalPreset;
}

CameraRuntime *g_cameraRuntime = nullptr;

CameraRuntime *cameraRuntime()
{
	return g_cameraRuntime;
}

void setCameraRuntime(CameraRuntime *runtime)
{
	g_cameraRuntime = runtime;
}

} // namespace simplecam
