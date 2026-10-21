#include "camera_docks.hpp"

#include "camera_runtime.hpp"
#include "preset_source.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace simplecam {
namespace {

QColor pageColor(int page)
{
	static const QColor colors[] = {
		QColor(176, 176, 176), QColor(192, 176, 176), QColor(160, 176, 192), QColor(176, 192, 176), QColor(192, 192, 112)};
	return colors[page % 5];
}

void setButtonColor(QPushButton *button, const QColor &color)
{
	QPalette palette = button->palette();
	palette.setColor(QPalette::Button, color);
	button->setPalette(palette);
	button->setAutoFillBackground(true);
}

} // namespace

CameraControlDock::CameraControlDock(CameraRuntime &runtime, QWidget *parent) : QWidget(parent), runtime_(runtime)
{
	buildUi();
	refresh();
}

void CameraControlDock::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(6);

	auto *pageHeader = new QHBoxLayout();
	pageButton_ = new QPushButton(QStringLiteral("PG"), this);
	pageButton_->setMinimumWidth(46);
	pageDescription_ = new QLabel(this);
	pageDescription_->setAlignment(Qt::AlignCenter);
	pageDescription_->setMinimumWidth(150);
	settingsButton_ = new QPushButton(QStringLiteral("Settings"), this);
	pageHeader->addWidget(pageButton_);
	pageHeader->addWidget(pageDescription_, 1);
	pageHeader->addWidget(settingsButton_);
	root->addLayout(pageHeader);

	auto *pageTools = new QHBoxLayout();
	pageDescriptionEdit_ = new QLineEdit(this);
	pageDescriptionEdit_->setMaxLength(22);
	pageDescriptionEdit_->setPlaceholderText(QStringLiteral("Preset Group name"));
	addPageButton_ = new QToolButton(this);
	addPageButton_->setText(QStringLiteral("+"));
	addPageButton_->setToolTip(QStringLiteral("Add preset page"));
	removePageButton_ = new QToolButton(this);
	removePageButton_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
	removePageButton_->setToolTip(QStringLiteral("Remove this blank page"));
	pageTools->addWidget(pageDescriptionEdit_, 1);
	pageTools->addWidget(addPageButton_);
	pageTools->addWidget(removePageButton_);
	root->addLayout(pageTools);

	auto *presetGrid = new QGridLayout();
	presetGrid->setContentsMargins(0, 0, 0, 0);
	presetGrid->setHorizontalSpacing(2);
	presetGrid->setVerticalSpacing(2);
	for (int slot = 0; slot < PresetCatalog::SlotsPerPage; ++slot) {
		auto *button = new QPushButton(this);
		button->setMinimumSize(48, 34);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		presetButtons_.append(button);
		presetGrid->addWidget(button, slot / 7, slot % 7);
		QObject::connect(button, &QPushButton::clicked, this, [this, slot]() { handlePresetButton(slot); });
	}
	root->addLayout(presetGrid);

	auto *actions = new QHBoxLayout();
	setButton_ = new QPushButton(QStringLiteral("Set"), this);
	renameButton_ = new QPushButton(QStringLiteral("Rename"), this);
	swapButton_ = new QPushButton(QStringLiteral("Swap"), this);
	clearButton_ = new QPushButton(QStringLiteral("Clear"), this);
	actions->addWidget(setButton_);
	actions->addWidget(swapButton_);
	actions->addWidget(clearButton_);
	actions->addWidget(renameButton_);
	root->addLayout(actions);

	auto *navigation = new QHBoxLayout();
	auto *arrows = new QGridLayout();
	arrows->setSpacing(1);
	auto addArrow = [this, arrows](const QString &text, int row, int column, int pan, int tilt) {
		auto *button = new QPushButton(text, this);
		button->setFixedSize(40, 32);
		arrows->addWidget(button, row, column);
		QObject::connect(button, &QPushButton::pressed, this, [this, pan, tilt]() { sendPanTilt(pan, tilt); });
		QObject::connect(button, &QPushButton::released, this, [this]() { sendStopPanTilt(); });
	};
	addArrow(QStringLiteral("↖"), 0, 0, 1, 1);
	addArrow(QStringLiteral("↑"), 0, 1, 3, 1);
	addArrow(QStringLiteral("↗"), 0, 2, 2, 1);
	addArrow(QStringLiteral("←"), 1, 0, 1, 3);
	auto *homeButton = new QPushButton(QStringLiteral("HOME\n2-click"), this);
	homeButton->setFixedSize(40, 32);
arrows->addWidget(homeButton, 1, 1);
	addArrow(QStringLiteral("→"), 1, 2, 2, 3);
	addArrow(QStringLiteral("↙"), 2, 0, 1, 2);
	addArrow(QStringLiteral("↓"), 2, 1, 3, 2);
	addArrow(QStringLiteral("↘"), 2, 2, 2, 2);
	homeDoubleClickTimer_ = new QTimer(this);
	homeDoubleClickTimer_->setSingleShot(true);
	QPointer<CameraControlDock> self(this);
	QObject::connect(homeButton, &QPushButton::clicked, this, [this, self]() {
		if (homeDoubleClickTimer_->isActive()) {
			homeDoubleClickTimer_->stop();
			runtime_.home([self](bool ok, const QString &message) {
				if (self)
					self->setStatus(message, !ok);
			});
		} else {
			homeDoubleClickTimer_->start(350);
		}
	});
	navigation->addLayout(arrows);

	auto *motionControls = new QVBoxLayout();
	auto *zoomIn = new QPushButton(QStringLiteral("+ Zoom"), this);
	auto *zoomOut = new QPushButton(QStringLiteral("− Zoom"), this);
	QObject::connect(zoomIn, &QPushButton::pressed, this, [this, self]() {
		runtime_.zoom(true, [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	QObject::connect(zoomIn, &QPushButton::released, this, [this, self]() {
		runtime_.stopZoom([self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	QObject::connect(zoomOut, &QPushButton::pressed, this, [this, self]() {
		runtime_.zoom(false, [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	QObject::connect(zoomOut, &QPushButton::released, this, [this, self]() {
		runtime_.stopZoom([self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	motionControls->addWidget(zoomIn);
	motionControls->addWidget(zoomOut);

	auto *speedLabel = new QLabel(QStringLiteral("Speed"), this);
	speedSlider_ = new QSlider(Qt::Horizontal, this);
	speedSlider_->setRange(1, 24);
	speedSlider_->setValue(7);
	speedSlider_->setMinimumWidth(120);
	motionControls->addWidget(speedLabel);
	motionControls->addWidget(speedSlider_);
	navigation->addLayout(motionControls, 1);
	root->addLayout(navigation);

	backlightCheck_ = new QCheckBox(QStringLiteral("Backlight compensation"), this);
	QObject::connect(backlightCheck_, &QCheckBox::toggled, this, [this, self](bool enabled) {
		runtime_.backlight(enabled, [self, enabled](bool ok, const QString &message) {
			if (self)
				self->setStatus(ok ? QStringLiteral("Backlight compensation %1").arg(enabled ? "on" : "off") : message, !ok);
		});
	});
	root->addWidget(backlightCheck_);

	statusLabel_ = new QLabel(QStringLiteral("Camera not connected"), this);
	statusLabel_->setWordWrap(true);
	root->addWidget(statusLabel_);
	root->addStretch(1);

	QObject::connect(pageButton_, &QPushButton::clicked, this, [this]() {
		const int next = (runtime_.activePage() + 1) % runtime_.presets().pageCount();
		runtime_.setActivePage(next);
		refreshPresetSourceNames();
		refresh();
	});
	QObject::connect(settingsButton_, &QPushButton::clicked, this, [this]() {
		settingsMode_ = !settingsMode_;
		pendingSwapPage_ = -1;
		pendingSwapSlot_ = -1;
		setMode(Mode::Recall);
		refresh();
	});
	QObject::connect(pageDescriptionEdit_, &QLineEdit::editingFinished, this, [this]() {
		if (runtime_.presets().setPageDescription(runtime_.activePage(), pageDescriptionEdit_->text()))
			setStatus(QStringLiteral("Page name saved"));
		pageNameDirty_ = false;
		refresh();
	});
	QObject::connect(pageDescriptionEdit_, &QLineEdit::textEdited, this, [this]() { pageNameDirty_ = true; });
	QObject::connect(addPageButton_, &QToolButton::clicked, this, [this]() {
		if (runtime_.presets().addPage()) {
			runtime_.setActivePage(runtime_.presets().pageCount() - 1);
			refreshPresetSourceNames();
			setStatus(QStringLiteral("Added %1").arg(runtime_.presets().page(runtime_.activePage()).description));
		} else {
			setStatus(QStringLiteral("This camera's configured preset capacity has no room for another page"), true);
		}
		refresh();
	});
	QObject::connect(removePageButton_, &QToolButton::clicked, this, [this]() {
		const int page = runtime_.activePage();
		if (runtime_.presets().removePageIfBlank(page)) {
			runtime_.setActivePage(std::min(page, runtime_.presets().pageCount() - 1));
			refreshPresetSourceNames();
			setStatus(QStringLiteral("Removed the blank page"));
		} else {
			setStatus(QStringLiteral("Only a completely blank page above the first five can be removed"), true);
		}
		refresh();
	});
	QObject::connect(setButton_, &QPushButton::clicked, this, [this]() { setMode(mode_ == Mode::Set ? Mode::Recall : Mode::Set); });
	QObject::connect(renameButton_, &QPushButton::clicked, this,
			 [this]() { setMode(mode_ == Mode::Rename ? Mode::Recall : Mode::Rename); });
	QObject::connect(swapButton_, &QPushButton::clicked, this, [this]() {
		setMode(mode_ == Mode::Swap ? Mode::Recall : Mode::Swap);
		pendingSwapPage_ = -1;
		pendingSwapSlot_ = -1;
		refresh();
	});
	QObject::connect(clearButton_, &QPushButton::clicked, this, [this]() { setMode(mode_ == Mode::Clear ? Mode::Recall : Mode::Clear); });
}

void CameraControlDock::refresh()
{
	const int pageIndex = runtime_.activePage();
	const auto &page = runtime_.presets().page(pageIndex);
	pageDescription_->setText(page.description);
	{
		const QSignalBlocker blocker(pageDescriptionEdit_);
		if (!pageNameDirty_)
			pageDescriptionEdit_->setText(page.description);
	}
	pageDescriptionEdit_->setVisible(settingsMode_);
	addPageButton_->setVisible(settingsMode_);
	removePageButton_->setVisible(settingsMode_);
	settingsButton_->setText(settingsMode_ ? QStringLiteral("Controls") : QStringLiteral("Settings"));
	setButton_->setVisible(!settingsMode_);
	swapButton_->setVisible(settingsMode_);
	clearButton_->setVisible(settingsMode_);
	renameButton_->setVisible(true);
	pageButton_->setToolTip(QStringLiteral("Page %1 of %2 — click to cycle").arg(pageIndex + 1).arg(runtime_.presets().pageCount()));

	for (int slotIndex = 0; slotIndex < PresetCatalog::SlotsPerPage; ++slotIndex) {
		auto *button = presetButtons_.at(slotIndex);
		const auto &slot = runtime_.presets().slot(pageIndex, slotIndex);
		const bool usable = runtime_.presets().hasUsablePreset(pageIndex, slotIndex);
		button->setText(usable ? slot.label : QStringLiteral("---"));
		button->setToolTip(usable ? QStringLiteral("Physical camera preset %1").arg(slot.physicalPreset)
					    : QStringLiteral("No physical preset mapped"));
		button->setEnabled(mode_ != Mode::Recall || usable);
		setButtonColor(button, pendingSwapPage_ == pageIndex && pendingSwapSlot_ == slotIndex ? QColor(255, 244, 128)
												 : pageColor(pageIndex));
	}

	refreshPageManagement();
	setButton_->setText(mode_ == Mode::Set ? QStringLiteral("Select preset…") : QStringLiteral("Set"));
	swapButton_->setText(mode_ == Mode::Swap ? QStringLiteral("Select two…") : QStringLiteral("Swap"));
	clearButton_->setText(mode_ == Mode::Clear ? QStringLiteral("Select preset…") : QStringLiteral("Clear"));
	renameButton_->setText(mode_ == Mode::Rename ? QStringLiteral("Select preset…") : QStringLiteral("Rename"));
}

void CameraControlDock::refreshPageManagement()
{
	const int page = runtime_.activePage();
	addPageButton_->setEnabled(runtime_.presets().pageCount() < runtime_.presets().maxPageCount());
	bool allLabelsBlank = true;
	for (int index = 0; index < PresetCatalog::SlotsPerPage; ++index) {
		if (!runtime_.presets().slot(page, index).label.trimmed().isEmpty()) {
			allLabelsBlank = false;
			break;
		}
	}
	removePageButton_->setEnabled(settingsMode_ && page >= PresetCatalog::MinimumPages && allLabelsBlank);
}

void CameraControlDock::setMode(Mode mode)
{
	if (!settingsMode_ && (mode == Mode::Swap || mode == Mode::Clear))
		mode = Mode::Recall;
	if (settingsMode_ && mode == Mode::Set)
		mode = Mode::Recall;
	mode_ = mode;
	refresh();
}

void CameraControlDock::handlePresetButton(int slotIndex)
{
	const int pageIndex = runtime_.activePage();
	const auto slot = runtime_.presets().slot(pageIndex, slotIndex);
	switch (mode_) {
	case Mode::Set: {
		if (slot.physicalPreset > 0) {
			const auto answer = QMessageBox::question(
				this, QStringLiteral("Overwrite camera preset?"),
				QStringLiteral("Store the camera's current position over physical preset %1?").arg(slot.physicalPreset),
				QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
			if (answer != QMessageBox::Yes) {
				setMode(Mode::Recall);
				return;
			}
		}
		setStatus(QStringLiteral("Saving current camera position to preset button %1…").arg(slotIndex + 1));
		QPointer<CameraControlDock> self(this);
		runtime_.storeLogicalPreset(pageIndex, slotIndex, [self, slotIndex](bool ok, const QString &message) {
			if (!self)
				return;
			refreshPresetSourceNames();
			self->setStatus(ok ? QStringLiteral("Preset stored") : message, !ok);
			if (ok)
				self->flashPresetButton(slotIndex);
			self->refresh();
		});
		setMode(Mode::Recall);
		return;
	}
	case Mode::Rename: {
		bool accepted = false;
		const QString current = slot.label;
		const QString value = QInputDialog::getText(this, QStringLiteral("Rename preset"),
						   QStringLiteral("Button name:"), QLineEdit::Normal, current, &accepted);
		if (accepted) {
			runtime_.presets().setLabel(pageIndex, slotIndex, value);
			refreshPresetSourceNames();
			setStatus(QStringLiteral("Preset name saved"));
		}
		setMode(Mode::Recall);
		return;
	}
	case Mode::Swap:
		if (pendingSwapPage_ < 0) {
			pendingSwapPage_ = pageIndex;
			pendingSwapSlot_ = slotIndex;
			setStatus(QStringLiteral("First position selected; use PG to change pages if needed"));
			refresh();
		} else {
			if (runtime_.presets().swapSlots(pendingSwapPage_, pendingSwapSlot_, pageIndex, slotIndex)) {
				refreshPresetSourceNames();
				setStatus(QStringLiteral("Visual preset positions swapped"));
			}
			pendingSwapPage_ = -1;
			pendingSwapSlot_ = -1;
			setMode(Mode::Recall);
		}
		return;
	case Mode::Clear:
		runtime_.presets().clearLabel(pageIndex, slotIndex);
		refreshPresetSourceNames();
		setStatus(QStringLiteral("Button label cleared; camera preset was not erased"));
		setMode(Mode::Recall);
		return;
	case Mode::Recall: {
		if (!runtime_.presets().hasUsablePreset(pageIndex, slotIndex)) {
			setStatus(QStringLiteral("This button has no physical preset assigned"), true);
			return;
		}
		setStatus(QStringLiteral("Moving to %1…").arg(slot.label));
		QPointer<CameraControlDock> self(this);
		runtime_.recallLogicalPreset(slotIndex + 1, 0, [self, slotIndex, label = slot.label](bool ok, const QString &message) {
			if (!self)
				return;
			self->setStatus(ok ? QStringLiteral("Moved to %1").arg(label) : message, !ok);
			if (ok)
				self->flashPresetButton(slotIndex);
		});
		return;
	}
	}
}

void CameraControlDock::flashPresetButton(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= presetButtons_.size())
		return;
	auto *button = presetButtons_.at(slotIndex);
	setButtonColor(button, QColor(255, 244, 128));
	if (!flashTimers_[slotIndex]) {
		flashTimers_[slotIndex] = new QTimer(this);
		flashTimers_[slotIndex]->setSingleShot(true);
		QObject::connect(flashTimers_[slotIndex], &QTimer::timeout, this, [this]() { refresh(); });
	}
	flashTimers_[slotIndex]->start(700);
}

void CameraControlDock::setStatus(const QString &message, bool error)
{
	statusLabel_->setText(message);
	QPalette palette = statusLabel_->palette();
	palette.setColor(QPalette::WindowText, error ? QColor(190, 48, 48) : this->palette().color(QPalette::WindowText));
	statusLabel_->setPalette(palette);
}

void CameraControlDock::sendPanTilt(int panDirection, int tiltDirection)
{
	const int speed = speedSlider_->value();
	QPointer<CameraControlDock> self(this);
	runtime_.panTilt(speed, std::min(speed, 20), static_cast<std::uint8_t>(panDirection), static_cast<std::uint8_t>(tiltDirection),
			 [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
}

void CameraControlDock::sendStopPanTilt()
{
	const int speed = speedSlider_->value();
	QPointer<CameraControlDock> self(this);
	runtime_.stopPanTilt(speed, std::min(speed, 20),
			 [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
}

CamerasDock::CamerasDock(CameraRuntime &runtime, QWidget *parent) : QWidget(parent), runtime_(runtime)
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(10, 10, 10, 10);
	auto *form = new QFormLayout();
	cameraNameEdit_ = new QLineEdit(runtime_.cameraName(), this);
	cameraNameEdit_->setMaxLength(40);
	auto *model = new QLabel(QStringLiteral("AViPAS AV-1280W — VISCA over TCP"), this);
	model->setWordWrap(true);
	hostEdit_ = new QLineEdit(runtime_.host(), this);
	hostEdit_->setPlaceholderText(QStringLiteral("Camera IP address"));
	portSpin_ = new QSpinBox(this);
	portSpin_->setRange(1, 65535);
	portSpin_->setValue(runtime_.port());
	addressSpin_ = new QSpinBox(this);
	addressSpin_->setRange(1, 7);
	addressSpin_->setValue(runtime_.cameraAddress());
	capacitySpin_ = new QSpinBox(this);
	capacitySpin_->setRange(1, 255);
	capacitySpin_->setValue(runtime_.capacity());
	form->addRow(QStringLiteral("Camera name"), cameraNameEdit_);
	form->addRow(QStringLiteral("Model / protocol"), model);
	form->addRow(QStringLiteral("Network IP"), hostEdit_);
	form->addRow(QStringLiteral("TCP port"), portSpin_);
	form->addRow(QStringLiteral("VISCA address"), addressSpin_);
	form->addRow(QStringLiteral("Preset capacity"), capacitySpin_);
	root->addLayout(form);

	auto *testButton = new QPushButton(QStringLiteral("Test TCP connection"), this);
	statusLabel_ = new QLabel(QStringLiteral("Enter the camera address. The test sends no PTZ command."), this);
	statusLabel_->setWordWrap(true);
	root->addWidget(testButton);
	root->addWidget(statusLabel_);
	root->addStretch(1);

	QObject::connect(cameraNameEdit_, &QLineEdit::editingFinished, this,
				 [this]() {
					 runtime_.setCameraName(cameraNameEdit_->text());
					 refreshPresetSourceNames();
				 });
	QObject::connect(hostEdit_, &QLineEdit::editingFinished, this,
			 [this]() { runtime_.setHost(hostEdit_->text()); });
	QObject::connect(portSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
			 [this](int port) { runtime_.setPort(static_cast<quint16>(port)); });
	QObject::connect(addressSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
			 [this](int address) { runtime_.setCameraAddress(address); });
	QObject::connect(capacitySpin_, qOverload<int>(&QSpinBox::valueChanged), this,
			 [this](int capacity) { runtime_.setCapacity(capacity); });
	QPointer<CamerasDock> self(this);
	QObject::connect(testButton, &QPushButton::clicked, this, [this, self]() {
		runtime_.testConnection([self](bool ok, const QString &message) {
			if (!self)
				return;
			self->statusLabel_->setText(message);
			QPalette palette = self->statusLabel_->palette();
			palette.setColor(QPalette::WindowText, ok ? QColor(40, 130, 70) : QColor(190, 48, 48));
			self->statusLabel_->setPalette(palette);
		});
	});
}

} // namespace simplecam
