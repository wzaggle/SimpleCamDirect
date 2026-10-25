#include "camera_docks.hpp"

#include "camera_runtime.hpp"
#include "preset_source.hpp"

#include <obs-module.h>
#include <obs.h>
#include <plugin-support.h>

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QComboBox>
#include <QDockWidget>
#include <QCursor>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
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
#include <QMoveEvent>
#include <QMouseEvent>
#include <QMainWindow>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPushButton>
#include <QPointer>
#include <QRubberBand>
#include <QResizeEvent>
#include <QRectF>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QSet>
#include <QSplitter>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <memory>
#include <utility>

namespace simplecam {
obs_source_t *currentProgramSceneSource();

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

// One sizing contract for every button in the bottom function row,
// including the protected power subclass and the settings gear.
QString bottomFunctionStyle(const QColor &background)
{
	return buttonStyle(background) + QStringLiteral(
		" QPushButton { margin:0px; padding:0px; min-height:24px; }");
}

void useBottomRowSizing(QPushButton *button)
{
	button->setMinimumHeight(26);
	button->setMaximumHeight(QWIDGETSIZE_MAX);
	button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
}

// Paint the gear after the styled button face, bypassing OBS/QIcon tinting.
class BlackSettingsButton final : public QPushButton {
public:
	explicit BlackSettingsButton(QWidget *parent = nullptr) : QPushButton(parent) {}

protected:
	void paintEvent(QPaintEvent *event) override
	{
		QPushButton::paintEvent(event);
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.translate((width() - 18.0) / 2.0, (height() - 18.0) / 2.0);
		QPainterPath gear;
		for (int point = 0; point < 64; ++point) {
			const double angle = point * 6.283185307179586 / 64.0;
			const double radius = (point % 8 >= 2 && point % 8 <= 5) ? 7.5 : 5.7;
			const QPointF vertex(9.0 + radius * std::cos(angle), 9.0 + radius * std::sin(angle));
			if (point == 0)
				gear.moveTo(vertex);
			else
				gear.lineTo(vertex);
		}
		gear.closeSubpath();
		gear.addEllipse(QPointF(9, 9), 2.6, 2.6);
		gear.setFillRule(Qt::OddEvenFill);
		painter.fillPath(gear, Qt::black);
	}
};

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
		"[SimpleCamPTZ LayoutDiag] phase=%s transition=%d resize=%d widget=%s geom=%dx%d@%d,%d "
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
		"[SimpleCamPTZ LayoutDiag] phase=%s transition=%d resize=%d layout=%s geom=%dx%d@%d,%d "
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
		"[SimpleCamPTZ PresetFadeWidget] id=%llu phase=%s elapsed_ms=%lld path=%s class=%s name=\"%s\" "
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
		"[SimpleCamPTZ PresetFadeLayout] id=%llu phase=%s elapsed_ms=%lld path=%s "
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
				"[SimpleCamPTZ PresetFadeSpacer] id=%llu phase=%s elapsed_ms=%lld path=%s hint=%dx%d",
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

struct CameraFeedScanContext {
	std::array<QString, CameraSlotCount> sourceUuids;
	int topCameraIndex = -1;
	int depthLimit = 24;
};

int configuredCameraForSource(const QString &uuid, const CameraFeedScanContext &context)
{
	if (uuid.isEmpty())
		return -1;
	int match = -1;
	for (int index = 0; index < CameraSlotCount; ++index) {
		if (context.sourceUuids[index] != uuid)
			continue;
		if (match >= 0)
			return -1; // Duplicate mappings are ambiguous; never guess.
		match = index;
	}
	return match;
}

void scanVisibleCameraFeeds(obs_scene_t *scene, CameraFeedScanContext &context, int depth = 0);

bool scanCameraFeedItem(obs_scene_t *, obs_sceneitem_t *item, void *opaque)
{
	auto *state = static_cast<std::pair<CameraFeedScanContext *, int> *>(opaque);
	CameraFeedScanContext &context = *state->first;
	const int depth = state->second;
	if (!obs_sceneitem_visible(item))
		return true;

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source)
		return true;
	const char *uuid = obs_source_get_uuid(source);
	if (uuid) {
		const int cameraIndex = configuredCameraForSource(QString::fromUtf8(uuid), context);
		if (cameraIndex >= 0)
			context.topCameraIndex = cameraIndex;
	}

	if (depth >= context.depthLimit)
		return true;
	if (obs_sceneitem_is_group(item)) {
		if (obs_scene_t *groupScene = obs_sceneitem_group_get_scene(item))
			scanVisibleCameraFeeds(groupScene, context, depth + 1);
	} else if (obs_scene_t *nestedScene = obs_scene_from_source(source)) {
		scanVisibleCameraFeeds(nestedScene, context, depth + 1);
	}
	return true;
}

void scanVisibleCameraFeeds(obs_scene_t *scene, CameraFeedScanContext &context, int depth)
{
	if (!scene || depth > context.depthLimit)
		return;
	std::pair<CameraFeedScanContext *, int> state{&context, depth};
	// OBS enumerates bottom-to-top. Each nested scene is scanned at its parent
	// layer's position, so a later camera item in the parent stack supersedes it.
	obs_scene_enum_items(scene, scanCameraFeedItem, &state);
}

struct CameraFeedSourceListContext {
	QComboBox *combo = nullptr;
	QString selectedUuid;
	bool selectedFound = false;
};

bool addCameraFeedSourceToList(void *opaque, obs_source_t *source)
{
	auto *context = static_cast<CameraFeedSourceListContext *>(opaque);
	const char *uuid = obs_source_get_uuid(source);
	const char *name = obs_source_get_name(source);
	if (!uuid || !name)
		return true;
	const QString sourceUuid = QString::fromUtf8(uuid);
	context->combo->addItem(QString::fromUtf8(name), sourceUuid);
	if (sourceUuid == context->selectedUuid) {
		context->combo->setCurrentIndex(context->combo->count() - 1);
		context->selectedFound = true;
	}
	return true;
}

class CameraIndicator final : public QPushButton {
public:
	explicit CameraIndicator(int cameraNumber, QWidget *parent = nullptr) : QPushButton(parent), cameraNumber_(cameraNumber)
	{
		// Keep the existing 33 px button face, but narrow its layout cell so five
		// camera selectors fit closely together without changing their appearance.
		setFixedSize(36, 40);
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
	void setOnAir(bool onAir)
	{
		if (onAir_ == onAir)
			return;
		onAir_ = onAir;
		setToolTip(onAir_ ? QStringLiteral("This camera feed is highest in the visible Program scene stack")
				  : QStringLiteral("Click to change which camera this card controls"));
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
		// Keep the complete rounded square inside the 36x40 layout cell.
		const QRectF indicatorRect(1.5, 1.0, 33.0, 33.0);
		painter.drawRoundedRect(indicatorRect, 5.0, 5.0);
		// Center the 22 px number circle within the 33 px rounded-square face.
		const QRectF circle(7, 6.5, 22, 22);
		painter.setPen(Qt::NoPen);
		painter.setBrush(onAir_ ? (isChecked() ? QColor("#050505") : QColor("#FFFF00"))
					: (isChecked() ? QColor("#050505") : QColor("#D4D7D8")));
		painter.drawEllipse(circle);
		painter.setPen(onAir_ ? (isChecked() ? QColor("#FFFF00") : QColor("#050505"))
				       : (isChecked() ? QColor("#86C9EE") : QColor("#168A3A")));
		QFont font = painter.font();
		font.setBold(true);
		font.setPointSize(11);
		painter.setFont(font);
		painter.drawText(circle, Qt::AlignCenter, QString::number(cameraNumber_));
	}

private:
	int cameraNumber_ = 1;
	bool onAir_ = false;
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
			"[SimpleCamPTZ PresetFadeEvent] id=%llu origin=%s elapsed_ms=%lld target=%s event=%s "
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

class AbsoluteZoomSlider final : public QSlider {
public:
	explicit AbsoluteZoomSlider(QWidget *parent = nullptr) : QSlider(Qt::Vertical, parent) {}

	void setPositionCommitHandler(std::function<void()> handler) { positionCommitHandler_ = std::move(handler); }

protected:
	void mousePressEvent(QMouseEvent *event) override
	{
		valueAtPress_ = value();
		QSlider::mousePressEvent(event);
	}

	void mouseReleaseEvent(QMouseEvent *event) override
	{
		QSlider::mouseReleaseEvent(event);
		if (value() != valueAtPress_ && positionCommitHandler_)
			positionCommitHandler_();
	}

private:
	int valueAtPress_ = 0;
	std::function<void()> positionCommitHandler_;
};

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
	for (int index = 0; index < CameraSlotCount; ++index) {
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

void CameraControlCard::setOnAirCamera(int cameraIndex)
{
	onAirCameraIndex_ = cameraIndex;
	updateCameraTitleStyle();
	for (int index = 0; index < CameraSlotCount; ++index) {
		if (auto *indicator = dynamic_cast<CameraIndicator *>(cameraSelectButtons_[index]))
			indicator->setOnAir(index == cameraIndex);
	}
}

void CameraControlCard::updateCameraTitleStyle()
{
	const bool onAir = onAirCameraIndex_ >= 0 && cameraNumber_ - 1 == onAirCameraIndex_;
	if (!cameraTitle_ || onAir == cameraTitleOnAir_)
		return;
	cameraTitleOnAir_ = onAir;
	cameraTitle_->setStyleSheet(QStringLiteral("color:%1; font-family:'Arial Black'; font-size:20px; font-weight:900;")
				   .arg(onAir ? QStringLiteral("#FFFF00") : QStringLiteral("#46C828")));
}

void CameraControlCard::bindZoomPositionObserver()
{
	QPointer<CameraControlCard> self(this);
	runtime_->setZoomPositionObserver([self](int sliderValue) {
		if (!self || !self->zoomSlider_)
			return;
		const QSignalBlocker blocker(self->zoomSlider_);
		self->zoomSlider_->setValue(sliderValue);
	});
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
	cameraSelectorRow->setSpacing(0);
	cameraSelectorRow->addStretch(1);
	for (int cameraIndex = 0; cameraIndex < CameraSlotCount; ++cameraIndex) {
		cameraSelectButtons_[cameraIndex] = new CameraIndicator(cameraIndex + 1, this);
		cameraSelectButtons_[cameraIndex]->setChecked(cameraIndex + 1 == cameraNumber_);
		cameraSelectorRow->addWidget(cameraSelectButtons_[cameraIndex]);
		QObject::connect(cameraSelectButtons_[cameraIndex], &QPushButton::clicked, this, [this, cameraIndex]() {
			cameraSelectButtons_[cameraIndex]->setChecked(true);
			if (cameraSelectionHandler_)
				cameraSelectionHandler_(cameraIndex);
		});
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
		button->setMinimumSize(90, presetRowHeight_);
		button->setFixedHeight(presetRowHeight_);
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
	// Keep the navigation/speed coordinate system centered on the preset grid.
	navigation->setContentsMargins(0, 0, 0, 0);
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
	auto *absoluteZoomSlider = new AbsoluteZoomSlider(this);
	zoomSlider_ = absoluteZoomSlider;
	zoomSlider_->setRange(0, 200);
	zoomSlider_->setValue(50);
	zoomSlider_->setFixedHeight(105);
	zoomSlider_->setStyleSheet(QStringLiteral("QSlider::groove:vertical { background:white; width:4px; } "
						 "QSlider::handle:vertical { background:white; border:1px solid #777; height:13px; width:12px; margin:0 -4px; }"));
	absoluteZoomSlider->setPositionCommitHandler([this]() { sendZoomPosition(); });
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
	zoomLabels->addWidget(new RotatedZoomLabel(this), 0, Qt::AlignCenter);
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
	bottomFunctionRow_ = cameraFunctionRow;
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
		useBottomRowSizing(button);
		button->setStyleSheet(bottomFunctionStyle(QColor("#3C96C9")));
		cameraFunctionRow->addWidget(button, 1);
	}
	QObject::connect(backlightButton_, &QPushButton::toggled, this, [this, self](bool enabled) {
		backlightButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), enabled) + QStringLiteral(" QPushButton { margin:0px; padding:0px; min-height:24px; }"));
		runtime_->backlight(enabled, [self, enabled](bool ok, const QString &message) {
			if (self)
				self->setStatus(ok ? QStringLiteral("BackLight %1").arg(enabled ? "on" : "off") : message, !ok);
		});
	});
	QObject::connect(exposureButton_, &QPushButton::toggled, this, [this, self](bool enabled) {
		exposureButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), enabled) + QStringLiteral(" QPushButton { margin:0px; padding:0px; min-height:24px; }"));
		runtime_->exposureCompensation(enabled, [self, enabled](bool ok, const QString &message) {
			if (self)
				self->setStatus(ok ? QStringLiteral("Exposure compensation %1").arg(enabled ? "on" : "off") : message, !ok);
		});
	});
	const auto updatePowerStyle = [power](bool cameraOn) {
		power->setText(cameraOn ? QStringLiteral("Off") : QStringLiteral("On"));
		const QColor base = cameraOn ? QColor("#3C96C9") : QColor("#E99A9A");
		power->setStyleSheet(bottomFunctionStyle(base));
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
	updateCameraTitleStyle();
	const bool profileLoaded = runtime_->hasLoadedProfile();
	const int absoluteZoomTemplates = runtime_->zoomAbsoluteTemplateCount();
	const bool zoomPositionAvailable = runtime_->canSetZoomPosition();
	zoomSlider_->setEnabled(zoomPositionAvailable);
	zoomSlider_->setToolTip(zoomPositionAvailable
				 ? QStringLiteral("Uses the calibrated zoom range when available; otherwise uses Sony's 0x4000 optical telephoto endpoint")
				 : QStringLiteral("Select a camera profile with an absolute zoom command to use this slider"));
	const QString zoomDiagnostic =
		QStringLiteral("camera=%1 profileId=\"%2\" profileLoaded=%3 zoomAbsoluteTemplates=%4 canSetZoomPosition=%5 sliderEnabled=%6 loadError=\"%7\"")
			.arg(cameraNumber_)
			.arg(runtime_->profileId())
			.arg(profileLoaded ? 1 : 0)
			.arg(absoluteZoomTemplates)
			.arg(zoomPositionAvailable ? 1 : 0)
			.arg(zoomSlider_->isEnabled() ? 1 : 0)
			.arg(runtime_->profileLoadError());
	if (zoomDiagnostic != lastZoomDiagnostic_) {
		lastZoomDiagnostic_ = zoomDiagnostic;
		const QByteArray diagnosticUtf8 = zoomDiagnostic.toUtf8();
		obs_log(LOG_INFO, "[SimpleCamPTZ ZoomSliderDiag] %s", diagnosticUtf8.constData());
	}
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
	for (auto *button : {manualFocusButton_, focusPlusButton_, focusMinusButton_})
		button->setFixedHeight(26);
	for (auto *button : {backlightButton_, exposureButton_, powerButton_})
		useBottomRowSizing(button);
	manualFocusButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), manualFocusButton_->isChecked()));
	backlightButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), backlightButton_->isChecked()) + QStringLiteral(" QPushButton { margin:0px; padding:0px; min-height:24px; }"));
	exposureButton_->setStyleSheet(toggleButtonStyle(QColor("#3C96C9"), exposureButton_->isChecked()) + QStringLiteral(" QPushButton { margin:0px; padding:0px; min-height:24px; }"));
	pageButton_->setText(QStringLiteral("▲▼"));
	pageButton_->setToolTip(QStringLiteral("Click to cycle preset groups (%1 of %2)").arg(pageIndex + 1).arg(runtime_->presets().pageCount()));

	for (int slotIndex = 0; slotIndex < PresetCatalog::SlotsPerPage; ++slotIndex) {
		auto *button = presetButtons_.at(slotIndex);
		// Keep the actual button geometry identical in Controls and Settings.
		// The settings page editor replaces the navigation block, but must not
		// alter preset row heights or shift the action and function rows.
		button->setFixedHeight(presetRowHeight_);
		const auto &slot = runtime_->presets().slot(pageIndex, slotIndex);
		const bool usable = runtime_->hasUsablePreset(pageIndex, slotIndex);
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
		setMode(Mode::Recall);
		// The flash confirms the user's Set action immediately; it is not an
		// indicator that the camera accepted or completed the command.
		flashPresetButton(slotIndex, "manual-set");
		setStatus(QStringLiteral("Saving current camera position to preset button %1…").arg(slotIndex + 1));
		QPointer<CameraControlCard> self(this);
		runtime_->storeLogicalPreset(pageIndex, slotIndex, [self, slotIndex](bool ok, const QString &message) {
			if (!self)
				return;
			refreshPresetSourceNames();
			self->setStatus(ok ? QStringLiteral("Preset stored") : message, !ok);
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
		if (!runtime_->hasUsablePreset(pageIndex, slotIndex)) {
			setStatus(QStringLiteral("This button has no usable preset mapping for the selected camera profile"), true);
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
			"[SimpleCamPTZ PresetFadeDiag] id=%llu origin=%s stage=%s elapsed_ms=%lld fade_step=%d/%d "
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

void CameraControlCard::sendZoomPosition()
{
	if (!zoomSlider_ || !runtime_)
		return;
	QPointer<CameraControlCard> self(this);
	runtime_->setZoomPosition(zoomSlider_->value(), [self](bool ok, const QString &message) {
		if (self)
			self->setStatus(ok ? QStringLiteral("Zoom position sent") : message, !ok);
	});
}

CameraControlDock::CameraControlDock(const CameraRuntimeArray &runtimes, QWidget *parent)
	: QWidget(parent), runtimes_(runtimes)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	obs_log(LOG_INFO, "[SimpleCamPTZ ResponsiveLayout] build=r9-right-gear-yellow-onair-title");
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 4, 4, 4);
	root->setSpacing(2);
	// OBS owns the dock rectangle. Visible-card minima must not lock the
	// outer dock at two-card width and prevent shrinking back to one card.
	root->setSizeConstraint(QLayout::SetNoConstraint);
	cameraHeadersWidget_ = new QWidget(this);
	cameraHeadersLayout_ = new QHBoxLayout(cameraHeadersWidget_);
	cameraHeadersLayout_->setContentsMargins(0, 0, 0, 0);
	cameraHeadersLayout_->setSpacing(2);
	// Zero-stretch expanding spacers receive only width left over after the
	// card/header widgets reach their maximum; do not cap the OBS dock.
	cameraHeadersLayout_->addStretch(0);
	cameraHeadersLayout_->addStretch(0);
	cameraHeadersWidget_->hide();
	root->addWidget(cameraHeadersWidget_);

	cardsLayout_ = new QHBoxLayout();
	cardsLayout_->setContentsMargins(0, 0, 0, 0);
	cardsLayout_->setSpacing(2);
	cardsLayout_->setAlignment(Qt::AlignTop);
	camera1Card_ = new CameraControlCard(*runtimes_[0], 1, true, this);
	camera2Card_ = new CameraControlCard(*runtimes_[1], 2, true, this);
	camera1Card_->pageDescriptionSuppressed_ = true;
	camera2Card_->pageDescriptionSuppressed_ = true;
	camera1Card_->refresh();
	camera2Card_->refresh();
	camera1Card_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	camera2Card_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	camera1Card_->setMaximumWidth(320);
	camera2Card_->setMaximumWidth(320);
	camera1Card_->cameraHeaderWidget_->setMaximumWidth(320);
	camera2Card_->cameraHeaderWidget_->setMaximumWidth(320);
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
		runtime.setPresetRecallResultObserver(
			[self, runtimePointer](int logicalButton, int pageIndex, CameraRuntime::PresetRecallOrigin origin, bool success) {
				if (!self || !success || origin != CameraRuntime::PresetRecallOrigin::ObsSource ||
				    logicalButton < 1 || logicalButton > PresetCatalog::SlotsPerPage)
					return;
			const QString cameraName = runtimePointer->cameraName();
			const QString presetName = pageIndex >= 0 && pageIndex < runtimePointer->presets().pageCount()
						   ? runtimePointer->presets().presetLabel(pageIndex, logicalButton - 1)
						   : QString();
			const QString displayPreset = presetName.trimmed().isEmpty()
						      ? QStringLiteral("Preset %1").arg(logicalButton)
						      : presetName;
			const QString message = QStringLiteral("%1 moved to %2 by OBS").arg(cameraName, displayPreset);
			for (CameraControlCard *card : {self->camera1Card_, self->camera2Card_}) {
				if (card && card->runtime_ == runtimePointer) {
					card->setStatus(message, false, 5000);
					break;
				}
			}
		});
	};
	for (CameraRuntime *runtime : runtimes_) {
		if (runtime)
			installRecallFeedback(*runtime);
	}
	bindZoomObservers();
	sharedGroupToolsWidget_ = camera1Card_->groupToolsWidget_;
	cardsLayout_->addStretch(0);
	cardsLayout_->addWidget(camera1Card_, 1);
	cardsLayout_->addWidget(camera2Card_, 1);
	cardsLayout_->addStretch(0);
	root->addLayout(cardsLayout_);
	root->addStretch(1);
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
	onAirRefreshTimer_ = new QTimer(this);
	onAirRefreshTimer_->setInterval(250);
	QObject::connect(onAirRefreshTimer_, &QTimer::timeout, this, [this]() { refreshOnAirCamera(); });
	onAirRefreshTimer_->start();
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
	// Put the mode control beside the bottom blue buttons, with no toolbar row.
	settingsButton_ = new BlackSettingsButton(camera1Card_);
	settingsButton_->setFixedWidth(26);
	useBottomRowSizing(settingsButton_);
	settingsButton_->setCheckable(true);
	settingsButton_->setToolTip(QStringLiteral("Manage presets and preset groups"));
	settingsButton_->setAccessibleName(QStringLiteral("Preset settings"));
	settingsButton_->setStyleSheet(bottomFunctionStyle(QColor("#3C96C9")));
	camera1Card_->bottomFunctionRow_->addWidget(settingsButton_);
	// Measure from the real cards (including selectors and the gear), not a
	// guessed pixel width. Never resize the QDockWidget to these hints.
	singleControlWidth_ = camera1Card_->minimumSizeHint().width() + 8;
	settingsWidth_ = 2 * std::max(camera1Card_->minimumSizeHint().width(), camera2Card_->minimumSizeHint().width()) + 10;
	setMinimumWidth(singleControlWidth_);
	camera2Card_->hide();
	updateSharedGroupIndicator();

	auto enterSettingsLayout = [this, root]() {
		auto *card1Layout = qobject_cast<QVBoxLayout *>(camera1Card_->layout());
		auto *card2Layout = qobject_cast<QVBoxLayout *>(camera2Card_->layout());
		card1Layout->removeWidget(camera1Card_->cameraHeaderWidget_);
		card2Layout->removeWidget(camera2Card_->cameraHeaderWidget_);
		camera1Card_->cameraHeaderWidget_->setParent(cameraHeadersWidget_);
		camera2Card_->cameraHeaderWidget_->setParent(cameraHeadersWidget_);
		cameraHeadersLayout_->insertWidget(cameraHeadersLayout_->count() - 1, camera1Card_->cameraHeaderWidget_, 1);
		cameraHeadersLayout_->insertWidget(cameraHeadersLayout_->count() - 1, camera2Card_->cameraHeaderWidget_, 1);
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
		sharedGroupToolsWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		camera1Card_->pageDescriptionEdit_->setMinimumWidth(0);
		root->insertWidget(1, sharedGroupToolsWidget_);
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
		[this, root, enterSettingsLayout, restoreControlLayout]() {
		++layoutTransitionSequence_;
		settingsMode_ = !settingsMode_;
		camera1Card_->setSettingsMode(settingsMode_);
		camera2Card_->setSettingsMode(settingsMode_);
		if (settingsMode_)
			enterSettingsLayout();
		else
			restoreControlLayout();
		settingsButton_->setChecked(settingsMode_);
		settingsButton_->setToolTip(settingsMode_ ? QStringLiteral("Return to camera controls")
						: QStringLiteral("Manage presets and preset groups"));
		cameraHeadersWidget_->setVisible(settingsMode_);
		camera1Card_->cameraHeaderWidget_->show();
		camera2Card_->setVisible(dualLayout_);
		camera2Card_->cameraHeaderWidget_->setVisible(dualLayout_);
		root->invalidate();
		updateResponsiveLayout();
		updateSharedGroupIndicator();
	});
	auto *refreshTimer = new QTimer(this);
	refreshTimer->setInterval(1000);
	QObject::connect(refreshTimer, &QTimer::timeout, this, [this]() {
		camera1Card_->refreshView();
		camera2Card_->refreshView();
		updateSharedGroupIndicator();
	});
	refreshTimer->start();
	QTimer::singleShot(0, this, [this]() { updateResponsiveLayout(); });
}

void CameraControlDock::refreshView()
{
	if (camera1Card_)
		camera1Card_->refreshView();
	if (camera2Card_)
		camera2Card_->refreshView();
	updateSharedGroupIndicator();
	refreshOnAirCamera();
}

void CameraControlDock::bindZoomObservers()
{
	for (CameraRuntime *runtime : runtimes_) {
		if (runtime)
			runtime->setZoomPositionObserver({});
	}
	if (camera1Card_)
		camera1Card_->bindZoomPositionObserver();
	if (camera2Card_)
		camera2Card_->bindZoomPositionObserver();
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
		"[SimpleCamPTZ LayoutDiag] phase=%s transition=%d resize=%d mode=%s dual=%d "
		"dockWidget=%dx%d dockHint=%dx%d dockMin=%dx%d dockMax=%dx%d "
		"dockFloating=%d dockVisible=%d windowActive=%d focus=%s/%s cursor=%d,%d:%s/%s",
		phase, transition, resizeRequest, modeSettings ? "Settings" : "Controls", dualLayout_ ? 1 : 0,
		dock ? dock->width() : -1, dock ? dock->height() : -1, dock ? dock->sizeHint().width() : -1, dock ? dock->sizeHint().height() : -1,
		dock ? dock->minimumWidth() : -1, dock ? dock->minimumHeight() : -1, dock ? dock->maximumWidth() : -1,
		dock ? dock->maximumHeight() : -1, dock && dock->isFloating() ? 1 : 0, dock && dock->isVisible() ? 1 : 0,
		dock && dock->window()->isActiveWindow() ? 1 : 0,
		focusWidget ? focusWidget->metaObject()->className() : "-", focusObjectName.constData(), cursorPosition.x(), cursorPosition.y(),
		cursorWidget ? cursorWidget->metaObject()->className() : "-", cursorObjectName.constData());
	QMainWindow *mainWindow = qobject_cast<QMainWindow *>(dock ? dock->window() : nullptr);
	if (!mainWindow) {
		for (QWidget *parent = dock ? dock->parentWidget() : nullptr; parent && !mainWindow; parent = parent->parentWidget())
			mainWindow = qobject_cast<QMainWindow *>(parent);
	}
	if (mainWindow) {
		const auto globalGeometry = [](QWidget *widget) {
			return widget ? QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size()) : QRect();
		};
		const auto widgetName = [](QWidget *widget) -> QByteArray {
			if (!widget)
				return QByteArrayLiteral("-");
			const QByteArray name = widget->objectName().toUtf8();
			return name.isEmpty() ? QByteArray(widget->metaObject()->className()) : name;
		};
		const QList<QSplitter *> splitters = mainWindow->findChildren<QSplitter *>();
		const QList<QDockWidget *> dockWidgets = mainWindow->findChildren<QDockWidget *>();
		obs_log(LOG_INFO,
			"[SimpleCamPTZ SplitterDiag] phase=%s mainWindow=%s rect=(%d,%d %dx%d) splitters=%d dockWidgets=%d targetDock=%s targetArea=%d",
			phase, mainWindow->metaObject()->className(), globalGeometry(mainWindow).x(), globalGeometry(mainWindow).y(),
			globalGeometry(mainWindow).width(), globalGeometry(mainWindow).height(), splitters.size(), dockWidgets.size(),
			dock ? dock->windowTitle().toUtf8().constData() : "-", dock ? int(mainWindow->dockWidgetArea(dock)) : -1);
		if (splitters.isEmpty()) {
			obs_log(LOG_INFO,
				"[SimpleCamPTZ SplitterDiag] phase=%s no-public-QSplitter-widgets; use QMainWindow dock geometry records below",
				phase);
		}
		for (int splitterIndex = 0; splitterIndex < splitters.size(); ++splitterIndex) {
			QSplitter *splitter = splitters.at(splitterIndex);
			const QRect splitterRect = globalGeometry(splitter);
			const QByteArray splitterName = widgetName(splitter);
			obs_log(LOG_INFO,
				"[SimpleCamPTZ SplitterDiag] phase=%s splitter=%d name=%s orientation=%s rect=(%d,%d %dx%d) count=%d handleWidth=%d visible=%d",
				phase, splitterIndex, splitterName.constData(), splitter->orientation() == Qt::Horizontal ? "H" : "V",
				splitterRect.x(), splitterRect.y(), splitterRect.width(), splitterRect.height(), splitter->count(),
				splitter->handleWidth(), splitter->isVisible() ? 1 : 0);
			for (int handleIndex = 1; handleIndex < splitter->count(); ++handleIndex) {
				QWidget *handle = splitter->handle(handleIndex);
				const QRect handleRect = globalGeometry(handle);
				const QRect beforeRect = globalGeometry(splitter->widget(handleIndex - 1));
				const QRect afterRect = globalGeometry(splitter->widget(handleIndex));
				const QByteArray beforeName = widgetName(splitter->widget(handleIndex - 1));
				const QByteArray afterName = widgetName(splitter->widget(handleIndex));
				obs_log(LOG_INFO,
					"[SimpleCamPTZ SplitterDiag] phase=%s splitter=%d handle=%d handleRect=(%d,%d %dx%d) handleVisible=%d before=%s:(%d,%d %dx%d) after=%s:(%d,%d %dx%d)",
					phase, splitterIndex, handleIndex, handleRect.x(), handleRect.y(), handleRect.width(), handleRect.height(),
					handle && handle->isVisible() ? 1 : 0, beforeName.constData(), beforeRect.x(), beforeRect.y(),
					beforeRect.width(), beforeRect.height(), afterName.constData(), afterRect.x(), afterRect.y(),
					afterRect.width(), afterRect.height());
			}
		}
		for (int dockIndex = 0; dockIndex < dockWidgets.size(); ++dockIndex) {
			QDockWidget *listedDock = dockWidgets.at(dockIndex);
			const QRect dockRect = globalGeometry(listedDock);
			const QByteArray dockObjectName = widgetName(listedDock);
			const QByteArray dockTitle = listedDock->windowTitle().toUtf8();
			obs_log(LOG_INFO,
				"[SimpleCamPTZ SplitterDiag] phase=%s dock=%d object=%s title=%s area=%d rect=(%d,%d %dx%d) visible=%d floating=%d minWidth=%d maxWidth=%d",
				phase, dockIndex, dockObjectName.constData(), dockTitle.constData(), int(mainWindow->dockWidgetArea(listedDock)),
				dockRect.x(), dockRect.y(), dockRect.width(), dockRect.height(), listedDock->isVisible() ? 1 : 0,
				listedDock->isFloating() ? 1 : 0, listedDock->minimumWidth(), listedDock->maximumWidth());
		}
	} else {
		obs_log(LOG_INFO, "[SimpleCamPTZ SplitterDiag] phase=%s main-window-not-found", phase);
	}
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

void CameraControlDock::refreshOnAirCamera()
{
	CameraFeedScanContext context;
	for (int index = 0; index < CameraSlotCount; ++index) {
		if (runtimes_[index])
			context.sourceUuids[index] = runtimes_[index]->cameraFeedSourceUuid();
	}

	obs_source_t *programSource = currentProgramSceneSource();
	if (programSource) {
		if (obs_scene_t *programScene = obs_scene_from_source(programSource))
			scanVisibleCameraFeeds(programScene, context);
		obs_source_release(programSource);
	}

	if (camera1Card_)
		camera1Card_->setOnAirCamera(context.topCameraIndex);
	if (camera2Card_)
		camera2Card_->setOnAirCamera(context.topCameraIndex);
}

QSize CameraControlDock::sizeHint() const
{
	const int preferredWidth = dualLayout_ ? settingsWidth_ : singleControlWidth_;
	const int rowHeight = camera1Card_ ? camera1Card_->presetRowHeight_ : 34;
	const int preferredHeight = layout() ? layout()->minimumSize().height() + 7 * (34 - rowHeight) : 580;
	return QSize(preferredWidth > 0 ? preferredWidth : 212, preferredHeight);
}

QSize CameraControlDock::minimumSizeHint() const
{
	const int rowHeight = camera1Card_ ? camera1Card_->presetRowHeight_ : 34;
	// Allow OBS to allocate enough less height to trigger compression. The
	// visible buttons remain fixed at the chosen height in every layout pass.
	const int compactHeight = layout() ? std::max(0, layout()->minimumSize().height() - 7 * (rowHeight - 26)) : 0;
	return QSize(singleControlWidth_ > 0 ? singleControlWidth_ : 212, compactHeight);
}

void CameraControlDock::updatePresetRowHeight()
{
	if (!layout() || !camera1Card_ || !camera2Card_)
		return;
	// Measure the existing compact layout: only the seven preset rows may
	// contribute to this reduction. Navigation and function rows are untouched.
	for (CameraControlCard *card : {camera1Card_, camera2Card_}) {
		if (card->layout()) {
			card->layout()->invalidate();
			card->layout()->activate();
		}
	}
	layout()->invalidate();
	const int currentRowHeight = camera1Card_->presetRowHeight_;
	const int fullHeight = layout()->minimumSize().height() + 7 * (34 - currentRowHeight);
	const int shortage = std::max(0, fullHeight - height());
	const int nextRowHeight = std::clamp(34 - (shortage + 6) / 7, 26, 34);
	if (nextRowHeight == currentRowHeight)
		return;
	for (CameraControlCard *card : {camera1Card_, camera2Card_}) {
		card->presetRowHeight_ = nextRowHeight;
		for (QPushButton *button : card->presetButtons_)
			button->setFixedHeight(nextRowHeight);
		if (card->layout()) {
			card->layout()->invalidate();
			card->layout()->activate();
		}
		card->updateGeometry();
	}
	layout()->invalidate();
	layout()->activate();
	updateGeometry();
	obs_log(LOG_INFO,
		"[SimpleCamPTZ PresetHeight] available=%d fullHeight=%d shortage=%d row=%d previous=%d cards=%d mode=%s",
		height(), fullHeight, shortage, nextRowHeight, currentRowHeight, dualLayout_ ? 2 : 1,
		settingsMode_ ? "Settings" : "Controls");
}

void CameraControlDock::updateResponsiveLayout()
{
	if (responsiveLayoutPending_)
		return;
	responsiveLayoutPending_ = true;
	QTimer::singleShot(0, this, [this]() {
		responsiveLayoutPending_ = false;
		bindFloatingDock();
		// No upper hysteresis margin: floating maximum is exactly the compact
		// two-card width. Never show two cards below their measured minimum.
		const int threshold = settingsWidth_;
		const bool nextDual = width() >= threshold;
		if (nextDual != dualLayout_) {
			dualLayout_ = nextDual;
			moveSettingsGear();
			camera2Card_->setVisible(dualLayout_);
			camera2Card_->cameraHeaderWidget_->setVisible(dualLayout_);
			if (layout())
				layout()->invalidate();
			obs_log(LOG_INFO, "[SimpleCamPTZ ResponsiveLayout] width=%d threshold=%d cards=%d mode=%s",
				width(), threshold, dualLayout_ ? 2 : 1, settingsMode_ ? "Settings" : "Controls");
			updateGeometry();
		}
		// The one shared Settings editor follows the width of the centered
		// cards. QVBoxLayout centers a widget narrower than its allocated cell.
		sharedGroupToolsWidget_->setMaximumWidth(dualLayout_ ? 642 : 320);
		updatePresetRowHeight();
		updateFloatingLimits();
		positionSharedGroupIndicator();
	});
}

void CameraControlDock::moveSettingsGear()
{
	CameraControlCard *target = dualLayout_ ? camera2Card_ : camera1Card_;
	if (!settingsButton_ || !target || settingsButton_->parentWidget() == target)
		return;
	for (CameraControlCard *card : {camera1Card_, camera2Card_}) {
		if (card && card->bottomFunctionRow_)
			card->bottomFunctionRow_->removeWidget(settingsButton_);
	}
	settingsButton_->setParent(target);
	target->bottomFunctionRow_->addWidget(settingsButton_);
	// setParent hides the widget; explicitly restore it after the transfer.
	settingsButton_->show();
	for (CameraControlCard *card : {camera1Card_, camera2Card_}) {
		if (card && card->layout())
			card->layout()->invalidate();
	}
}

void CameraControlDock::bindFloatingDock()
{
	QWidget *ancestor = parentWidget();
	while (ancestor && !qobject_cast<QDockWidget *>(ancestor))
		ancestor = ancestor->parentWidget();
	auto *dock = qobject_cast<QDockWidget *>(ancestor);
	if (!dock || floatingDock_ == dock)
		return;
	QObject::disconnect(floatingConnection_);
	floatingDock_ = dock;
	floatingConnection_ = QObject::connect(dock, &QDockWidget::topLevelChanged, this, [this](bool floating) {
		if (!floating && floatingDock_) {
			// Release synchronously, before the docked layout settles. No forced
			// resize, separator style override or fixed dock dimensions.
			floatingDock_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
			obs_log(LOG_INFO, "[SimpleCamPTZ FloatingLimits] docked maximum=released");
		}
		updateResponsiveLayout();
	});
}

void CameraControlDock::updateFloatingLimits()
{
	if (!floatingDock_ || !layout())
		return;
	const int rowHeight = camera1Card_->presetRowHeight_;
	const int fullContentHeight = layout()->minimumSize().height() + 7 * (34 - rowHeight);
	if (!settingsMode_)
		fullControlHeight_ = std::max(fullControlHeight_, fullContentHeight);
	if (!floatingDock_->isFloating()) {
		if (floatingDock_->maximumSize() != QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX))
			floatingDock_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
		return;
	}
	// QDockWidget maximumSize governs its client rectangle. Add only the
	// measured dock-to-content inset; native OS window borders remain external.
	const int insetWidth = std::max(0, floatingDock_->width() - width());
	const int insetHeight = std::max(0, floatingDock_->height() - height());
	const QSize minimum = floatingDock_->minimumSizeHint().expandedTo(floatingDock_->minimumSize());
	const QSize maximum(std::max(minimum.width(), settingsWidth_ + insetWidth),
			    std::max(minimum.height(), (fullControlHeight_ > 0 ? fullControlHeight_ : fullContentHeight) + insetHeight));
	if (floatingDock_->maximumSize() == maximum)
		return;
	obs_log(LOG_INFO, "[SimpleCamPTZ FloatingLimits] floating max=%dx%d contentDualWidth=%d fullControlHeight=%d inset=%d,%d",
		maximum.width(), maximum.height(), settingsWidth_, fullControlHeight_, insetWidth, insetHeight);
	floatingDock_->setMaximumSize(maximum);
}

void CameraControlDock::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	bindFloatingDock();
	updateResponsiveLayout();
}

void CameraControlDock::positionSharedGroupIndicator()
{
	if (!sharedGroupIndicator_ || !camera1Card_ || settingsMode_)
		return;
	auto *leftLabel = camera1Card_->pageDescription_;
	auto *rightLabel = dualLayout_ && !camera2Card_->isHidden() ? camera2Card_->pageDescription_ : leftLabel;
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
	updateResponsiveLayout();
}

void CameraControlDock::selectCameraForCard(int cardIndex, int cameraIndex)
{
	if (cardIndex < 0 || cardIndex > 1 || cameraIndex < 0 || cameraIndex >= CameraSlotCount || !runtimes_[cameraIndex])
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
	bindZoomObservers();
	updateSharedGroupIndicator();
}

CamerasTool::CamerasTool(const CameraRuntimeArray &runtimes, const QString &builtInProfileDirectory,
			 const QString &userProfileDirectory, std::function<void()> refreshControls, QWidget *parent)
	: QDialog(parent), runtimes_(runtimes), builtInProfileDirectory_(builtInProfileDirectory),
	  userProfileDirectory_(userProfileDirectory), refreshControls_(std::move(refreshControls))
{
	setWindowTitle(QStringLiteral("SimpleCamPTZ - Cameras"));
	setMinimumSize(460, 300);
	resize(560, 380);
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(10, 10, 10, 10);
	auto *form = new QFormLayout();
	cameraSelector_ = new QComboBox(this);
	for (int index = 0; index < CameraSlotCount; ++index)
		cameraSelector_->addItem(runtimes_[index]->cameraName());
	cameraFeedSourceSelector_ = new QComboBox(this);
	cameraFeedSourceSelector_->setToolTip(QStringLiteral(
		"Select this camera's raw video input source. The on-air marker follows this source through visible scenes and groups."));
	auto *feedSourceRow = new QWidget(this);
	auto *feedSourceRowLayout = new QHBoxLayout(feedSourceRow);
	feedSourceRowLayout->setContentsMargins(0, 0, 0, 0);
	auto *refreshFeedSourcesButton = new QPushButton(QStringLiteral("Refresh"), feedSourceRow);
	feedSourceRowLayout->addWidget(cameraFeedSourceSelector_, 1);
	feedSourceRowLayout->addWidget(refreshFeedSourcesButton);
	profileSelector_ = new QComboBox(this);
	cameraNameEdit_ = new QLineEdit(runtimes_[0]->cameraName(), this);
	cameraNameEdit_->setMaxLength(40);
	hostEdit_ = new QLineEdit(runtimes_[0]->host(), this);
	hostEdit_->setPlaceholderText(QStringLiteral("Camera IP address"));
	portSpin_ = new QSpinBox(this);
	portSpin_->setRange(1, 65535);
	portSpin_->setValue(runtimes_[0]->port());
	addressSpin_ = new QSpinBox(this);
	addressSpin_->setRange(1, 7);
	addressSpin_->setValue(runtimes_[0]->cameraAddress());
	capacityLabel_ = new QLabel(this);
	capacityLabel_->setToolTip(QStringLiteral("Preset capacity is defined by the selected camera profile."));
	auto *profileRow = new QWidget(this);
	auto *profileRowLayout = new QHBoxLayout(profileRow);
	profileRowLayout->setContentsMargins(0, 0, 0, 0);
	auto *refreshProfilesButton = new QPushButton(QStringLiteral("Refresh"), profileRow);
	refreshProfilesButton->setToolTip(QStringLiteral("Rescan built-in and saved camera profiles."));
	profileRowLayout->addWidget(profileSelector_, 1);
	profileRowLayout->addWidget(refreshProfilesButton);
	form->addRow(QStringLiteral("Camera"), cameraSelector_);
	form->addRow(QStringLiteral("Raw camera feed"), feedSourceRow);
	form->addRow(QStringLiteral("Camera profile"), profileRow);
	form->addRow(QStringLiteral("Camera name"), cameraNameEdit_);
	form->addRow(QStringLiteral("Network IP"), hostEdit_);
	form->addRow(QStringLiteral("TCP port"), portSpin_);
	form->addRow(QStringLiteral("VISCA address"), addressSpin_);
	form->addRow(QStringLiteral("Preset capacity"), capacityLabel_);
	root->addLayout(form);

	auto *testButton = new QPushButton(QStringLiteral("Test TCP connection"), this);
	statusLabel_ = new QLabel(QStringLiteral("Enter the camera address. The test sends no PTZ command."), this);
	statusLabel_->setWordWrap(true);
	root->addWidget(testButton);
	root->addWidget(statusLabel_);
	root->addStretch(1);

	QObject::connect(cameraSelector_, qOverload<int>(&QComboBox::currentIndexChanged), this,
			 [this](int) { loadSelectedCamera(); });
	QObject::connect(profileSelector_, qOverload<int>(&QComboBox::currentIndexChanged), this,
			 [this](int index) {
				if (index < 0)
					return;
				CameraRuntime &selected = *runtimes_[cameraSelector_->currentIndex()];
				const QString path = profileSelector_->itemData(index).toString();
				if (path.isEmpty() || path == selected.profilePath())
					return;
				QString error;
				if (!selected.setProfilePath(path, &error)) {
					statusLabel_->setText(error);
					loadSelectedCamera();
					return;
				}
				updateProfileSummary(selected);
				statusLabel_->setText(QStringLiteral("Using %1 for %2").arg(selected.profileDisplayName(), selected.cameraName()));
				refreshPresetSourceNames();
				if (refreshControls_)
					refreshControls_();
			});
	QObject::connect(refreshProfilesButton, &QPushButton::clicked, this, [this]() { refreshProfileList(); });
	QObject::connect(refreshFeedSourcesButton, &QPushButton::clicked, this,
			 [this]() { refreshCameraFeedSourceList(); });
	QObject::connect(cameraFeedSourceSelector_, qOverload<int>(&QComboBox::currentIndexChanged), this,
			 [this](int index) {
				if (index < 0 || !cameraSelector_ || cameraSelector_->currentIndex() < 0)
					return;
				CameraRuntime &selected = *runtimes_[cameraSelector_->currentIndex()];
				selected.setCameraFeedSourceUuid(cameraFeedSourceSelector_->itemData(index).toString());
				statusLabel_->setText(cameraFeedSourceSelector_->itemData(index).toString().isEmpty()
						      ? QStringLiteral("On-air camera marker disabled for %1").arg(selected.cameraName())
						      : QStringLiteral("Raw feed mapped for %1").arg(selected.cameraName()));
				if (refreshControls_)
					refreshControls_();
			});
	QObject::connect(cameraNameEdit_, &QLineEdit::editingFinished, this,
			 [this]() {
				CameraRuntime &selected = *runtimes_[cameraSelector_->currentIndex()];
				selected.setCameraName(cameraNameEdit_->text());
				cameraSelector_->setItemText(cameraSelector_->currentIndex(), cameraNameEdit_->text());
				refreshPresetSourceNames();
			});
	QObject::connect(hostEdit_, &QLineEdit::editingFinished, this,
			 [this]() { runtimes_[cameraSelector_->currentIndex()]->setHost(hostEdit_->text()); });
	QObject::connect(portSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
			 [this](int port) { runtimes_[cameraSelector_->currentIndex()]->setPort(static_cast<quint16>(port)); });
	QObject::connect(addressSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
			 [this](int address) { runtimes_[cameraSelector_->currentIndex()]->setCameraAddress(address); });
	QPointer<CamerasTool> self(this);
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
	refreshProfileList();
}

void CamerasTool::refreshProfileList()
{
	const QString selectedPath = runtimes_[cameraSelector_ ? cameraSelector_->currentIndex() : 0]->profilePath();
	const QSignalBlocker blocker(profileSelector_);
	profileSelector_->clear();
	QSet<QString> seen;
	auto addDirectory = [this, &seen](const QString &directoryPath) {
		if (directoryPath.isEmpty())
			return;
		const QDir directory(directoryPath);
		const QFileInfoList files = directory.entryInfoList(
			QStringList{QStringLiteral("*.camera.cfg"), QStringLiteral("*.json")}, QDir::Files | QDir::Readable,
			QDir::Name);
		for (const QFileInfo &file : files) {
			const QString path = file.absoluteFilePath();
			const QString key = file.canonicalFilePath().isEmpty() ? path : file.canonicalFilePath();
			if (seen.contains(key))
				continue;
			CameraProfileDocument profile;
			QString error;
			if (!profile.load(path, &error) || !profile.supportsRawViscaTcp() || profile.presetCapacity() < 1)
				continue;
			const int index = profileSelector_->count();
			profileSelector_->addItem(profile.displayName(), path);
			const QString status = profile.root().value(QStringLiteral("support")).toObject()
					       .value(QStringLiteral("status")).toString();
			profileSelector_->setItemData(index,
				QStringLiteral("%1 · %2\n%3").arg(profile.id()).arg(status).arg(path), Qt::ToolTipRole);
			seen.insert(key);
		}
	};
	addDirectory(builtInProfileDirectory_);
	addDirectory(userProfileDirectory_);
	if (!selectedPath.isEmpty()) {
		int selectedIndex = -1;
		for (int index = 0; index < profileSelector_->count(); ++index) {
			if (QFileInfo(profileSelector_->itemData(index).toString()).canonicalFilePath() ==
			    QFileInfo(selectedPath).canonicalFilePath()) {
				selectedIndex = index;
				break;
			}
		}
		if (selectedIndex < 0) {
			profileSelector_->addItem(QStringLiteral("Unavailable: %1").arg(QFileInfo(selectedPath).fileName()), selectedPath);
			selectedIndex = profileSelector_->count() - 1;
			profileSelector_->setItemData(selectedIndex, selectedPath, Qt::ToolTipRole);
		}
		profileSelector_->setCurrentIndex(selectedIndex);
	}
	loadSelectedCamera();
}

void CamerasTool::loadSelectedCamera()
{
	if (!cameraSelector_ || cameraSelector_->currentIndex() < 0)
		return;
	CameraRuntime &selected = *runtimes_[cameraSelector_->currentIndex()];
	const QSignalBlocker blockPort(portSpin_);
	const QSignalBlocker blockAddress(addressSpin_);
	const QSignalBlocker blockProfile(profileSelector_);
	cameraNameEdit_->setText(selected.cameraName());
	hostEdit_->setText(selected.host());
	portSpin_->setValue(selected.port());
	addressSpin_->setValue(selected.cameraAddress());
	refreshCameraFeedSourceList();
	int profileIndex = -1;
	for (int index = 0; index < profileSelector_->count(); ++index) {
		if (profileSelector_->itemData(index).toString() == selected.profilePath()) {
			profileIndex = index;
			break;
		}
	}
	if (profileIndex < 0 && !selected.profilePath().isEmpty()) {
		profileSelector_->addItem(QStringLiteral("Unavailable: %1").arg(QFileInfo(selected.profilePath()).fileName()),
					  selected.profilePath());
		profileIndex = profileSelector_->count() - 1;
	}
	profileSelector_->setCurrentIndex(profileIndex);
	updateProfileSummary(selected);
	statusLabel_->setText(selected.hasLoadedProfile() ? QStringLiteral("Settings for %1 · %2 presets")
								  .arg(selected.cameraName()).arg(selected.capacity())
								  : selected.profileLoadError());
}

void CamerasTool::refreshCameraFeedSourceList()
{
	if (!cameraFeedSourceSelector_ || !cameraSelector_ || cameraSelector_->currentIndex() < 0)
		return;
	const QString selectedUuid = runtimes_[cameraSelector_->currentIndex()]->cameraFeedSourceUuid();
	const QSignalBlocker blocker(cameraFeedSourceSelector_);
	cameraFeedSourceSelector_->clear();
	cameraFeedSourceSelector_->addItem(QStringLiteral("Not configured"), QString());
	CameraFeedSourceListContext context{cameraFeedSourceSelector_, selectedUuid, false};
	obs_enum_sources(addCameraFeedSourceToList, &context);
	if (!selectedUuid.isEmpty() && !context.selectedFound) {
		cameraFeedSourceSelector_->addItem(QStringLiteral("Unavailable source (not found in OBS)"), selectedUuid);
		cameraFeedSourceSelector_->setCurrentIndex(cameraFeedSourceSelector_->count() - 1);
		cameraFeedSourceSelector_->setItemData(cameraFeedSourceSelector_->currentIndex(), selectedUuid, Qt::ToolTipRole);
	} else if (selectedUuid.isEmpty()) {
		cameraFeedSourceSelector_->setCurrentIndex(0);
	}
}

void CamerasTool::updateProfileSummary(CameraRuntime &runtime)
{
	capacityLabel_->setText(runtime.hasLoadedProfile()
				 ? QStringLiteral("%1 presets").arg(runtime.capacity())
				 : QStringLiteral("Unavailable"));
	capacityLabel_->setToolTip(runtime.hasLoadedProfile()
					   ? QStringLiteral("Capacity from %1").arg(runtime.profileDisplayName())
					   : runtime.profileLoadError());
}

} // namespace simplecam
