#include "camera_runtime.hpp"

#include <plugin-support.h>
#include <util/base.h>

#include <QAbstractSocket>
#include <QMetaObject>
#include <QtMath>
#include <QThread>
#include <QTcpSocket>

#include <memory>

#include <algorithm>
#include <array>
#include <utility>

namespace simplecam {
namespace {

constexpr int kViscaTimeoutMs = 2500;
constexpr int kConnectTimeoutMs = 3000;
constexpr int kMaxPresetCapacity = 255;
// Sony VISCA's conventional optical telephoto endpoint is hexadecimal 0x4000.
constexpr int kDefaultZoomMaximum = 0x4000;

QString defaultPageDescription(int pageIndex)
{
	return QStringLiteral("Preset Group %1").arg(pageIndex + 1);
}

QString defaultPresetLabel(int physicalPreset)
{
	return QStringLiteral("Preset %1").arg(physicalPreset);
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

int PresetCatalog::allocatePhysicalPreset(int pageIndex, int slotIndex, int allowedCapacity)
{
	if (!validSlot(pageIndex, slotIndex))
		return -1;
	const int limit = allowedCapacity > 0 ? std::min(capacity_, allowedCapacity) : capacity_;

	auto &selected = pages_[pageIndex].slots[slotIndex];
	if (selected.physicalPreset >= 1 && selected.physicalPreset <= limit)
		return selected.physicalPreset;
	// Existing visual mappings are shared by the cards. Never silently remap a
	// preset that is outside this camera model's capacity, as that would change
	// the other card's meaning for the same button.
	if (selected.physicalPreset > limit)
		return -1;

	QVector<bool> used(limit + 1, false);
	for (const auto &page : pages_) {
		for (const auto &slot : page.slots) {
			if (slot.physicalPreset >= 1 && slot.physicalPreset <= limit)
				used[slot.physicalPreset] = true;
		}
	}

	for (int candidate = 1; candidate <= limit; ++candidate) {
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
	const auto &command = current_.commands.at(current_.commandIndex);
	if (command.onReply)
		command.onReply(frame);
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

CameraRuntime::CameraRuntime(const QString &settingsPath, const QString &defaultName,
			     const QString &defaultProfilePath, QObject *parent)
	: QObject(parent), settings_(settingsPath, QSettings::IniFormat),
	  profilePath_(settings_.value(QStringLiteral("camera/profilePath"), defaultProfilePath).toString()),
	  cameraAddress_(settings_.value(QStringLiteral("camera/address"), 1).toInt()),
	  port_(static_cast<quint16>(settings_.value(QStringLiteral("camera/port"), 1259).toUInt())),
	  cameraName_(settings_.value(QStringLiteral("camera/name"), defaultName).toString()),
	  cameraFeedSourceUuid_(settings_.value(QStringLiteral("camera/feedSourceUuid"), QString()).toString()),
	  host_(settings_.value(QStringLiteral("camera/host"), QString()).toString()),
	capacity_(settings_.value(QStringLiteral("camera/capacity"), 100).toInt()),
	  presets_(settings_, kMaxPresetCapacity), transport_(this)
{
	if (cameraAddress_ < 1 || cameraAddress_ > 7)
		cameraAddress_ = 1;
	if (port_ == 0)
		port_ = 1259;
	capacity_ = std::clamp(capacity_, 0, kMaxPresetCapacity);
	if (profilePath_.isEmpty()) {
		profileLoadError_ = QStringLiteral("No camera profile is selected");
	} else if (!profile_.load(profilePath_, &profileLoadError_)) {
		obs_log(LOG_ERROR, "SimpleCamPTZ could not load camera profile '%s': %s",
			profilePath_.toUtf8().constData(), profileLoadError_.toUtf8().constData());
	} else if (!profile_.supportsRawViscaTcp()) {
		profileLoadError_ = QStringLiteral("Selected profile has no supported raw VISCA over TCP connection");
	} else if (profile_.presetCapacity() < 1) {
		profileLoadError_ = QStringLiteral("Selected profile does not declare a valid preset capacity");
	} else {
		capacity_ = profile_.presetCapacity();
	}
	transport_.setEndpoint(host_, port_, cameraAddress_);
	writeCameraSettings();
}

QString CameraRuntime::cameraName() const
{
	return cameraName_;
}

QString CameraRuntime::cameraFeedSourceUuid() const
{
	return cameraFeedSourceUuid_;
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

bool CameraRuntime::hasUsablePreset(int pageIndex, int slotIndex) const
{
	if (!hasLoadedProfile() || !presets().hasUsablePreset(pageIndex, slotIndex))
		return false;
	const int physical = presets().slot(pageIndex, slotIndex).physicalPreset;
	return physical >= 1 && physical <= capacity_;
}

QString CameraRuntime::profilePath() const
{
	return profilePath_;
}

QString CameraRuntime::profileId() const
{
	return profile_.id();
}

QString CameraRuntime::profileDisplayName() const
{
	return profile_.displayName();
}

QString CameraRuntime::profileLoadError() const
{
	return profileLoadError_;
}

bool CameraRuntime::hasLoadedProfile() const
{
	return profile_.isValid() && profileLoadError_.isEmpty() && profile_.supportsRawViscaTcp();
}

int CameraRuntime::zoomAbsoluteTemplateCount() const
{
	return profile_.command(QStringLiteral("zoomAbsolute")).templates.size();
}

bool CameraRuntime::canSetZoomPosition() const
{
	// The absolute slider remains useful before endpoint calibration: use the
	// established 0x4000 endpoint until a measured profile range is available.
	return hasLoadedProfile() && zoomAbsoluteTemplateCount() > 0;
}

void CameraRuntime::setCameraName(const QString &name)
{
	cameraName_ = name.trimmed().left(40);
	writeCameraSettings();
}

void CameraRuntime::setCameraFeedSourceUuid(const QString &uuid)
{
	const QString clean = uuid.trimmed();
	if (cameraFeedSourceUuid_ == clean)
		return;
	cameraFeedSourceUuid_ = clean;
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

bool CameraRuntime::setProfilePath(const QString &path, QString *error)
{
	CameraProfileDocument candidate;
	QString loadError;
	if (!candidate.load(path, &loadError)) {
		if (error)
			*error = loadError;
		return false;
	}
	if (!candidate.supportsRawViscaTcp()) {
		if (error)
			*error = QStringLiteral("This first runtime supports raw VISCA over TCP profiles only");
		return false;
	}
	const int candidateCapacity = candidate.presetCapacity();
	if (candidateCapacity < 1) {
		if (error)
			*error = QStringLiteral("The selected profile does not declare a valid preset capacity");
		return false;
	}
	profile_ = candidate;
	profilePath_ = candidate.path();
	profileLoadError_.clear();
	capacity_ = candidateCapacity;
	writeCameraSettings();
	return true;
}

void CameraRuntime::setSharedPresetOwner(CameraRuntime &owner)
{
	sharedPresetOwner_ = (&owner == this) ? nullptr : &owner;
}

int CameraRuntime::activePage() const
{
	if (sharedPresetOwner_)
		return sharedPresetOwner_->activePage();
	return activePage_;
}

void CameraRuntime::setActivePage(int pageIndex)
{
	if (sharedPresetOwner_) {
		sharedPresetOwner_->setActivePage(pageIndex);
		return;
	}
	if (presets().pageCount() == 0) {
		activePage_ = 0;
		return;
	}
	activePage_ = std::clamp(pageIndex, 0, presets().pageCount() - 1);
}

PresetCatalog &CameraRuntime::presets()
{
	return sharedPresetOwner_ ? sharedPresetOwner_->presets() : presets_;
}

const PresetCatalog &CameraRuntime::presets() const
{
	return sharedPresetOwner_ ? sharedPresetOwner_->presets() : presets_;
}

void CameraRuntime::testConnection(Completion completion)
{
	transport_.testConnection(std::move(completion));
}

void CameraRuntime::panTilt(int panSpeed, int tiltSpeed, std::uint8_t panDirection, std::uint8_t tiltDirection,
			    Completion completion)
{
	sendProfileCommand(QStringLiteral("movePanTilt"),
			   templateValues(1, panSpeed, tiltSpeed, panDirection, tiltDirection), std::move(completion));
}

void CameraRuntime::stopPanTilt(int panSpeed, int tiltSpeed, Completion completion)
{
	sendProfileCommand(QStringLiteral("stopPanTilt"), templateValues(1, panSpeed, tiltSpeed), std::move(completion));
}

void CameraRuntime::zoom(bool tele, Completion completion)
{
	// SimpleCamQt's working AViPAS continuous zoom packet used speed nibble 0
	// (20/30). Keep that established rate; the generic template default of 8
	// changed the old camera behavior and appeared to stop before digital zoom.
	sendProfileCommand(tele ? QStringLiteral("zoomIn") : QStringLiteral("zoomOut"),
			   templateValues(1, 8, 8, 1, 3, 0),
			   std::move(completion));
}

void CameraRuntime::stopZoom(Completion completion)
{
	QVector<QByteArray> stopPackets;
	QString error;
	const ViscaTemplateValues values = templateValues();
	if (!renderCommandPackets(QStringLiteral("zoomStop"), values, &stopPackets, &error)) {
		if (completion)
			completion(false, error);
		return;
	}
	QVector<ViscaTcpTransport::FrameCommand> commands;
	commands.reserve(stopPackets.size() + 1);
	for (const QByteArray &packet : stopPackets)
		commands.append({packet, viscaCommandWaitsForCompletion(QStringLiteral("zoomStop"))});
	appendZoomInquiry(commands);
	transport_.enqueue(commands, std::move(completion));
}

void CameraRuntime::setZoomPosition(int sliderValue, Completion completion)
{
	if (sliderValue < 0 || sliderValue > 200) {
		if (completion)
			completion(false, QStringLiteral("Zoom slider value must be 0 through 200"));
		return;
	}
	const int calibratedMaximum = profile_.nativeZoomMaximum();
	const int maximum = calibratedMaximum >= 0 ? calibratedMaximum : kDefaultZoomMaximum;
	const int nativePosition = qRound(static_cast<double>(maximum) * sliderValue / 200.0);
	sendProfileCommand(QStringLiteral("zoomAbsolute"), templateValues(1, 8, 8, 1, 3, 8, nativePosition),
			   std::move(completion));
}

void CameraRuntime::home(Completion completion)
{
	sendProfileCommand(QStringLiteral("home"), templateValues(), std::move(completion));
}

void CameraRuntime::backlight(bool enabled, Completion completion)
{
	sendProfileCommand(enabled ? QStringLiteral("backlightOn") : QStringLiteral("backlightOff"), templateValues(),
			   std::move(completion));
}

void CameraRuntime::setManualFocus(bool manual, Completion completion)
{
	sendProfileCommand(manual ? QStringLiteral("focusManual") : QStringLiteral("focusAuto"), templateValues(),
			   std::move(completion));
}

void CameraRuntime::focusDrive(bool far, Completion completion)
{
	sendProfileCommand(far ? QStringLiteral("focusFar") : QStringLiteral("focusNear"), templateValues(),
			   std::move(completion));
}

void CameraRuntime::stopFocus(Completion completion)
{
	sendProfileCommand(QStringLiteral("focusStop"), templateValues(), std::move(completion));
}

void CameraRuntime::exposureCompensation(bool enabled, Completion completion)
{
	sendProfileCommand(enabled ? QStringLiteral("exposureCompOn") : QStringLiteral("exposureCompOff"),
			   templateValues(), std::move(completion));
}

void CameraRuntime::setCameraPower(bool on, Completion completion)
{
	sendProfileCommand(on ? QStringLiteral("powerOn") : QStringLiteral("powerOff"), templateValues(),
			   std::move(completion));
}

void CameraRuntime::recallLogicalPreset(int logicalButton, int delayMs, Completion completion, PresetRecallOrigin origin)
{
	if (logicalButton < 1 || logicalButton > PresetCatalog::SlotsPerPage) {
		if (completion)
			completion(false, QStringLiteral("Preset button must be 1 through 14"));
		return;
	}

	const int recallPage = activePage_;
	const int physical = resolvePhysicalPreset(recallPage, logicalButton - 1);
	if (physical < 1) {
		if (completion)
			completion(false, QStringLiteral("This button has no usable preset mapping for the selected camera profile"));
		return;
	}
	// The flash confirms use of the mapped preset button. Camera movement and
	// transport success are reported separately by the completion callback.
	if (presetRecallObserver_)
		presetRecallObserver_(logicalButton, origin);
	sendPresetRecallAndZoomInquiry(physical, delayMs,
		[ this, logicalButton, recallPage, origin, completion = std::move(completion) ](bool success,
																		 const QString &message) mutable {
			if (presetRecallResultObserver_)
				presetRecallResultObserver_(logicalButton, recallPage, origin, success);
			if (completion)
				completion(success, message);
		});
}

void CameraRuntime::setPresetRecallObserver(PresetRecallObserver observer)
{
	presetRecallObserver_ = std::move(observer);
}

void CameraRuntime::setPresetRecallResultObserver(PresetRecallResultObserver observer)
{
	presetRecallResultObserver_ = std::move(observer);
}

void CameraRuntime::setZoomPositionObserver(ZoomPositionObserver observer)
{
	zoomPositionObserver_ = std::move(observer);
}

void CameraRuntime::storeLogicalPreset(int pageIndex, int slotIndex, Completion completion)
{
	const int physical = presets().allocatePhysicalPreset(pageIndex, slotIndex, capacity_);
	if (physical < 1) {
		if (completion)
			completion(false, QStringLiteral("This preset mapping is outside the selected profile capacity, or no unused camera preset slots remain"));
		return;
	}
	sendProfileCommand(QStringLiteral("presetStore"), templateValues(physical), std::move(completion));
}

ViscaTemplateValues CameraRuntime::templateValues(int physicalPreset, int panSpeed, int tiltSpeed,
						  std::uint8_t panDirection, std::uint8_t tiltDirection,
						  int zoomSpeed, int zoomPosition) const
{
	ViscaTemplateValues values;
	values.cameraAddress = cameraAddress_;
	values.panSpeed = panSpeed;
	values.tiltSpeed = tiltSpeed;
	values.panDirection = panDirection;
	values.tiltDirection = tiltDirection;
	values.zoomSpeed = zoomSpeed;
	values.zoomPosition = zoomPosition;
	values.physicalPreset = physicalPreset;
	return values;
}

bool CameraRuntime::renderCommandPackets(const QString &commandId, const ViscaTemplateValues &values,
						 QVector<QByteArray> *packets, QString *error) const
{
	if (!hasLoadedProfile()) {
		if (error)
			*error = profileLoadError_.isEmpty() ? QStringLiteral("No supported camera profile is loaded") : profileLoadError_;
		return false;
	}
	const CameraProfileCommand binding = profile_.command(commandId);
	if (binding.templates.isEmpty()) {
		if (error)
			*error = QStringLiteral("Profile '%1' does not define command '%2'").arg(profile_.displayName(), commandId);
		return false;
	}
	return renderViscaTemplates(binding.templates, values, packets, error);
}

void CameraRuntime::sendProfileCommand(const QString &commandId, const ViscaTemplateValues &values,
					       Completion completion, int delayBeforeMs)
{
	QVector<QByteArray> packets;
	QString error;
	if (!renderCommandPackets(commandId, values, &packets, &error)) {
		if (completion)
			completion(false, error);
		return;
	}
	QVector<ViscaTcpTransport::FrameCommand> commands;
	commands.reserve(packets.size());
	const bool waitForCompletion = viscaCommandWaitsForCompletion(commandId);
	for (const QByteArray &packet : packets)
		commands.append({packet, waitForCompletion});
	transport_.enqueue(commands, std::move(completion), delayBeforeMs);
}

void CameraRuntime::appendZoomInquiry(QVector<ViscaTcpTransport::FrameCommand> &commands)
{
	QVector<QByteArray> inquiryPackets;
	if (!renderCommandPackets(QStringLiteral("zoomInquiry"), templateValues(), &inquiryPackets, nullptr))
		return;
	for (const QByteArray &packet : inquiryPackets) {
		ViscaTcpTransport::FrameCommand inquiry{packet, true};
		inquiry.onReply = [this](const QByteArray &reply) { handleZoomInquiryReply(reply); };
		commands.append(std::move(inquiry));
	}
}

void CameraRuntime::sendPresetRecallAndZoomInquiry(int physicalPreset, int delayMs, Completion completion)
{
	const ViscaTemplateValues values = templateValues(physicalPreset);
	QVector<QByteArray> recallPackets;
	QString error;
	if (!renderCommandPackets(QStringLiteral("presetRecall"), values, &recallPackets, &error)) {
		if (completion)
			completion(false, error);
		return;
	}
	QVector<ViscaTcpTransport::FrameCommand> commands;
	for (const QByteArray &packet : recallPackets)
		commands.append({packet, viscaCommandWaitsForCompletion(QStringLiteral("presetRecall"))});

	appendZoomInquiry(commands);
	transport_.enqueue(commands, std::move(completion), delayMs);
}

void CameraRuntime::handleZoomInquiryReply(const QByteArray &reply)
{
	if (reply.size() < 7 || (static_cast<quint8>(reply.at(1)) & 0xF0) != 0x50 ||
	    static_cast<quint8>(reply.at(reply.size() - 1)) != 0xFF)
		return;
	int nativePosition = 0;
	for (int index = 2; index <= 5; ++index) {
		const quint8 value = static_cast<quint8>(reply.at(index));
		if ((value & 0xF0) != 0)
			return;
		nativePosition = (nativePosition << 4) | (value & 0x0F);
	}
	const int calibratedMaximum = profile_.nativeZoomMaximum();
	const int maximum = calibratedMaximum > 0 ? calibratedMaximum : kDefaultZoomMaximum;
	const int sliderValue = std::clamp(qRound(static_cast<double>(nativePosition) * 200.0 / maximum), 0, 200);
	if (zoomPositionObserver_)
		zoomPositionObserver_(sliderValue);
}

void CameraRuntime::writeCameraSettings()
{
	settings_.setValue(QStringLiteral("camera/name"), cameraName_);
	settings_.setValue(QStringLiteral("camera/feedSourceUuid"), cameraFeedSourceUuid_);
	settings_.setValue(QStringLiteral("camera/host"), host_);
	settings_.setValue(QStringLiteral("camera/port"), port_);
	settings_.setValue(QStringLiteral("camera/address"), cameraAddress_);
	settings_.setValue(QStringLiteral("camera/capacity"), capacity_);
	settings_.setValue(QStringLiteral("camera/profilePath"), profilePath_);
	settings_.sync();
}

int CameraRuntime::resolvePhysicalPreset(int pageIndex, int slotIndex) const
{
	if (!hasUsablePreset(pageIndex, slotIndex))
		return -1;
	return presets().slot(pageIndex, slotIndex).physicalPreset;
}

CameraRuntimeArray g_cameraRuntimes{};

CameraRuntime *cameraRuntime(int cameraIndex)
{
	if (cameraIndex < 0 || cameraIndex >= static_cast<int>(g_cameraRuntimes.size()))
		return nullptr;
	return g_cameraRuntimes[static_cast<size_t>(cameraIndex)];
}

void setCameraRuntime(int cameraIndex, CameraRuntime *runtime)
{
	if (cameraIndex < 0 || cameraIndex >= static_cast<int>(g_cameraRuntimes.size()))
		return;
	g_cameraRuntimes[static_cast<size_t>(cameraIndex)] = runtime;
}

} // namespace simplecam
