#include "camera_docks.hpp"

#include "camera_runtime.hpp"
#include "preset_source.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QComboBox>
#include <QDockWidget>
#include <QCursor>
#include <QElapsedTimer>
#include <QEvent>
#include <QEasingCurve>
#include <QFormLayout>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QLayoutItem>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPushButton>
#include <QPointer>
#include <QResizeEvent>
#include <QRectF>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>

namespace simplecam {
namespace {

QColor pageColor(int page)
{
	static const QColor colors[] = {
		QColor("#C4C8CA"), QColor("#CBB8BA"), QColor("#B4CCDA"), QColor("#B4CFB6"), QColor("#D0B9A4")};
	return colors[page % 5];
	}

QColor groupAccent(int page)
{
	static const QColor colors[] = {QColor("#777F83"), QColor("#8B1722"), QColor("#0077A6"),
					 QColor("#16752B"), QColor("#965A2C")};
	return colors[page % 5];
}

QString buttonStyle(const QColor &background, const QString &foreground = QStringLiteral("black"))
{
	return QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid #E8E8E8; "
			      "font-weight:bold; padding:0px; } QPushButton:pressed { border:2px solid white; }")
		.arg(background.name(), foreground);
}

QString toggleButtonStyle(const QColor &background, bool checked)
{
	if (checked)
		return QStringLiteral("QPushButton { background:#168A3A; color:white; border:3px solid #FFF76A; "
				      "font-weight:bold; padding:0px; }");
	return buttonStyle(background);
}

void logLayoutWidgetMetrics(const char *phase, int transition, int resizeRequest, const char *name, const QWidget *widget)
{
	if (!widget)
		return;
	const QRect geometry = widget->geometry();
	const QSize hint = widget->sizeHint();
	const QSize minimum = widget->minimumSize();
	const QSize maximum = widget->maximumSize();
	obs_log(LOG_INFO,
		"[SimpleCamDirect LayoutDiag] phase=%s transition=%d resize=%d widget=%s geom=%dx%d@%d,%d "
		"hint=%dx%d min=%dx%d max=%dx%d visible=%d",
		phase, transition, resizeRequest, name, geometry.width(), geometry.height(), geometry.x(), geometry.y(), hint.width(), hint.height(),
		minimum.width(), minimum.height(), maximum.width(), maximum.height(), widget->isVisible() ? 1 : 0);
}

void logLayoutMetrics(const char *phase, int transition, int resizeRequest, const char *name, const QLayout *layout)
{
	if (!layout)
		return;
	const QRect geometry = layout->geometry();
	const QSize hint = layout->sizeHint();
	const QSize minimum = layout->minimumSize();
	const QSize maximum = layout->maximumSize();
	obs_log(LOG_INFO,
		"[SimpleCamDirect LayoutDiag] phase=%s transition=%d resize=%d layout=%s geom=%dx%d@%d,%d "
		"hint=%dx%d min=%dx%d max=%dx%d",
		phase, transition, resizeRequest, name, geometry.width(), geometry.height(), geometry.x(), geometry.y(), hint.width(), hint.height(),
		minimum.width(), minimum.height(), maximum.width(), maximum.height());
}

void logPresetFadeWidgetState(const char *phase, unsigned long long sequence, qint64 elapsedMs, const char *path,
			      const QWidget *widget)
{
	if (!widget)
		return;
	const QRect geometry = widget->geometry();
	const QSize hint = widget->sizeHint();
	const QSize minimumHint = widget->minimumSizeHint();
	const QSize minimum = widget->minimumSize();
	const QSize maximum = widget->maximumSize();
	const QMargins margins = widget->contentsMargins();
	const QSizePolicy policy = widget->sizePolicy();
	const QFont font = widget->font();
	const QString objectName = widget->objectName();
	const QByteArray objectNameBytes = objectName.toUtf8();
	const QByteArray className = QByteArray(widget->metaObject()->className());
	int textLength = -1;
	if (const auto *label = qobject_cast<const QLabel *>(widget))
		textLength = label->text().size();
	else if (const auto *button = qobject_cast<const QPushButton *>(widget))
		textLength = button->text().size();
	else if (const auto *lineEdit = qobject_cast<const QLineEdit *>(widget))
		textLength = lineEdit->text().size();

	obs_log(LOG_INFO,
		"[SimpleCamDirect PresetFadeWidget] id=%llu phase=%s elapsed_ms=%lld path=%s class=%s name=\"%s\" "
		"geom=(%d,%d %dx%d) hint=%dx%d minHint=%dx%d min=%dx%d max=%dx%d "
		"policy=(%d,%d stretch=%d,%d hfw=%d) margins=(%d,%d,%d,%d) "
		"font=(pt=%.1f px=%d bold=%d) text_len=%d visible=%d hidden=%d",
		sequence, phase, static_cast<long long>(elapsedMs), path, className.constData(), objectNameBytes.constData(), geometry.x(),
		geometry.y(), geometry.width(), geometry.height(), hint.width(), hint.height(), minimumHint.width(), minimumHint.height(),
		minimum.width(), minimum.height(), maximum.width(), maximum.height(), static_cast<int>(policy.horizontalPolicy()),
		static_cast<int>(policy.verticalPolicy()), policy.horizontalStretch(), policy.verticalStretch(), policy.hasHeightForWidth() ? 1 : 0,
		margins.left(), margins.top(), margins.right(), margins.bottom(), font.pointSizeF(), font.pixelSize(), font.bold() ? 1 : 0,
		textLength, widget->isVisible() ? 1 : 0, widget->isHidden() ? 1 : 0);
}

void logPresetFadeLayoutTree(const char *phase, unsigned long long sequence, qint64 elapsedMs, const QLayout *layout,
			     const QString &path)
{
	if (!layout)
		return;
	const QRect geometry = layout->geometry();
	const QSize hint = layout->sizeHint();
	const QSize minimum = layout->minimumSize();
	const QSize maximum = layout->maximumSize();
	const QMargins margins = layout->contentsMargins();
	const QByteArray pathBytes = path.toUtf8();
	obs_log(LOG_INFO,
		"[SimpleCamDirect PresetFadeLayout] id=%llu phase=%s elapsed_ms=%lld path=%s "
		"geom=(%d,%d %dx%d) hint=%dx%d min=%dx%d max=%dx%d margins=(%d,%d,%d,%d) "
		"spacing=%d enabled=%d constraint=%d count=%d",
		sequence, phase, static_cast<long long>(elapsedMs), pathBytes.constData(), geometry.x(), geometry.y(), geometry.width(),
		geometry.height(), hint.width(), hint.height(), minimum.width(), minimum.height(), maximum.width(), maximum.height(), margins.left(),
		margins.top(), margins.right(), margins.bottom(), layout->spacing(), layout->isEnabled() ? 1 : 0,
		static_cast<int>(layout->sizeConstraint()), layout->count());

	for (int index = 0; index < layout->count(); ++index) {
		QLayoutItem *item = layout->itemAt(index);
		if (!item)
			continue;
		const QString itemPath = QStringLiteral("%1/%2").arg(path).arg(index);
		if (QWidget *widget = item->widget()) {
			const QByteArray itemPathBytes = itemPath.toUtf8();
			logPresetFadeWidgetState(phase, sequence, elapsedMs, itemPathBytes.constData(), widget);
			if (widget->layout())
				logPresetFadeLayoutTree(phase, sequence, elapsedMs, widget->layout(), itemPath + QStringLiteral("/layout"));
		} else if (QLayout *childLayout = item->layout()) {
			logPresetFadeLayoutTree(phase, sequence, elapsedMs, childLayout, itemPath);
		} else {
			const QSize spacerHint = item->sizeHint();
			const QByteArray itemPathBytes = itemPath.toUtf8();
			obs_log(LOG_INFO,
				"[SimpleCamDirect PresetFadeSpacer] id=%llu phase=%s elapsed_ms=%lld path=%s hint=%dx%d",
				sequence, phase, static_cast<long long>(elapsedMs), itemPathBytes.constData(), spacerHint.width(), spacerHint.height());
		}
	}
}

void logPresetFadeLayoutSnapshot(const char *phase, unsigned long long sequence, const QElapsedTimer &clock, QWidget *card,
				 QWidget *dock = nullptr)
{
	if (!card)
		return;
	const qint64 elapsedMs = clock.elapsed();
	logPresetFadeWidgetState(phase, sequence, elapsedMs, "card", card);
	logPresetFadeLayoutTree(phase, sequence, elapsedMs, card->layout(), QStringLiteral("card"));
	const auto descendants = card->findChildren<QWidget *>();
	for (int index = 0; index < descendants.size(); ++index) {
		const QString path = QStringLiteral("descendant[%1]").arg(index);
		const QByteArray pathBytes = path.toUtf8();
		logPresetFadeWidgetState(phase, sequence, elapsedMs, pathBytes.constData(), descendants.at(index));
	}
	if (dock && dock != card) {
		logPresetFadeWidgetState(phase, sequence, elapsedMs, "dock", dock);
		logPresetFadeLayoutTree(phase, sequence, elapsedMs, dock->layout(), QStringLiteral("dock"));
	}
}

QIcon directionIcon(const QString &resource)
{
	return QIcon(QStringLiteral(":/simplecamdirect/%1.png").arg(resource));
}

QIcon homeIcon()
{
	return QIcon(QStringLiteral(":/simplecamdirect/BlackHomeButton.png"));
}

class CameraIndicator final : public QPushButton {
public:
	explicit CameraIndicator(int cameraNumber, QWidget *parent = nullptr) : QPushButton(parent), cameraNumber_(cameraNumber)
	{
		setFixedSize(40, 40);
		setCheckable(true);
		setChecked(true);
		setStyleSheet(QStringLiteral("QPushButton { background:transparent; border:0; padding:0px; }"));
		setCursor(Qt::PointingHandCursor);
		setToolTip(QStringLiteral("Click to change which camera this card controls"));
	}
	void setCameraNumber(int number)
	{
		cameraNumber_ = number;
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(QPen(QColor("#FFFFFF"), 1));
		painter.setBrush(isChecked() ? QColor("#83C9EE") : QColor("#48B43D"));
		// Keep the 1 px outline fully inside the paint device. A 1 px inset
		// places the lower/right half of the pen on the widget boundary, where
		// Qt clips it and makes the rounded bottom corners look squared off.
		// OBS clips a couple of pixels at the selector row's lower paint edge.
		// Keep the complete rounded square inside the existing 40x40 slot.
		const QRectF indicatorRect(3.5, 1.0, 33.0, 33.0);
		painter.drawRoundedRect(indicatorRect, 5.0, 5.0);
		const QRectF circle(9, 6.5, 22, 22);
		painter.setPen(Qt::NoPen);
		painter.setBrush(isChecked() ? QColor("#050505") : QColor("#D4D7D8"));
		painter.drawEllipse(circle);
		painter.setPen(isChecked() ? QColor("#86C9EE") : QColor("#168A3A"));
		QFont font = painter.font();
		font.setBold(true);
		font.setPointSize(11);
		painter.setFont(font);
		painter.drawText(circle, Qt::AlignCenter, QString::number(cameraNumber_));
	}

private:
	int cameraNumber_ = 1;
};

class RotatedZoomLabel final : public QWidget {
public:
	explicit RotatedZoomLabel(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(16, 56); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::TextAntialiasing);
		painter.translate(0, height());
		painter.rotate(-90);
		painter.setPen(Qt::white);
		QFont font = painter.font();
		font.setBold(true);
		font.setPointSize(8);
		painter.setFont(font);
		painter.drawText(QRect(0, 0, height(), width()), Qt::AlignCenter, QStringLiteral("Zoom"));
	}
};

class PresetFlashButton final : public QPushButton {
public:
	explicit PresetFlashButton(QWidget *parent = nullptr) : QPushButton(parent) {}

	void setFlashColor(const QColor &color)
	{
		flashColor_ = color;
		update();
	}

	void clearFlashColor()
	{
		if (!flashColor_.isValid())
			return;
		flashColor_ = QColor();
		update();
	}

	QRect textBounds() const
	{
		QStyleOptionButton option;
		initStyleOption(&option);
		return style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
	}

protected:
	void paintEvent(QPaintEvent *event) override
	{
		// Keep Qt's normal button rendering active throughout the fade. Only
		// overlay color changes; text, border, and pressed-state geometry stay
		// exactly as they are outside the animation.
		QPushButton::paintEvent(event);
		if (!flashColor_.isValid())
			return;

		QStyleOptionButton option;
		initStyleOption(&option);
		QPainter painter(this);
		painter.setOpacity(0.88);
		painter.fillRect(rect().adjusted(1, 1, -1, -1), flashColor_);
		painter.setOpacity(1.0);

		// The wash covers the original glyph, so redraw it using Qt's normal
		// content rectangle and palette to keep its position and contrast.
		const QRect textRect = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
		style()->drawItemText(&painter, textRect, Qt::AlignCenter, option.palette, isEnabled(), text(), QPalette::ButtonText);
	}

private:
	QColor flashColor_;
};


const char *presetFadeEventName(QEvent::Type type)
{
	switch (type) {
	case QEvent::Move: return "Move";
	case QEvent::Resize: return "Resize";
	case QEvent::LayoutRequest: return "LayoutRequest";
	case QEvent::StyleChange: return "StyleChange";
	case QEvent::Polish: return "Polish";
	case QEvent::PolishRequest: return "PolishRequest";
	case QEvent::ContentsRectChange: return "ContentsRectChange";
	case QEvent::FontChange: return "FontChange";
	case QEvent::DynamicPropertyChange: return "DynamicPropertyChange";
	case QEvent::ParentChange: return "ParentChange";
	case QEvent::LayoutDirectionChange: return "LayoutDirectionChange";
	case QEvent::Show: return "Show";
	case QEvent::Hide: return "Hide";
	default: return nullptr;
	}
}

class PresetFadeEventProbe final : public QObject {
public:
	PresetFadeEventProbe(QWidget *button, QWidget *card, QWidget *dock, unsigned long long sequence,
			     QString origin, std::shared_ptr<QElapsedTimer> clock, QObject *parent)
		: QObject(parent), button_(button), card_(card), dock_(dock), sequence_(sequence), origin_(std::move(origin)), clock_(std::move(clock))
	{
		if (card_)
			card_->installEventFilter(this);
		if (card_) {
			const auto descendants = card_->findChildren<QWidget *>();
			for (QWidget *child : descendants)
				child->installEventFilter(this);
		}
		if (dock_ && dock_ != card_)
			dock_->installEventFilter(this);
	}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		const char *name = presetFadeEventName(event->type());
		if (!name)
			return QObject::eventFilter(watched, event);
		QWidget *widget = qobject_cast<QWidget *>(watched);
		if (!widget)
			return QObject::eventFilter(watched, event);
		const char *target = widget == button_ ? "button" : widget == card_ ? "card" : widget == dock_ ? "dock" : "child";
		QString details;
		if (event->type() == QEvent::Move) {
			const auto *move = static_cast<QMoveEvent *>(event);
			details = QStringLiteral(" old=(%1,%2) new=(%3,%4)")
				   .arg(move->oldPos().x()).arg(move->oldPos().y()).arg(move->pos().x()).arg(move->pos().y());
		} else if (event->type() == QEvent::Resize) {
			const auto *resize = static_cast<QResizeEvent *>(event);
			details = QStringLiteral(" old=%1x%2 new=%3x%4")
				   .arg(resize->oldSize().width()).arg(resize->oldSize().height())
				   .arg(resize->size().width()).arg(resize->size().height());
		}
		const QRect rect = widget->geometry();
		const QPoint global = widget->mapToGlobal(QPoint(0, 0));
		const QByteArray extra = details.toLatin1();
		const QByteArray source = origin_.toUtf8();
		obs_log(LOG_INFO,
			"[SimpleCamDirect PresetFadeEvent] id=%llu origin=%s elapsed_ms=%lld target=%s event=%s "
			"geom=(%d,%d %dx%d) global=(%d,%d) contents=(%d,%d %dx%d)%s",
			sequence_, source.constData(), static_cast<long long>(clock_->elapsed()), target, name,
			rect.x(), rect.y(), rect.width(), rect.height(), global.x(), global.y(), widget->contentsRect().x(),
			widget->contentsRect().y(), widget->contentsRect().width(), widget->contentsRect().height(), extra.constData());
		if (event->type() == QEvent::LayoutRequest && card_)
			logPresetFadeLayoutSnapshot("layout-request", sequence_, *clock_, card_, dock_);
		return QObject::eventFilter(watched, event);
	}

private:
	QPointer<QWidget> button_;
	QPointer<QWidget> card_;
	QPointer<QWidget> dock_;
	unsigned long long sequence_ = 0;
	QString origin_;
	std::shared_ptr<QElapsedTimer> clock_;
};

class ProtectedPowerButton final : public QPushButton {
public:
	explicit ProtectedPowerButton(QWidget *parent = nullptr) : QPushButton(QStringLiteral("Off"), parent) {}
	std::function<void()> onUnauthorizedOff;

protected:
	void nextCheckState() override
	{
		if (isChecked() && !QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
			if (onUnauthorizedOff)
				onUnauthorizedOff();
			return;
		}
		QPushButton::nextCheckState();
	}
};

void setButtonColor(QPushButton *button, const QColor &color)
{
	button->setStyleSheet(QStringLiteral("QPushButton { background:%1; color:black; border:1px solid #505050; "
					     "font-weight:bold; margin:0px; padding:0px; border-radius:0px; } "
					     "QPushButton:pressed { border:1px solid white; }")
				      .arg(color.name()));
}

} // namespace

CameraControlCard::CameraControlCard(CameraRuntime &runtime, int cameraNumber, bool embedded, QWidget *parent)
	: QWidget(parent), runtime_(&runtime), embedded_(embedded), cameraNumber_(cameraNumber)
{
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	buildUi();
	refresh();
	if (layout())
		layout()->activate();
}

void CameraControlCard::setRuntime(CameraRuntime &runtime, int cameraNumber)
{
	runtime_ = &runtime;
	cameraNumber_ = cameraNumber;
	for (int index = 0; index < 2; ++index) {
		if (auto *indicator = dynamic_cast<CameraIndicator *>(cameraSelectButtons_[index])) {
			indicator->setChecked(index + 1 == cameraNumber_);
			indicator->update();
		}
	}
	setStatus(QStringLiteral("Showing %1").arg(runtime_->cameraName()));
	refresh();
}

void CameraControlCard::setCameraSelectionHandler(std::function<void(int)> handler)
{
	cameraSelectionHandler_ = std::move(handler);
}

void CameraControlCard::buildUi()
{
	auto *root = new QVBoxLayout(this);
	// The dock toolbar already supplies the top separation. Keep the card's
	// camera selector directly beneath it instead of stacking another 4 px.
	root->setContentsMargins(6, 0, 6, 4);
	root->setSpacing(1);

	cameraHeaderWidget_ = new QWidget(this);
	auto *cameraHeaderLayout = new QVBoxLayout(cameraHeaderWidget_);
	cameraHeaderLayout->setContentsMargins(0, 0, 0, 0);
	cameraHeaderLayout->setSpacing(0);

	auto *pageHeader = new QHBoxLayout();
	pageHeader->setContentsMargins(0, 0, 0, 0);
	pageButton_ = new QPushButton(this);
	pageButton_->setFixedSize(112, 22);
	pageButton_->setStyleSheet(QStringLiteral("QPushButton { background:black; color:yellow; border:1px solid #E8E8E8; font-weight:bold; }"));
	pageDescription_ = new QLabel(this);
	pageDescription_->setAlignment(Qt::AlignCenter);
	pageDescription_->setMinimumWidth(120);
	settingsButton_ = new QPushButton(QStringLiteral("Settings"), this);
	settingsButton_->setFixedSize(55, 22);
	settingsButton_->setStyleSheet(buttonStyle(QColor("#3C96C9")));
	settingsButton_->setVisible(!embedded_);
	pageHeader->addStretch(1);
	pageHeader->addWidget(settingsButton_);
	if (!embedded_)
		root->addLayout(pageHeader);

	auto *cameraSelectorRow = new QHBoxLayout();
	cameraSelectorRow->setContentsMargins(0, 0, 0, 0);
	cameraSelectorRow->addStretch(1);
	for (int cameraIndex = 0; cameraIndex < 2; ++cameraIndex) {
		cameraSelectButtons_[cameraIndex] = new CameraIndicator(cameraIndex + 1, this);
		cameraSelectButtons_[cameraIndex]->setChecked(cameraIndex + 1 == cameraNumber_);
		cameraSelectorRow->addWidget(cameraSelectButtons_[cameraIndex]);
		QObject::connect(cameraSelectButtons_[cameraIndex], &QPushButton::clicked, this, [this, cameraIndex]() {
			cameraSelectButtons_[cameraIndex]->setChecked(true);
			if (cameraSelectionHandler_)
				cameraSelectionHandler_(cameraIndex);
		});
		if (cameraIndex == 0)
			cameraSelectorRow->addSpacing(6);
	}
	cameraSelectorRow->addStretch(1);
	cameraSelectorWidget_ = new QWidget(cameraHeaderWidget_);
	cameraSelectorWidget_->setLayout(cameraSelectorRow);
	cameraSelectorWidget_->setFixedHeight(40);
	for (auto *cameraSelectButton : cameraSelectButtons_)
		cameraSelectButton->setParent(cameraSelectorWidget_);
	cameraHeaderLayout->addWidget(cameraSelectorWidget_);

	cameraTitle_ = new QLabel(cameraHeaderWidget_);
	cameraTitle_->setAlignment(Qt::AlignCenter);
	cameraTitle_->setFixedHeight(28);
	cameraTitle_->setStyleSheet(QStringLiteral("color:#46C828; font-family:'Arial Black'; font-size:20px; font-weight:900;"));
	cameraHeaderLayout->addWidget(cameraTitle_);
	root->addWidget(cameraHeaderWidget_);

	groupToolsWidget_ = new QWidget(this);
	auto *groupToolsLayout = new QVBoxLayout(groupToolsWidget_);
	groupToolsLayout->setContentsMargins(0, 0, 0, 0);
	groupToolsLayout->setSpacing(1);
	settingsInstructions_ = new QLabel(QStringLiteral("Use ▲▼ to select a Preset Group.\nUse ADD/DEL to manage groups.\n"
								  "All presets in a group must be cleared to delete the group.\n"
								  "Swap selects two presets to exchange their position in the grid."), groupToolsWidget_);
	settingsInstructions_->setAlignment(Qt::AlignCenter);
	settingsInstructions_->setWordWrap(true);
	settingsInstructions_->setStyleSheet(QStringLiteral("QLabel { background:#050505; color:white; padding:2px; font-size:12px; }"));
	groupToolsLayout->addWidget(settingsInstructions_);

	auto *pageToolsWidget = new QWidget(groupToolsWidget_);
	auto *pageTools = new QHBoxLayout(pageToolsWidget);
	pageTools->setContentsMargins(0, 0, 0, 0);
	pageNumberLabel_ = new QLabel(pageToolsWidget);
	pageNumberLabel_->setFixedWidth(14);
	pageNumberLabel_->setAlignment(Qt::AlignCenter);
	pageDescriptionEdit_ = new QLineEdit(pageToolsWidget);
	pageDescriptionEdit_->setMaxLength(22);
	pageDescriptionEdit_->setPlaceholderText(QStringLiteral("Preset Group name"));
	pageButton_->setParent(pageToolsWidget);
	pageButton_->setText(QStringLiteral("▲▼"));
	pageButton_->setFixedSize(24, 34);
	pageButton_->setToolTip(QStringLiteral("Select preset group"));
	pageButton_->setStyleSheet(QStringLiteral("QPushButton { background:#D0D0D0; color:#111; border:1px solid #EEE; font-weight:bold; padding:0px; }"));
	addPageButton_ = new QToolButton(pageToolsWidget);
	addPageButton_->setText(QStringLiteral("ADD"));
	addPageButton_->setFixedSize(36, 17);
	addPageButton_->setToolTip(QStringLiteral("Add preset page"));
	removePageButton_ = new QToolButton(pageToolsWidget);
	removePageButton_->setText(QStringLiteral("DEL"));
	removePageButton_->setFixedSize(36, 17);
	removePageButton_->setToolTip(QStringLiteral("Remove this blank page"));
	pageTools->addWidget(pageNumberLabel_);
	pageTools->addWidget(pageDescriptionEdit_, 1);
	pageTools->addWidget(pageButton_);
	auto *pageManagementButtons = new QWidget(pageToolsWidget);
	addPageButton_->setParent(pageManagementButtons);
	removePageButton_->setParent(pageManagementButtons);
	auto *pageManagementColumn = new QVBoxLayout(pageManagementButtons);
	pageManagementColumn->setContentsMargins(0, 0, 0, 0);
	pageManagementColumn->setSpacing(1);
	pageManagementColumn->addWidget(addPageButton_);
	pageManagementColumn->addWidget(removePageButton_);
	pageTools->addWidget(pageManagementButtons);
	groupToolsLayout->addWidget(pageToolsWidget);
	root->addWidget(groupToolsWidget_);

	auto *presetGrid = new QGridLayout();
	presetGrid->setContentsMargins(0, 0, 0, 0);
	presetGrid->setHorizontalSpacing(0);
	presetGrid->setVerticalSpacing(0);
	for (int slot = 0; slot < PresetCatalog::SlotsPerPage; ++slot) {
		auto *button = new PresetFlashButton(this);
		button->setMinimumSize(90, 34);
		button->setFixedHeight(34);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		presetButtons_.append(button);
		// Match SimpleCamQt's camera-card layout: two columns by seven rows.
		presetGrid->addWidget(button, slot % 7, slot / 7);
		QObject::connect(button, &QPushButton::clicked, this, [this, slot]() { handlePresetButton(slot); });
	}

	auto *actions = new QHBoxLayout();
	actions->setContentsMargins(0, 0, 0, 0);
	actions->setSpacing(2);
	setButton_ = new QPushButton(QStringLiteral("Set"), this);
	renameButton_ = new QPushButton(QStringLiteral("Rename"), this);
	swapButton_ = new QPushButton(QStringLiteral("Swap"), this);
	clearButton_ = new QPushButton(QStringLiteral("Clear"), this);
	actions->addWidget(setButton_);
	actions->addWidget(swapButton_);
	actions->addWidget(clearButton_);
	actions->addWidget(renameButton_);

	auto *navigation = new QVBoxLayout();
	// Shift the shared navigation/speed coordinate system 4 px right to match
	// the measured visual center of the preset grid.
	navigation->setContentsMargins(8, 0, 0, 0);
	navigation->setSpacing(1);
	auto *navigationControls = new QHBoxLayout();
	navigationControls->setContentsMargins(0, 0, 0, 0);
	navigationControls->setSpacing(8);
	constexpr int arrowButtonSize = 35;
	constexpr int arrowGap = 1;
	constexpr int arrowStep = arrowButtonSize + arrowGap;
	constexpr int arrowGridSize = arrowButtonSize * 3 + arrowGap * 2;
	auto *arrowGridWidget = new QWidget(this);
	arrowGridWidget->setFixedSize(arrowGridSize, arrowGridSize);
	auto addArrow = [this, arrowGridWidget](int row, int column, const QString &icon, int pan, int tilt) {
		auto *button = new QPushButton(arrowGridWidget);
		button->setFixedSize(35, 35);
		button->move(column * arrowStep, row * arrowStep);
		button->setIcon(directionIcon(icon));
		button->setIconSize(QSize(34, 34));
		button->setStyleSheet(QStringLiteral(
			"QPushButton { background:#D0D0D0; color:black; border:1px solid #E8E8E8; "
			"margin:0px; padding:0px; border-radius:0px; } "
			"QPushButton:pressed { border:2px solid white; }"));
		QObject::connect(button, &QPushButton::pressed, this, [this, pan, tilt]() { sendPanTilt(pan, tilt); });
		QObject::connect(button, &QPushButton::released, this, [this]() { sendStopPanTilt(); });
	};
	addArrow(0, 0, QStringLiteral("arrow8"), 1, 1);
	addArrow(0, 1, QStringLiteral("arrow1"), 3, 1);
	addArrow(0, 2, QStringLiteral("arrow2"), 2, 1);
	addArrow(1, 0, QStringLiteral("arrow7"), 1, 3);
	auto *homeButton = new QPushButton(arrowGridWidget);
	homeButton->setFixedSize(35, 35);
	homeButton->move(arrowStep, arrowStep);
	homeButton->setIcon(homeIcon());
	homeButton->setIconSize(QSize(28, 28));
	homeButton->setStyleSheet(QStringLiteral(
		"QPushButton { background:#808080; color:black; border:1px solid #E8E8E8; "
		"margin:0px; padding:0px; border-radius:0px; } "
		"QPushButton:pressed { border:2px solid white; }"));
	homeButton->setToolTip(QStringLiteral("Double click to send Home"));
	addArrow(1, 2, QStringLiteral("arrow3"), 2, 3);
	addArrow(2, 0, QStringLiteral("arrow6"), 1, 2);
	addArrow(2, 1, QStringLiteral("arrow5"), 3, 2);
	addArrow(2, 2, QStringLiteral("arrow4"), 2, 2);
	homeDoubleClickTimer_ = new QTimer(this);
	homeDoubleClickTimer_->setSingleShot(true);
	QPointer<CameraControlCard> self(this);
	QObject::connect(homeButton, &QPushButton::clicked, this, [this, self]() {
		if (homeDoubleClickTimer_->isActive()) {
			homeDoubleClickTimer_->stop();
			runtime_->home([self](bool ok, const QString &message) {
				if (self)
					self->setStatus(message, !ok);
			});
		} else {
			homeDoubleClickTimer_->start(350);
		}
	});
	// Use fixed child geometry instead of a nested layout so extra dock height
	// cannot stretch the rows. Each button row is exactly one pixel apart.
	navigationControls->addStretch(1);
	navigationControls->addWidget(arrowGridWidget, 0, Qt::AlignVCenter);

	auto *zoomSide = new QHBoxLayout();
	zoomSide->setContentsMargins(0, 0, 0, 0);
	zoomSide->setSpacing(3);
	zoomSlider_ = new QSlider(Qt::Vertical, this);
	zoomSlider_->setRange(0, 200);
	zoomSlider_->setValue(50);
	zoomSlider_->setFixedHeight(105);
	zoomSlider_->setStyleSheet(QStringLiteral("QSlider::groove:vertical { background:white; width:4px; } "
						 "QSlider::handle:vertical { background:white; border:1px solid #777; height:13px; width:12px; margin:0 -4px; }"));
	QObject::connect(zoomSlider_, &QSlider::sliderReleased, this, [this, self]() {
		runtime_->setZoomPosition(zoomSlider_->value(), [self](bool ok, const QString &message) {
			if (self)
				self->setStatus(ok ? QStringLiteral("Zoom position sent") : message, !ok);
		});
	});
	QObject::connect(zoomSlider_, &QSlider::valueChanged, this, [this](int value) {
		zoomValueLabel_->setText(QString::number(value) + QLatin1Char('%'));
	});
	auto *zoomLabels = new QVBoxLayout();
	zoomLabels->setContentsMargins(0, 0, 0, 0);
	zoomLabels->setSpacing(0);
	auto *zoomIn = new QPushButton(QStringLiteral("+"), this);
	auto *zoomOut = new QPushButton(QStringLiteral("−"), this);
	zoomIn->setFixedSize(22, 20);
	zoomOut->setFixedSize(22, 20);
	zoomIn->setStyleSheet(QStringLiteral("QPushButton { color:#48A9F0; background:transparent; border:0; font-size:18px; font-weight:bold; padding:0px; }"));
	zoomOut->setStyleSheet(QStringLiteral("QPushButton { color:#48A9F0; background:transparent; border:0; font-size:18px; font-weight:bold; padding:0px; }"));
	QObject::connect(zoomIn, &QPushButton::pressed, this, [this, self]() {
		runtime_->zoom(true, [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	QObject::connect(zoomIn, &QPushButton::released, this, [this, self]() {
		runtime_->stopZoom([self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	QObject::connect(zoomOut, &QPushButton::pressed, this, [this, self]() {
		runtime_->zoom(false, [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	QObject::connect(zoomOut, &QPushButton::released, this, [this, self]() {
		runtime_->stopZoom([self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	zoomLabels->addWidget(zoomIn, 0, Qt::AlignLeft | Qt::AlignTop);
	zoomLabels->addStretch(1);
	auto *zoomReadout = new QHBoxLayout();
	zoomReadout->setContentsMargins(0, 0, 0, 0);
	zoomReadout->setSpacing(0);
	zoomReadout->addWidget(new RotatedZoomLabel(this), 0, Qt::AlignVCenter);
	zoomValueLabel_ = new QLabel(QStringLiteral("50%"), this);
	zoomValueLabel_->setMinimumWidth(28);
	zoomValueLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	zoomValueLabel_->setStyleSheet(QStringLiteral("color:white; font-size:9px; font-weight:bold;"));
	zoomReadout->addWidget(zoomValueLabel_, 0, Qt::AlignVCenter);
	zoomLabels->addLayout(zoomReadout);
	zoomLabels->addStretch(1);
	zoomLabels->addWidget(zoomOut, 0, Qt::AlignLeft | Qt::AlignBottom);
	zoomSide->addWidget(zoomSlider_, 0, Qt::AlignVCenter);
	zoomSide->addLayout(zoomLabels);
	navigationControls->addLayout(zoomSide);
	navigationControls->addStretch(1);
	auto *navigationControlsWidget = new QWidget(this);
	navigationControlsWidget->setLayout(navigationControls);
	const int navigationWidth = navigationControlsWidget->sizeHint().width();
	navigationControlsWidget->setFixedWidth(navigationWidth);
	// Keep the 3x3 grid at its natural height when the dock is resized. Without
	// this, the expanding grid layout stretches its rows vertically even though
	// the arrow buttons themselves have a fixed size.
	navigationControlsWidget->setFixedHeight(navigationControlsWidget->sizeHint().height());
	navigation->addWidget(navigationControlsWidget, 0, Qt::AlignHCenter);

	speedSlider_ = new QSlider(Qt::Horizontal, this);
	speedSlider_->setRange(0, 10);
	speedSlider_->setValue(5);
	speedSlider_->setFixedWidth(arrowGridSize);
	auto *speedReadout = new QVBoxLayout();
	speedReadout->setContentsMargins(0, 0, 0, 0);
	speedReadout->setSpacing(0);
	auto *speedLabel = new QLabel(QStringLiteral("Speed"), this);
	auto *speedValue = new QLabel(QString::number(speedSlider_->value()), this);
	speedLabel->setAlignment(Qt::AlignHCenter);
	speedValue->setAlignment(Qt::AlignHCenter);
	speedLabel->setStyleSheet(QStringLiteral("color:#C8C8C8; font-size:9px; font-weight:bold;"));
	speedValue->setStyleSheet(QStringLiteral("color:white; font-size:9px; font-weight:bold;"));
	speedReadout->addWidget(speedLabel);
	speedReadout->addWidget(speedValue);
	QObject::connect(speedSlider_, &QSlider::valueChanged, speedValue,
			 [speedValue](int value) { speedValue->setText(QString::number(value)); });
	auto *speedRowWidget = new QWidget(this);
	speedRowWidget->setFixedWidth(navigationWidth);
	speedRowWidget->setFixedHeight(30);
	auto *speedRow = new QHBoxLayout(speedRowWidget);
	speedRow->setContentsMargins(0, 0, 0, 0);
	speedRow->setSpacing(0);
	speedSlider_->setParent(speedRowWidget);
	speedRow->addWidget(speedSlider_);
	auto *speedReadoutContainer = new QWidget(speedRowWidget);
	speedReadoutContainer->setFixedWidth(38);
	speedReadoutContainer->setLayout(speedReadout);
	speedRow->addWidget(speedReadoutContainer);
	speedRow->addStretch(1);
	navigation->addWidget(speedRowWidget, 0, Qt::AlignHCenter);
	navigationWidget_ = new QWidget(this);
	navigationWidget_->setLayout(navigation);
	navigationWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	root->addWidget(navigationWidget_);

	statusLabel_ = new QLabel(this);
	statusLabel_->setAlignment(Qt::AlignCenter);
	statusLabel_->hide();
	settingsPromptLabel_ = new QLabel(this);
	settingsPromptLabel_->setAlignment(Qt::AlignCenter);
	settingsPromptLabel_->setMinimumHeight(18);
	root->addWidget(settingsPromptLabel_);
	pageDescription_->setMinimumHeight(18);
	root->addWidget(pageDescription_);
	root->addLayout(presetGrid);
	// Keep the preset grid visually separate from its Set/Rename actions while
	// preserving the compact, fixed-height button rows.
	root->addSpacing(8);
	root->addLayout(actions);

	auto *focusRow = new QHBoxLayout();
	focusRow->setContentsMargins(0, 0, 0, 0);
	focusRow->setSpacing(1);
	manualFocusButton_ = new QPushButton(QStringLiteral("Manual Focus"), this);
	manualFocusButton_->setCheckable(true);
	focusPlusButton_ = new QPushButton(QStringLiteral("Focus +"), this);
	focusMinusButton_ = new QPushButton(QStringLiteral("Focus -"), this);
	focusPlusButton_->setFixedWidth(49);
	focusMinusButton_->setFixedWidth(49);
	for (auto *button : {manualFocusButton_, focusPlusButton_, focusMinusButton_}) {
		button->setFixedHeight(26);
		button->setStyleSheet(buttonStyle(QColor("#3C96C9")));
		focusRow->addWidget(button, button == manualFocusButton_ ? 1 : 0);
	}
	focusPlusButton_->setEnabled(false);
	focusMinusButton_->setEnabled(false);
	QObject::connect(manualFocusButton_, &QPushButton::toggled, this, [this, self](bool manual) {
		manualFocusButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), manual));
		focusPlusButton_->setEnabled(manual);
		focusMinusButton_->setEnabled(manual);
		runtime_->setManualFocus(manual, [self](bool ok, const QString &message) {
			if (self)
				self->setStatus(ok ? QStringLiteral("Manual focus %1").arg(self->manualFocusButton_->isChecked() ? "on" : "off") : message, !ok);
		});
	});
	QObject::connect(focusPlusButton_, &QPushButton::pressed, this, [this, self]() {
		runtime_->focusDrive(true, [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	QObject::connect(focusMinusButton_, &QPushButton::pressed, this, [this, self]() {
		runtime_->focusDrive(false, [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
	});
	for (auto *button : {focusPlusButton_, focusMinusButton_})
		QObject::connect(button, &QPushButton::released, this, [this, self]() {
			runtime_->stopFocus([self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
		});
	root->addLayout(focusRow);

	auto *cameraFunctionRow = new QHBoxLayout();
	cameraFunctionRow->setContentsMargins(0, 0, 0, 0);
	cameraFunctionRow->setSpacing(1);
	backlightButton_ = new QPushButton(QStringLiteral("BackLight"), this);
	exposureButton_ = new QPushButton(QStringLiteral("Exp Comp"), this);
	auto *power = new ProtectedPowerButton(this);
	powerButton_ = power;
	backlightButton_->setCheckable(true);
	exposureButton_->setCheckable(true);
	powerButton_->setCheckable(true);
	powerButton_->setChecked(true);
	for (auto *button : {backlightButton_, exposureButton_, powerButton_}) {
		button->setFixedHeight(26);
		button->setStyleSheet(buttonStyle(QColor("#3C96C9")));
		cameraFunctionRow->addWidget(button, 1);
	}
	QObject::connect(backlightButton_, &QPushButton::toggled, this, [this, self](bool enabled) {
		backlightButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), enabled));
		runtime_->backlight(enabled, [self, enabled](bool ok, const QString &message) {
			if (self)
				self->setStatus(ok ? QStringLiteral("BackLight %1").arg(enabled ? "on" : "off") : message, !ok);
		});
	});
	QObject::connect(exposureButton_, &QPushButton::toggled, this, [this, self](bool enabled) {
		exposureButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), enabled));
		runtime_->exposureCompensation(enabled, [self, enabled](bool ok, const QString &message) {
			if (self)
				self->setStatus(ok ? QStringLiteral("Exposure compensation %1").arg(enabled ? "on" : "off") : message, !ok);
		});
	});
	const auto updatePowerStyle = [power](bool cameraOn) {
		power->setText(cameraOn ? QStringLiteral("Off") : QStringLiteral("On"));
		const QColor base = cameraOn ? QColor("#3C96C9") : QColor("#E99A9A");
		power->setStyleSheet(buttonStyle(base));
	};
	power->onUnauthorizedOff = [this]() { setStatus(QStringLiteral("Hold Ctrl while clicking Off to turn the camera off")); };
	QObject::connect(power, &QPushButton::toggled, this, [this, self, power, updatePowerStyle](bool cameraOn) {
		updatePowerStyle(cameraOn);
		runtime_->setCameraPower(cameraOn, [self, cameraOn](bool ok, const QString &message) {
			if (self)
				self->setStatus(ok ? QStringLiteral("Camera %1").arg(cameraOn ? "on" : "off") : message, !ok);
		});
	});
	updatePowerStyle(true);
	root->addLayout(cameraFunctionRow);

	QObject::connect(pageButton_, &QPushButton::clicked, this, [this]() {
		const int next = (runtime_->activePage() + 1) % runtime_->presets().pageCount();
		runtime_->setActivePage(next);
		refreshPresetSourceNames();
		refresh();
	});
	QObject::connect(settingsButton_, &QPushButton::clicked, this, [this]() {
		setSettingsMode(!settingsMode_);
	});
	QObject::connect(pageDescriptionEdit_, &QLineEdit::editingFinished, this, [this]() {
		if (runtime_->presets().setPageDescription(runtime_->activePage(), pageDescriptionEdit_->text()))
			setStatus(QStringLiteral("Page name saved"));
		pageNameDirty_ = false;
		refresh();
	});
	QObject::connect(pageDescriptionEdit_, &QLineEdit::textEdited, this, [this]() { pageNameDirty_ = true; });
	QObject::connect(addPageButton_, &QToolButton::clicked, this, [this]() {
		if (runtime_->presets().addPage()) {
			runtime_->setActivePage(runtime_->presets().pageCount() - 1);
			refreshPresetSourceNames();
			setStatus(QStringLiteral("Added %1").arg(runtime_->presets().page(runtime_->activePage()).description));
		} else {
			setStatus(QStringLiteral("This camera's configured preset capacity has no room for another page"), true);
		}
		refresh();
	});
	QObject::connect(removePageButton_, &QToolButton::clicked, this, [this]() {
		const int page = runtime_->activePage();
		if (runtime_->presets().removePageIfBlank(page)) {
			runtime_->setActivePage(std::min(page, runtime_->presets().pageCount() - 1));
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

void CameraControlCard::setSettingsMode(bool enabled)
{
	settingsMode_ = enabled;
	pendingSwapPage_ = -1;
	pendingSwapSlot_ = -1;
	setMode(Mode::Recall);
	refresh();
	if (layout())
		layout()->invalidate();
	if (layout())
		layout()->activate();
	updateGeometry();
}

void CameraControlCard::refreshView()
{
	refresh();
}

void CameraControlCard::refresh()
{
	const int pageIndex = runtime_->activePage();
	const auto &page = runtime_->presets().page(pageIndex);
	pageDescription_->setText(page.description);
	pageDescription_->setVisible(!settingsMode_);
	if (pageDescriptionSuppressed_)
		pageDescription_->clear();
	cameraSelectorWidget_->setVisible(true);
	cameraTitle_->setText(runtime_->cameraName());
	{
		const QSignalBlocker blocker(pageDescriptionEdit_);
		if (!pageNameDirty_)
			pageDescriptionEdit_->setText(page.description);
	}
	pageDescriptionEdit_->setVisible(settingsMode_);
	pageNumberLabel_->setVisible(settingsMode_);
	pageNumberLabel_->setText(QString::number(pageIndex + 1));
	pageButton_->setVisible(settingsMode_);
	addPageButton_->setVisible(settingsMode_);
	removePageButton_->setVisible(settingsMode_);
	groupToolsWidget_->setVisible(settingsMode_ && !groupToolsSuppressed_);
	if (mode_ == Mode::Swap)
		settingsInstructions_->setText(QStringLiteral("Swap selects two presets to exchange their visual positions.\n"
							      "Camera preset numbers and OBS preset names stay unchanged."));
	else if (mode_ == Mode::Clear)
		settingsInstructions_->setText(QStringLiteral("Select a preset button to clear its label.\n"
							      "The camera preset itself is not erased."));
	else if (mode_ == Mode::Rename)
		settingsInstructions_->setText(QStringLiteral("Select a preset button to edit its name.\n"
							      "The updated name is shared with OBS."));
	else
		settingsInstructions_->setText(QStringLiteral("Use ▲▼ to select a Preset Group.\nUse ADD/DEL to manage groups.\n"
							      "All presets in a group must be cleared to delete the group.\n"
							      "Swap selects two presets to exchange their position in the grid."));
	settingsPromptLabel_->setVisible(false);
	settingsPromptLabel_->setText(statusLabel_->text());
	navigationWidget_->setVisible(!settingsMode_);
	statusLabel_->hide();
	settingsButton_->setText(settingsMode_ ? QStringLiteral("Controls") : QStringLiteral("Settings"));
	setButton_->setVisible(!settingsMode_);
	swapButton_->setVisible(settingsMode_);
	clearButton_->setVisible(settingsMode_);
	renameButton_->setVisible(true);
	setButton_->setCheckable(true);
	renameButton_->setCheckable(true);
	swapButton_->setCheckable(true);
	clearButton_->setCheckable(true);
	setButton_->setChecked(mode_ == Mode::Set);
	renameButton_->setChecked(mode_ == Mode::Rename);
	swapButton_->setChecked(mode_ == Mode::Swap);
	clearButton_->setChecked(mode_ == Mode::Clear);
	const QColor actionColor = groupAccent(pageIndex);
	const QString actionStyle = QStringLiteral("QPushButton { background:%1; color:yellow; border:1px solid #B8B8B8; "
						    "font-weight:bold; padding:0px; } QPushButton:checked { background:%2; color:white; border:2px solid white; }")
					    .arg(actionColor.name(), actionColor.lighter(145).name());
	const QString fixedActionStyle = actionStyle + QStringLiteral(" QPushButton { min-height:28px; max-height:28px; }");
	for (auto *button : {setButton_, renameButton_, swapButton_, clearButton_}) {
		button->setStyleSheet(fixedActionStyle);
		button->setFixedHeight(28);
	}
	for (auto *button : {manualFocusButton_, focusPlusButton_, focusMinusButton_, backlightButton_, exposureButton_, powerButton_})
		button->setFixedHeight(26);
	manualFocusButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), manualFocusButton_->isChecked()));
	backlightButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), backlightButton_->isChecked()));
	exposureButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), exposureButton_->isChecked()));
	pageButton_->setText(QStringLiteral("▲▼"));
	pageButton_->setToolTip(QStringLiteral("Click to cycle preset groups (%1 of %2)").arg(pageIndex + 1).arg(runtime_->presets().pageCount()));

	for (int slotIndex = 0; slotIndex < PresetCatalog::SlotsPerPage; ++slotIndex) {
		auto *button = presetButtons_.at(slotIndex);
		// Keep the actual button geometry identical in Controls and Settings.
		// The settings page editor replaces the navigation block, but must not
		// alter preset row heights or shift the action and function rows.
		button->setFixedHeight(34);
		const auto &slot = runtime_->presets().slot(pageIndex, slotIndex);
		const bool usable = runtime_->presets().hasUsablePreset(pageIndex, slotIndex);
		// Names can be edited before a physical preset is assigned on newly added
		// pages. Keep those names visible; reserve --- for an unnamed, unmapped slot.
		button->setText(!slot.label.trimmed().isEmpty() || usable ? slot.label : QStringLiteral("---"));
		button->setToolTip(usable ? QStringLiteral("Physical camera preset %1").arg(slot.physicalPreset)
					    : QStringLiteral("No physical preset mapped"));
		button->setEnabled(mode_ != Mode::Recall || usable);
		// The flash color is painted on top of the button, so updating its base
		// stylesheet during the flash does not interrupt the visible confirmation.
		// Keeping every preset button on the same stylesheet also keeps their
		// effective size limits consistent when OBS sends a style refresh.
		setButtonColor(button, pendingSwapPage_ == pageIndex && pendingSwapSlot_ == slotIndex ? QColor(255, 244, 128)
										 : pageColor(pageIndex));
	}

	refreshPageManagement();
	setButton_->setText(mode_ == Mode::Set ? QStringLiteral("Select preset…") : QStringLiteral("Set"));
	swapButton_->setText(mode_ == Mode::Swap ? QStringLiteral("Select two…") : QStringLiteral("Swap"));
	clearButton_->setText(mode_ == Mode::Clear ? QStringLiteral("Select preset…") : QStringLiteral("Clear"));
	renameButton_->setText(mode_ == Mode::Rename ? QStringLiteral("Select preset…") : QStringLiteral("Rename"));
}

void CameraControlCard::refreshPageManagement()
{
	const int page = runtime_->activePage();
	addPageButton_->setEnabled(runtime_->presets().pageCount() < runtime_->presets().maxPageCount());
	bool allLabelsBlank = true;
	for (int index = 0; index < PresetCatalog::SlotsPerPage; ++index) {
		if (!runtime_->presets().slot(page, index).label.trimmed().isEmpty()) {
			allLabelsBlank = false;
			break;
		}
	}
	removePageButton_->setEnabled(settingsMode_ && page >= PresetCatalog::MinimumPages && allLabelsBlank);
}

void CameraControlCard::setMode(Mode mode)
{
	const Mode previousMode = mode_;
	if (!settingsMode_ && (mode == Mode::Swap || mode == Mode::Clear))
		mode = Mode::Recall;
	if (settingsMode_ && mode == Mode::Set)
		mode = Mode::Recall;
	mode_ = mode;
	refresh();
	if (!settingsMode_) {
		if (mode_ == Mode::Set)
			setStatus(QStringLiteral("Select a preset button to set"), false, 0);
		else if (mode_ == Mode::Rename)
			setStatus(QStringLiteral("Select a preset button to rename"), false, 0);
		else if ((previousMode == Mode::Set || previousMode == Mode::Rename) && statusHandler_)
			statusHandler_(QString(), false, 0);
	}
}

void CameraControlCard::handlePresetButton(int slotIndex)
{
	const int pageIndex = runtime_->activePage();
	const auto slot = runtime_->presets().slot(pageIndex, slotIndex);
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
		setMode(Mode::Recall);
		setStatus(QStringLiteral("Saving current camera position to preset button %1…").arg(slotIndex + 1));
		QPointer<CameraControlCard> self(this);
		runtime_->storeLogicalPreset(pageIndex, slotIndex, [self, slotIndex](bool ok, const QString &message) {
			if (!self)
				return;
			refreshPresetSourceNames();
			self->setStatus(ok ? QStringLiteral("Preset stored") : message, !ok);
			if (ok)
				self->flashPresetButton(slotIndex);
			self->refresh();
		});
		return;
	}
	case Mode::Rename: {
		bool accepted = false;
		const QString current = slot.label;
		const QString value = QInputDialog::getText(this, QStringLiteral("Rename preset"),
						   QStringLiteral("Button name:"), QLineEdit::Normal, current, &accepted);
		setMode(Mode::Recall);
		if (accepted) {
			runtime_->presets().setLabel(pageIndex, slotIndex, value);
			refreshPresetSourceNames();
			refresh();
			setStatus(QStringLiteral("Preset name saved"));
			flashPresetButton(slotIndex);
		}
		return;
	}
	case Mode::Swap:
		if (pendingSwapPage_ < 0) {
			pendingSwapPage_ = pageIndex;
			pendingSwapSlot_ = slotIndex;
			setStatus(QStringLiteral("First position selected; use PG to change pages if needed"));
			refresh();
		} else {
			const int firstPage = pendingSwapPage_;
			const int firstSlot = pendingSwapSlot_;
			const bool swapped = runtime_->presets().swapSlots(firstPage, firstSlot, pageIndex, slotIndex);
			if (swapped) {
				refreshPresetSourceNames();
				setStatus(QStringLiteral("Visual preset positions swapped"));
			}
			pendingSwapPage_ = -1;
			pendingSwapSlot_ = -1;
			setMode(Mode::Recall);
			if (swapped) {
				if (firstPage == pageIndex)
					flashPresetButton(firstSlot);
				flashPresetButton(slotIndex);
			}
		}
		return;
	case Mode::Clear:
		runtime_->presets().clearLabel(pageIndex, slotIndex);
		refreshPresetSourceNames();
		setStatus(QStringLiteral("Button label cleared; camera preset was not erased"));
		setMode(Mode::Recall);
		flashPresetButton(slotIndex);
		return;
	case Mode::Recall: {
		if (!runtime_->presets().hasUsablePreset(pageIndex, slotIndex)) {
			setStatus(QStringLiteral("This button has no physical preset assigned"), true);
			return;
		}
		setStatus(QStringLiteral("Moving to %1…").arg(slot.label));
		QPointer<CameraControlCard> self(this);
		runtime_->recallLogicalPreset(slotIndex + 1, 0, [self, label = slot.label](bool ok, const QString &message) {
			if (!self)
				return;
			self->setStatus(ok ? QStringLiteral("Moved to %1").arg(label) : message, !ok);
		});
		return;
	}
	}
}

void CameraControlCard::flashPresetButton(int slotIndex, const char *origin)
{
	if (slotIndex < 0 || slotIndex >= presetButtons_.size())
		return;
	const int pageIndex = runtime_->activePage();
	const bool diagnosticTarget = cameraNumber_ == 1 && pageIndex == 0 && slotIndex == 0;
	constexpr int fadeSteps = 19;
	constexpr int frameIntervalMs = 67;
	static std::atomic<unsigned long long> nextDiagnosticSequence{0};
	const unsigned long long diagnosticSequence = diagnosticTarget ? nextDiagnosticSequence.fetch_add(1) + 1 : 0;
	const auto fadeClock = std::make_shared<QElapsedTimer>();
	fadeClock->start();
	auto *button = presetButtons_.at(slotIndex);
	auto *flashButton = static_cast<PresetFlashButton *>(button);
	QWidget *dockWidget = parentWidget();
	const auto logGeometry = [this, button, flashButton, dockWidget, diagnosticTarget, diagnosticSequence, origin, fadeClock, fadeSteps](
																	const char *stage, const QColor &color, int fadeStep) {
		if (!diagnosticTarget)
			return;
		const QRect buttonRect = button->geometry();
		const QPoint buttonGlobal = button->mapToGlobal(QPoint(0, 0));
		const QSize buttonHint = button->sizeHint();
		const QRect contents = button->contentsRect();
		const QRect textRect = flashButton->textBounds();
		const QRect cardRect = geometry();
		const QPoint cardGlobal = mapToGlobal(QPoint(0, 0));
		const QRect cardLayoutRect = layout() ? layout()->geometry() : QRect();
		const QRect dockRect = dockWidget ? dockWidget->geometry() : QRect();
		const QPoint dockGlobal = dockWidget ? dockWidget->mapToGlobal(QPoint(0, 0)) : QPoint();
		const QRect dockLayoutRect = dockWidget && dockWidget->layout() ? dockWidget->layout()->geometry() : QRect();
		const QByteArray colorName = color.name().toLatin1();
		const QByteArray originBytes = QByteArray(origin ? origin : "unknown");
		obs_log(LOG_INFO,
			"[SimpleCamDirect PresetFadeDiag] id=%llu origin=%s stage=%s elapsed_ms=%lld fade_step=%d/%d "
			"group=1 button=1 button_local=(%d,%d %dx%d) button_global=(%d,%d) button_hint=%dx%d "
			"contents=(%d,%d %dx%d) text_bounds=(%d,%d %dx%d) card_local=(%d,%d %dx%d) "
			"card_layout=(%d,%d %dx%d) card_global=(%d,%d) dock_local=(%d,%d %dx%d) "
			"dock_layout=(%d,%d %dx%d) dock_global=(%d,%d) color=%s",
			diagnosticSequence, originBytes.constData(), stage, static_cast<long long>(fadeClock->elapsed()), fadeStep, fadeSteps,
			buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height(), buttonGlobal.x(), buttonGlobal.y(),
			buttonHint.width(), buttonHint.height(), contents.x(), contents.y(), contents.width(), contents.height(),
			textRect.x(), textRect.y(), textRect.width(), textRect.height(), cardRect.x(), cardRect.y(), cardRect.width(),
			cardRect.height(), cardLayoutRect.x(), cardLayoutRect.y(), cardLayoutRect.width(), cardLayoutRect.height(),
			cardGlobal.x(), cardGlobal.y(), dockRect.x(), dockRect.y(), dockRect.width(), dockRect.height(), dockLayoutRect.x(),
			dockLayoutRect.y(), dockLayoutRect.width(), dockLayoutRect.height(), dockGlobal.x(), dockGlobal.y(), colorName.constData());
	};

	PresetFadeEventProbe *eventProbe = nullptr;
	if (diagnosticTarget)
		eventProbe = new PresetFadeEventProbe(button, this, dockWidget, diagnosticSequence,
									QString::fromUtf8(origin ? origin : "unknown"), fadeClock, this);
	if (diagnosticTarget)
		logPresetFadeLayoutSnapshot("flash-start", diagnosticSequence, *fadeClock, this, dockWidget);

	if (flashAnimations_[slotIndex]) {
		flashAnimations_[slotIndex]->stop();
		flashAnimations_[slotIndex]->deleteLater();
	}
	// A queued OBS/Qt layout request can otherwise move the preset row while
	// the yellow flash is visible. Hold this card's layout stable for the short
	// animation, then perform one layout pass after the final color is painted.
	if (!presetFlashLayoutFrozen_ && layout()) {
		presetFlashLayoutWasEnabled_ = layout()->isEnabled();
		if (presetFlashLayoutWasEnabled_)
			layout()->setEnabled(false);
		presetFlashLayoutFrozen_ = true;
	}

	// Twenty visible color states total: yellow at start plus nineteen fade steps.
	// At 67 ms per step, the fade lasts about 1.27 seconds and stays below 15 FPS.
	const QColor startColor(255, 255, 0);
	const QColor endColor = pageColor(pageIndex);
	logGeometry("before-start", endColor, -1);
	auto *timer = new QTimer(this);
	timer->setTimerType(Qt::PreciseTimer);
	timer->setInterval(frameIntervalMs);
	flashAnimations_[slotIndex] = timer;
	if (eventProbe)
		QObject::connect(timer, &QObject::destroyed, eventProbe, [eventProbe]() { eventProbe->deleteLater(); });

	flashButton->setFlashColor(startColor);
	logGeometry("frame", startColor, 0);
	logGeometry("animation-start", startColor, 0);
	auto fadeStep = std::make_shared<int>(0);
	QObject::connect(timer, &QTimer::timeout, this,
			 [this, timer, flashButton, slotIndex, pageIndex, fadeSteps, startColor, endColor, fadeStep, logGeometry,
			  diagnosticTarget, diagnosticSequence, fadeClock, dockWidget]() {
		++(*fadeStep);
		const qreal linearProgress = static_cast<qreal>(*fadeStep) / fadeSteps;
		const qreal progress = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(linearProgress);
		const QColor color = QColor::fromRgbF(startColor.redF() + (endColor.redF() - startColor.redF()) * progress,
								       startColor.greenF() + (endColor.greenF() - startColor.greenF()) * progress,
								       startColor.blueF() + (endColor.blueF() - startColor.blueF()) * progress,
								       startColor.alphaF() + (endColor.alphaF() - startColor.alphaF()) * progress);
		if (*fadeStep < fadeSteps)
			flashButton->setFlashColor(color);
		else
			flashButton->clearFlashColor();
		logGeometry("frame", color, *fadeStep);
		if (*fadeStep < fadeSteps)
			return;

		timer->stop();
		if (flashAnimations_[slotIndex] == timer) {
			flashAnimations_[slotIndex] = nullptr;
			// If the user changed groups during the flash, update only this
			// button's base color instead of refreshing every preset button.
			if (runtime_->activePage() != pageIndex) {
				auto *presetButton = presetButtons_.at(slotIndex);
				setButtonColor(presetButton, pageColor(runtime_->activePage()));
			}
		}
		logGeometry("finished-before-layout-release", endColor, *fadeStep);
		if (diagnosticTarget)
			logPresetFadeLayoutSnapshot("before-layout-release", diagnosticSequence, *fadeClock, this, dockWidget);
		releasePresetFlashLayoutIfIdle();
		logGeometry("finished-after-layout-release", endColor, *fadeStep);
		if (diagnosticTarget)
			logPresetFadeLayoutSnapshot("after-layout-release", diagnosticSequence, *fadeClock, this, dockWidget);
		timer->deleteLater();
	});
	timer->start();
}

void CameraControlCard::releasePresetFlashLayoutIfIdle()
{
	if (!presetFlashLayoutFrozen_)
		return;
	for (QTimer *timer : flashAnimations_) {
		if (timer && timer->isActive())
			return;
	}

	auto *cardLayout = layout();
	if (cardLayout) {
		cardLayout->setEnabled(presetFlashLayoutWasEnabled_);
		if (presetFlashLayoutWasEnabled_) {
			cardLayout->invalidate();
			cardLayout->activate();
		}
	}
	presetFlashLayoutFrozen_ = false;
}

void CameraControlCard::setStatus(const QString &message, bool error, int displayMs)
{
	// Hidden labels are not the status display in the dock. Avoid changing
	// their size hints on an OBS recall; the shared group/status line handles
	// visible feedback and should not relayout the preset card.
	if (!statusLabel_->isHidden()) {
		statusLabel_->setText(message);
		QPalette palette = statusLabel_->palette();
		palette.setColor(QPalette::WindowText, error ? QColor(190, 48, 48) : this->palette().color(QPalette::WindowText));
		statusLabel_->setPalette(palette);
	}
	if (!settingsPromptLabel_->isHidden()) {
		settingsPromptLabel_->setText(message);
		QPalette palette = settingsPromptLabel_->palette();
		palette.setColor(QPalette::WindowText, error ? QColor(190, 48, 48) : this->palette().color(QPalette::WindowText));
		settingsPromptLabel_->setPalette(palette);
	}
	if (statusHandler_)
		statusHandler_(message, error, displayMs);
}

void CameraControlCard::sendPanTilt(int panDirection, int tiltDirection)
{
	const int speed = 1 + qRound(speedSlider_->value() * 17.0 / 10.0);
	QPointer<CameraControlCard> self(this);
	runtime_->panTilt(speed, std::min(speed, 20), static_cast<std::uint8_t>(panDirection), static_cast<std::uint8_t>(tiltDirection),
			 [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
}

void CameraControlCard::sendStopPanTilt()
{
	const int speed = 1 + qRound(speedSlider_->value() * 17.0 / 10.0);
	QPointer<CameraControlCard> self(this);
	runtime_->stopPanTilt(speed, std::min(speed, 20),
			 [self](bool ok, const QString &message) { if (self && !ok) self->setStatus(message, true); });
}

CameraControlDock::CameraControlDock(CameraRuntime &camera1, CameraRuntime &camera2, QWidget *parent)
	: QWidget(parent), runtimes_{&camera1, &camera2}
{
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 4, 4, 4);
	root->setSpacing(2);
	auto *toolbar = new QHBoxLayout();
	dualButton_ = new QPushButton(QStringLiteral("Dual"), this);
	dualButton_->setCheckable(true);
	dualButton_->setFixedSize(54, 26);
	dualButton_->setStyleSheet(buttonStyle(QColor("#D0A13A")) + QStringLiteral(" QPushButton { min-height:26px; max-height:26px; }"));
	settingsButton_ = new QPushButton(QStringLiteral("Settings"), this);
	const int settingsButtonWidth =
		std::max(settingsButton_->fontMetrics().horizontalAdvance(QStringLiteral("Settings")),
			 settingsButton_->fontMetrics().horizontalAdvance(QStringLiteral("Controls"))) + 16;
	settingsButton_->setFixedSize(settingsButtonWidth, 26);
	settingsButton_->setStyleSheet(buttonStyle(QColor("#3C96C9")) + QStringLiteral(" QPushButton { min-height:26px; max-height:26px; }"));
	auto *settingsTitle = new QLabel(QStringLiteral("Settings"), this);
	settingsTitle->setAlignment(Qt::AlignCenter);
	settingsTitle->setFixedHeight(26);
	settingsTitle->setStyleSheet(QStringLiteral("color:white; font-size:16px;"));
	settingsTitle->hide();
	toolbar->addWidget(dualButton_);
	toolbar->addStretch(1);
	toolbar->addWidget(settingsTitle);
	toolbar->addStretch(1);
	toolbar->addWidget(settingsButton_);
	root->addLayout(toolbar);
	cameraHeadersWidget_ = new QWidget(this);
	cameraHeadersLayout_ = new QHBoxLayout(cameraHeadersWidget_);
	cameraHeadersLayout_->setContentsMargins(0, 0, 0, 0);
	cameraHeadersLayout_->setSpacing(2);
	cameraHeadersWidget_->hide();
	root->addWidget(cameraHeadersWidget_);

	cardsLayout_ = new QHBoxLayout();
	cardsLayout_->setContentsMargins(0, 0, 0, 0);
	cardsLayout_->setSpacing(2);
	camera1Card_ = new CameraControlCard(camera1, 1, true, this);
	camera2Card_ = new CameraControlCard(camera2, 2, true, this);
	camera1Card_->pageDescriptionSuppressed_ = true;
	camera2Card_->pageDescriptionSuppressed_ = true;
	camera1Card_->refresh();
	camera2Card_->refresh();
	camera1Card_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	camera2Card_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	camera1Card_->setCameraSelectionHandler([this](int cameraIndex) { selectCameraForCard(0, cameraIndex); });
	camera2Card_->setCameraSelectionHandler([this](int cameraIndex) { selectCameraForCard(1, cameraIndex); });
	const auto installRecallFeedback = [this](CameraRuntime &runtime) {
		QPointer<CameraControlDock> self(this);
		CameraRuntime *runtimePointer = &runtime;
		runtime.setPresetRecallObserver([self, runtimePointer](int logicalButton, CameraRuntime::PresetRecallOrigin origin) {
			if (!self || logicalButton < 1 || logicalButton > PresetCatalog::SlotsPerPage)
				return;
			for (CameraControlCard *card : {self->camera1Card_, self->camera2Card_}) {
				if (card && card->runtime_ == runtimePointer) {
					const char *originText = origin == CameraRuntime::PresetRecallOrigin::ObsSource ? "OBS-source" : "camera-control";
					card->flashPresetButton(logicalButton - 1, originText);
				}
			}
		});
	};
	installRecallFeedback(camera1);
	installRecallFeedback(camera2);
	sharedGroupToolsWidget_ = camera1Card_->groupToolsWidget_;
	cardsLayout_->addWidget(camera1Card_, 1);
	cardsLayout_->addWidget(camera2Card_, 1);
	root->addLayout(cardsLayout_);
	sharedGroupIndicator_ = new QLabel(this);
	sharedGroupIndicator_->setAlignment(Qt::AlignCenter);
	sharedGroupIndicator_->setMinimumHeight(18);
	sharedGroupIndicator_->setStyleSheet(QStringLiteral("QLabel { background:transparent; padding:0px; }"));
	sharedGroupMessageTimer_ = new QTimer(this);
	sharedGroupMessageTimer_->setSingleShot(true);
	QObject::connect(sharedGroupMessageTimer_, &QTimer::timeout, this, [this]() {
		sharedGroupMessage_.clear();
		updateSharedGroupIndicator();
	});
	auto showSharedMessage = [this](const QString &message, bool error, int displayMs) {
		if (settingsMode_)
			return;
		sharedGroupMessage_ = message;
		sharedGroupMessageError_ = error;
		if (message.isEmpty())
			sharedGroupMessageTimer_->stop();
		else if (displayMs > 0)
			sharedGroupMessageTimer_->start(displayMs);
		else
			sharedGroupMessageTimer_->stop();
		updateSharedGroupIndicator();
	};
	camera1Card_->statusHandler_ = showSharedMessage;
	camera2Card_->statusHandler_ = showSharedMessage;
	dualButton_->setChecked(true);
	dualButton_->setText(QStringLiteral("Single"));
	root->activate();
	adjustSize();
	// Settings always gets the width used by the two-card control layout.
	settingsWidth_ = sizeHint().width();
	// Measure the natural single-card control width once. Settings can open at
	// the dual width without leaving that width behind when Controls returns.
	camera2Card_->hide();
	root->invalidate();
	root->activate();
	updateGeometry();
	singleControlWidth_ = sizeHint().width();
	camera2Card_->show();
	root->invalidate();
	root->activate();
	adjustSize();
	updateSharedGroupIndicator();

	auto enterSettingsLayout = [this, root]() {
		auto *card1Layout = qobject_cast<QVBoxLayout *>(camera1Card_->layout());
		auto *card2Layout = qobject_cast<QVBoxLayout *>(camera2Card_->layout());
		card1Layout->removeWidget(camera1Card_->cameraHeaderWidget_);
		card2Layout->removeWidget(camera2Card_->cameraHeaderWidget_);
		camera1Card_->cameraHeaderWidget_->setParent(cameraHeadersWidget_);
		camera2Card_->cameraHeaderWidget_->setParent(cameraHeadersWidget_);
		cameraHeadersLayout_->addWidget(camera1Card_->cameraHeaderWidget_, 1);
		cameraHeadersLayout_->addWidget(camera2Card_->cameraHeaderWidget_, 1);
		cameraHeadersWidget_->setVisible(true);
		// The group editor adds a taller row than the default group caption in
		// Controls. Shorten Help by that difference so both modes have the same
		// total height and no blank filler is needed.
		const int helpHeightAdjustment = camera1Card_->pageButton_->height() - camera1Card_->pageDescription_->minimumHeight();
		const int helpHeight = std::max(60, camera1Card_->navigationWidget_->sizeHint().height() - helpHeightAdjustment - 3);
		camera1Card_->settingsInstructions_->setFixedHeight(helpHeight);
		camera2Card_->settingsInstructions_->setFixedHeight(helpHeight);

		card1Layout->removeWidget(sharedGroupToolsWidget_);
		sharedGroupToolsWidget_->setParent(this);
		root->insertWidget(2, sharedGroupToolsWidget_);
		camera1Card_->groupToolsSuppressed_ = false;
		camera2Card_->groupToolsSuppressed_ = true;
		camera2Card_->groupToolsWidget_->hide();
		sharedGroupMessageTimer_->stop();
		sharedGroupMessage_.clear();
		updateSharedGroupIndicator();
	};
	auto restoreControlLayout = [this, root]() {
		root->removeWidget(sharedGroupToolsWidget_);
		sharedGroupToolsWidget_->setParent(camera1Card_);
		auto *card1Layout = qobject_cast<QVBoxLayout *>(camera1Card_->layout());
		auto *card2Layout = qobject_cast<QVBoxLayout *>(camera2Card_->layout());
		card1Layout->insertWidget(1, sharedGroupToolsWidget_);

		cameraHeadersLayout_->removeWidget(camera1Card_->cameraHeaderWidget_);
		cameraHeadersLayout_->removeWidget(camera2Card_->cameraHeaderWidget_);
		camera1Card_->cameraHeaderWidget_->setParent(camera1Card_);
		camera2Card_->cameraHeaderWidget_->setParent(camera2Card_);
		card1Layout->insertWidget(0, camera1Card_->cameraHeaderWidget_);
		card2Layout->insertWidget(0, camera2Card_->cameraHeaderWidget_);
		cameraHeadersWidget_->hide();
		camera1Card_->groupToolsSuppressed_ = false;
		camera2Card_->groupToolsSuppressed_ = false;
		updateSharedGroupIndicator();
	};

	QObject::connect(settingsButton_, &QPushButton::clicked, this,
			 [this, settingsTitle, root, enterSettingsLayout, restoreControlLayout]() {
		const int transition = ++layoutTransitionSequence_;
		obs_log(LOG_INFO, "[SimpleCamDirect LayoutDiag] transition=%d begin currentMode=%s", transition,
			settingsMode_ ? "Settings" : "Controls");
		logLayoutDiagnostics("transition-before", transition, resizeRequestSequence_);
		settingsMode_ = !settingsMode_;
		camera1Card_->setSettingsMode(settingsMode_);
		camera2Card_->setSettingsMode(settingsMode_);
		if (settingsMode_)
			enterSettingsLayout();
		else
			restoreControlLayout();
		settingsButton_->setText(settingsMode_ ? QStringLiteral("Controls") : QStringLiteral("Settings"));
		dualButton_->setFixedHeight(26);
		settingsButton_->setFixedHeight(26);
		settingsTitle->setFixedHeight(26);
		settingsTitle->setVisible(settingsMode_);
		camera2Card_->setVisible(dualButton_->isChecked());
		cameraHeadersWidget_->setVisible(settingsMode_);
		camera2Card_->cameraHeaderWidget_->setVisible(dualButton_->isChecked());
		camera1Card_->setVisible(true);
		dualButton_->setText(dualButton_->isChecked() ? QStringLiteral("Single") : QStringLiteral("Dual"));
		const int targetWidth = (settingsMode_ || dualButton_->isChecked()) ? settingsWidth_ : singleControlWidth_;
		root->invalidate();
		camera1Card_->updateGeometry();
		camera2Card_->updateGeometry();
		root->activate();
		cardsLayout_->activate();
		logLayoutDiagnostics("transition-after-layout-activate", transition, resizeRequestSequence_);
		resizeDockToWidth(targetWidth);
		updateSharedGroupIndicator();
		logLayoutDiagnostics("transition-after-resize-call", transition, resizeRequestSequence_);
	});
	QObject::connect(dualButton_, &QPushButton::toggled, this, [this, settingsTitle, root](bool enabled) {
		const int transition = ++layoutTransitionSequence_;
		obs_log(LOG_INFO, "[SimpleCamDirect LayoutDiag] transition=%d dual-change enabled=%d mode=%s", transition, enabled ? 1 : 0,
			settingsMode_ ? "Settings" : "Controls");
		camera2Card_->setVisible(enabled);
		camera2Card_->cameraHeaderWidget_->setVisible(enabled);
		camera1Card_->setVisible(true);
		dualButton_->setText(enabled ? QStringLiteral("Single") : QStringLiteral("Dual"));
		dualButton_->setFixedHeight(26);
		settingsButton_->setFixedHeight(26);
		settingsTitle->setFixedHeight(26);
		settingsTitle->setVisible(settingsMode_);
		cameraHeadersWidget_->setVisible(settingsMode_);
		const int targetWidth = (settingsMode_ || enabled) ? settingsWidth_ : singleControlWidth_;
		root->invalidate();
		camera1Card_->updateGeometry();
		camera2Card_->updateGeometry();
		root->activate();
		cardsLayout_->activate();
		updateGeometry();
		logLayoutDiagnostics("dual-after-layout-activate", transition, resizeRequestSequence_);
		resizeDockToWidth(targetWidth);
		updateSharedGroupIndicator();
		logLayoutDiagnostics("dual-after-resize-call", transition, resizeRequestSequence_);
	});
	auto *refreshTimer = new QTimer(this);
	refreshTimer->setInterval(1000);
	QObject::connect(refreshTimer, &QTimer::timeout, this, [this]() {
		camera1Card_->refreshView();
		camera2Card_->refreshView();
		updateSharedGroupIndicator();
	});
	refreshTimer->start();
	QTimer::singleShot(0, this, [this]() { logLayoutDiagnostics("startup-after-show", 0, resizeRequestSequence_); });
}

void CameraControlDock::logLayoutDiagnostics(const char *phase, int transition, int resizeRequest) const
{
	QWidget *ancestor = parentWidget();
	while (ancestor && !qobject_cast<QDockWidget *>(ancestor))
		ancestor = ancestor->parentWidget();
	auto *dock = qobject_cast<QDockWidget *>(ancestor);
	QWidget *focusWidget = QApplication::focusWidget();
	const QPoint cursorPosition = QCursor::pos();
	QWidget *cursorWidget = QApplication::widgetAt(cursorPosition);
	const QByteArray focusObjectName = focusWidget ? focusWidget->objectName().toUtf8() : QByteArrayLiteral("-");
	const QByteArray cursorObjectName = cursorWidget ? cursorWidget->objectName().toUtf8() : QByteArrayLiteral("-");
	const bool modeSettings = settingsMode_;
	obs_log(LOG_INFO,
		"[SimpleCamDirect LayoutDiag] phase=%s transition=%d resize=%d mode=%s dual=%d "
		"dockWidget=%dx%d dockHint=%dx%d dockMin=%dx%d dockMax=%dx%d "
		"dockFloating=%d dockVisible=%d windowActive=%d focus=%s/%s cursor=%d,%d:%s/%s",
		phase, transition, resizeRequest, modeSettings ? "Settings" : "Controls", dualButton_ && dualButton_->isChecked() ? 1 : 0,
		dock ? dock->width() : -1, dock ? dock->height() : -1, dock ? dock->sizeHint().width() : -1, dock ? dock->sizeHint().height() : -1,
		dock ? dock->minimumWidth() : -1, dock ? dock->minimumHeight() : -1, dock ? dock->maximumWidth() : -1,
		dock ? dock->maximumHeight() : -1, dock && dock->isFloating() ? 1 : 0, dock && dock->isVisible() ? 1 : 0,
		dock && dock->window()->isActiveWindow() ? 1 : 0,
		focusWidget ? focusWidget->metaObject()->className() : "-", focusObjectName.constData(), cursorPosition.x(), cursorPosition.y(),
		cursorWidget ? cursorWidget->metaObject()->className() : "-", cursorObjectName.constData());
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "dock-content", dock ? dock->widget() : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "main", this);
	logLayoutMetrics(phase, transition, resizeRequest, "main-root", layout());
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "camera-headers", cameraHeadersWidget_);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "shared-group-tools", sharedGroupToolsWidget_);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card1", camera1Card_);
	logLayoutMetrics(phase, transition, resizeRequest, "card1-layout", camera1Card_ ? camera1Card_->layout() : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card1-navigation", camera1Card_ ? camera1Card_->navigationWidget_ : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card1-group-tools", camera1Card_ ? camera1Card_->groupToolsWidget_ : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card1-help", camera1Card_ ? camera1Card_->settingsInstructions_ : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card1-preset-first", camera1Card_ && !camera1Card_->presetButtons_.isEmpty()
									 ? camera1Card_->presetButtons_.first()
									 : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card1-preset-last", camera1Card_ && !camera1Card_->presetButtons_.isEmpty()
									? camera1Card_->presetButtons_.last()
									: nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card1-set", camera1Card_ ? camera1Card_->setButton_ : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card1-swap", camera1Card_ ? camera1Card_->swapButton_ : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card2", camera2Card_);
	logLayoutMetrics(phase, transition, resizeRequest, "card2-layout", camera2Card_ ? camera2Card_->layout() : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card2-navigation", camera2Card_ ? camera2Card_->navigationWidget_ : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card2-help", camera2Card_ ? camera2Card_->settingsInstructions_ : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card2-preset-first", camera2Card_ && !camera2Card_->presetButtons_.isEmpty()
									 ? camera2Card_->presetButtons_.first()
									 : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card2-preset-last", camera2Card_ && !camera2Card_->presetButtons_.isEmpty()
									? camera2Card_->presetButtons_.last()
									: nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card2-set", camera2Card_ ? camera2Card_->setButton_ : nullptr);
	logLayoutWidgetMetrics(phase, transition, resizeRequest, "card2-swap", camera2Card_ ? camera2Card_->swapButton_ : nullptr);
	logLayoutMetrics(phase, transition, resizeRequest, "cards-layout", cardsLayout_);
}

void CameraControlDock::updateSharedGroupIndicator()
{
	if (!sharedGroupIndicator_ || !camera1Card_ || settingsMode_) {
		if (sharedGroupIndicator_)
			sharedGroupIndicator_->hide();
		return;
	}
	positionSharedGroupIndicator();
	if (!sharedGroupMessage_.isEmpty()) {
		sharedGroupIndicator_->setText(sharedGroupMessage_);
		QPalette palette = sharedGroupIndicator_->palette();
		palette.setColor(QPalette::WindowText, sharedGroupMessageError_ ? QColor(230, 90, 90) : QColor(Qt::white));
		sharedGroupIndicator_->setPalette(palette);
	} else {
		sharedGroupIndicator_->setText(camera1Card_->runtime_->presets().page(camera1Card_->runtime_->activePage()).description);
		QPalette palette = sharedGroupIndicator_->palette();
		palette.setColor(QPalette::WindowText, QColor(Qt::white));
		sharedGroupIndicator_->setPalette(palette);
	}
	sharedGroupIndicator_->show();
	sharedGroupIndicator_->raise();
}

void CameraControlDock::resizeDockToWidth(int targetWidth)
{
	const int resizeRequest = ++resizeRequestSequence_;
	const int transition = layoutTransitionSequence_;
	logLayoutDiagnostics("resize-enter", transition, resizeRequest);
	QWidget *ancestor = parentWidget();
	while (ancestor && !qobject_cast<QDockWidget *>(ancestor))
		ancestor = ancestor->parentWidget();
	auto *dock = qobject_cast<QDockWidget *>(ancestor);
	const int dockContentInset = dock ? std::max(0, dock->width() - width()) : 0;
	const int dockContentHeight = dock ? std::max(0, dock->height() - height()) : 0;
	// Drop the previous mode's constraints before changing the dock's layout.
	// The new height is intentionally measured later, after Qt processes the
	// visibility/reparenting layout requests from this mode transition.
	setMinimumHeight(0);
	setMaximumHeight(QWIDGETSIZE_MAX);
	setMinimumWidth(targetWidth);
	setMaximumWidth(QWIDGETSIZE_MAX);
	if (dock) {
		dock->setMinimumHeight(0);
		dock->setMaximumHeight(QWIDGETSIZE_MAX);
		dock->setMinimumWidth(targetWidth + dockContentInset);
		dock->setMaximumWidth(QWIDGETSIZE_MAX);
	}
	if (layout())
		layout()->invalidate();
	updateGeometry();
	QPointer<QDockWidget> dockGuard(dock);
	QPointer<CameraControlDock> self(this);
	const bool hadDock = dock != nullptr;
	const bool requestedSettingsMode = settingsMode_;
	QTimer::singleShot(0, this, [dockGuard, self, hadDock, requestedSettingsMode, targetWidth, dockContentInset, dockContentHeight, transition,
				     resizeRequest]() {
		if (!self || (hadDock && !dockGuard))
			return;
		if (transition != self->layoutTransitionSequence_ || resizeRequest != self->resizeRequestSequence_) {
			obs_log(LOG_INFO,
				"[SimpleCamDirect LayoutDiag] phase=resize-callback-stale transition=%d resize=%d currentTransition=%d currentResize=%d",
				transition, resizeRequest, self->layoutTransitionSequence_, self->resizeRequestSequence_);
			return;
		}
		if (self->settingsMode_ != requestedSettingsMode) {
			obs_log(LOG_INFO,
				"[SimpleCamDirect LayoutDiag] phase=resize-callback-mode-mismatch transition=%d resize=%d mode=%s",
				transition, resizeRequest, self->settingsMode_ ? "Settings" : "Controls");
			return;
		}

		// Reparenting and visibility changes post layout requests. Activate child
		// layouts first, then the card row, then the dock content layout so the
		// measured minimum includes every visible preset row and editor.
		self->setMinimumHeight(0);
		self->setMaximumHeight(QWIDGETSIZE_MAX);
		self->setMinimumWidth(targetWidth);
		self->setMaximumWidth(QWIDGETSIZE_MAX);
		if (dockGuard) {
			dockGuard->setMinimumHeight(0);
			dockGuard->setMaximumHeight(QWIDGETSIZE_MAX);
			dockGuard->setMinimumWidth(targetWidth + dockContentInset);
			dockGuard->setMaximumWidth(QWIDGETSIZE_MAX);
		}
		for (CameraControlCard *card : {self->camera1Card_, self->camera2Card_}) {
			if (!card)
				continue;
			card->updateGeometry();
			if (card->layout()) {
				card->layout()->invalidate();
				card->layout()->activate();
			}
		}
		if (self->sharedGroupToolsWidget_ && self->sharedGroupToolsWidget_->layout()) {
			self->sharedGroupToolsWidget_->layout()->invalidate();
			self->sharedGroupToolsWidget_->layout()->activate();
		}
		if (self->cameraHeadersLayout_) {
			self->cameraHeadersLayout_->invalidate();
			self->cameraHeadersLayout_->activate();
		}
		if (self->cardsLayout_) {
			self->cardsLayout_->invalidate();
			self->cardsLayout_->activate();
		}
		if (self->layout()) {
			self->layout()->invalidate();
			self->layout()->activate();
		}
		self->adjustSize();

		const int layoutHintHeight = self->layout() ? self->layout()->sizeHint().height() : 0;
		const int layoutMinimumHeight = self->layout() ? self->layout()->minimumSize().height() : 0;
		const int widgetHintHeight = self->sizeHint().height();
		const int widgetMinimumHintHeight = self->minimumSizeHint().height();
		const int targetHeight = std::max({layoutHintHeight, layoutMinimumHeight, widgetHintHeight, widgetMinimumHintHeight});
		obs_log(LOG_INFO,
			"[SimpleCamDirect LayoutDiag] phase=resize-post-layout transition=%d resize=%d targetWidth=%d targetHeight=%d "
			"mode=%s layoutHint=%d layoutMin=%d widgetHint=%d widgetMinHint=%d",
			transition, resizeRequest, targetWidth, targetHeight, self->settingsMode_ ? "Settings" : "Controls", layoutHintHeight,
			layoutMinimumHeight, widgetHintHeight, widgetMinimumHintHeight);

		self->resize(targetWidth, targetHeight);
		self->setMinimumHeight(targetHeight);
		self->setMaximumHeight(targetHeight);
		if (dockGuard) {
			const int targetDockWidth = targetWidth + dockContentInset;
			const int targetDockHeight = targetHeight + dockContentHeight;
			dockGuard->setMinimumWidth(targetDockWidth);
			dockGuard->setMaximumWidth(QWIDGETSIZE_MAX);
			dockGuard->setMinimumHeight(targetDockHeight);
			dockGuard->setMaximumHeight(targetDockHeight);
			dockGuard->resize(targetDockWidth, targetDockHeight);
		}
		self->logLayoutDiagnostics("resize-post-layout-applied", transition, resizeRequest);
	});
}

void CameraControlDock::positionSharedGroupIndicator()
{
	if (!sharedGroupIndicator_ || !camera1Card_ || settingsMode_)
		return;
	auto *leftLabel = camera1Card_->pageDescription_;
	auto *rightLabel = dualButton_->isChecked() && !camera2Card_->isHidden() ? camera2Card_->pageDescription_ : leftLabel;
	const QPoint leftTop = leftLabel->mapTo(this, QPoint(0, 0));
	const QPoint rightTop = rightLabel->mapTo(this, QPoint(0, 0));
	const int left = leftTop.x();
	const int right = rightTop.x() + rightLabel->width();
	const int top = leftTop.y();
	const int height = leftLabel->height();
	sharedGroupIndicator_->setGeometry(left, top, std::max(0, right - left), height);
}

void CameraControlDock::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	positionSharedGroupIndicator();
}

void CameraControlDock::selectCameraForCard(int cardIndex, int cameraIndex)
{
	if (cardIndex < 0 || cardIndex > 1 || cameraIndex < 0 || cameraIndex > 1)
		return;
	const int otherCard = 1 - cardIndex;
	if (cameraAssignments_[cardIndex] == cameraIndex)
		return;
	if (cameraAssignments_[otherCard] == cameraIndex) {
		cameraAssignments_[otherCard] = cameraAssignments_[cardIndex];
		(otherCard == 0 ? camera1Card_ : camera2Card_)->setRuntime(
			*runtimes_[cameraAssignments_[otherCard]], cameraAssignments_[otherCard] + 1);
	}
	cameraAssignments_[cardIndex] = cameraIndex;
	(cardIndex == 0 ? camera1Card_ : camera2Card_)->setRuntime(
		*runtimes_[cameraAssignments_[cardIndex]], cameraAssignments_[cardIndex] + 1);
	updateSharedGroupIndicator();
}

CamerasDock::CamerasDock(CameraRuntime &camera1, CameraRuntime &camera2, QWidget *parent)
	: QWidget(parent), runtimes_{&camera1, &camera2}
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(10, 10, 10, 10);
	auto *form = new QFormLayout();
	cameraSelector_ = new QComboBox(this);
	cameraSelector_->addItem(camera1.cameraName());
	cameraSelector_->addItem(camera2.cameraName());
	cameraNameEdit_ = new QLineEdit(camera1.cameraName(), this);
	cameraNameEdit_->setMaxLength(40);
	auto *model = new QLabel(QStringLiteral("AViPAS AV-1280W — VISCA over TCP"), this);
	model->setWordWrap(true);
	hostEdit_ = new QLineEdit(camera1.host(), this);
	hostEdit_->setPlaceholderText(QStringLiteral("Camera IP address"));
	portSpin_ = new QSpinBox(this);
	portSpin_->setRange(1, 65535);
	portSpin_->setValue(camera1.port());
	addressSpin_ = new QSpinBox(this);
	addressSpin_->setRange(1, 7);
	addressSpin_->setValue(camera1.cameraAddress());
	capacitySpin_ = new QSpinBox(this);
	capacitySpin_->setRange(1, 255);
	capacitySpin_->setValue(camera1.capacity());
	form->addRow(QStringLiteral("Camera"), cameraSelector_);
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

	auto loadSelectedCamera = [this]() {
		CameraRuntime &selected = *runtimes_[cameraSelector_->currentIndex()];
		const QSignalBlocker blockPort(portSpin_);
		const QSignalBlocker blockAddress(addressSpin_);
		const QSignalBlocker blockCapacity(capacitySpin_);
		cameraNameEdit_->setText(selected.cameraName());
		hostEdit_->setText(selected.host());
		portSpin_->setValue(selected.port());
		addressSpin_->setValue(selected.cameraAddress());
		capacitySpin_->setValue(selected.capacity());
		statusLabel_->setText(QStringLiteral("Settings for %1").arg(selected.cameraName()));
	};
	QObject::connect(cameraSelector_, qOverload<int>(&QComboBox::currentIndexChanged), this,
			 [loadSelectedCamera](int) { loadSelectedCamera(); });
	QObject::connect(cameraNameEdit_, &QLineEdit::editingFinished, this,
				 [this]() {
					 runtimes_[cameraSelector_->currentIndex()]->setCameraName(cameraNameEdit_->text());
					 cameraSelector_->setItemText(cameraSelector_->currentIndex(), cameraNameEdit_->text());
					 refreshPresetSourceNames();
				 });
	QObject::connect(hostEdit_, &QLineEdit::editingFinished, this,
			 [this]() { runtimes_[cameraSelector_->currentIndex()]->setHost(hostEdit_->text()); });
	QObject::connect(portSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
			 [this](int port) { runtimes_[cameraSelector_->currentIndex()]->setPort(static_cast<quint16>(port)); });
	QObject::connect(addressSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
			 [this](int address) { runtimes_[cameraSelector_->currentIndex()]->setCameraAddress(address); });
	QObject::connect(capacitySpin_, qOverload<int>(&QSpinBox::valueChanged), this,
			 [this](int capacity) { runtimes_[cameraSelector_->currentIndex()]->setCapacity(capacity); });
	QPointer<CamerasDock> self(this);
	QObject::connect(testButton, &QPushButton::clicked, this, [this, self]() {
		runtimes_[cameraSelector_->currentIndex()]->testConnection([self](bool ok, const QString &message) {
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
