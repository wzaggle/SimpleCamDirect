#include "mac_dock_support.hpp"
#include "camera_docks.hpp"

#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

namespace simplecam {
namespace {

class DockTitleBar final : public QWidget {
public:
	explicit DockTitleBar(QDockWidget *dock) : QWidget(dock)
	{
		auto *row = new QHBoxLayout(this);
		row->setContentsMargins(6, 2, 2, 2);
		row->setSpacing(3);
		auto *label = new QLabel(dock->windowTitle(), this);
		label->setAttribute(Qt::WA_TransparentForMouseEvents);
		row->addWidget(label, 1);
		auto *floatButton = new QToolButton(this);
		floatButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
		floatButton->setToolTip(QStringLiteral("Dock / undock"));
		auto *closeButton = new QToolButton(this);
		closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
		closeButton->setToolTip(QStringLiteral("Close dock"));
		row->addWidget(floatButton);
		row->addWidget(closeButton);
		QObject::connect(dock, &QDockWidget::windowTitleChanged, label, &QLabel::setText);
		QObject::connect(floatButton, &QToolButton::clicked, dock, [dock]() { dock->setFloating(!dock->isFloating()); });
		QObject::connect(closeButton, &QToolButton::clicked, dock, &QDockWidget::close);
		auto syncFeatures = [dock, floatButton, closeButton]() {
			floatButton->setEnabled(dock->features().testFlag(QDockWidget::DockWidgetMovable) &&
						dock->features().testFlag(QDockWidget::DockWidgetFloatable));
			closeButton->setVisible(dock->features().testFlag(QDockWidget::DockWidgetClosable));
		};
		QObject::connect(dock, &QDockWidget::featuresChanged, this, syncFeatures);
		syncFeatures();
	}
protected:
	// Unhandled title-bar mouse events must reach QDockWidget's drag handler.
	void mousePressEvent(QMouseEvent *event) override { event->ignore(); }
	void mouseMoveEvent(QMouseEvent *event) override { event->ignore(); }
	void mouseReleaseEvent(QMouseEvent *event) override { event->ignore(); }
	void mouseDoubleClickEvent(QMouseEvent *event) override { event->ignore(); }
};

class MacDockSupport final : public QObject {
public:
	MacDockSupport(CameraControlDock *controls, QDockWidget *dock, const QString &settingsPath)
		: QObject(controls), controls_(controls), dock_(dock), settingsPath_(settingsPath)
	{
		main_ = qobject_cast<QMainWindow *>(dock->parentWidget());
		clock_.start();
		dock->installEventFilter(this);
		// Start this diagnostic with outer limits released. The menu permits
		// an A/B test against the previously accepted Windows-style limits.
		dock->setProperty("simplecamptz.macRelaxFloatingLimits", true);
		dock->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
		auto *menu = new QMenu(controls);
		rootAction_ = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction("SimpleCamPTZ Mac Dock Diagnostics"));
		if (rootAction_)
			rootAction_->setMenu(menu);
		auto *dockAction = menu->addAction(QStringLiteral("Dock into OBS"));
		QObject::connect(dockAction, &QAction::triggered, this, [this]() { dockIntoObs(false); });
		auto *releaseLimits = menu->addAction(QStringLiteral("Release floating size limits"));
		releaseLimits->setCheckable(true);
		releaseLimits->setChecked(true);
		QObject::connect(releaseLimits, &QAction::toggled, this, [this](bool enabled) {
			if (!dock_ || !controls_) return;
			dock_->setProperty("simplecamptz.macRelaxFloatingLimits", enabled);
			if (enabled) dock_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
			controls_->refreshView();
			log("limits-toggled");
		});
		auto *qtTitle = menu->addAction(QStringLiteral("Use Qt title bar"));
		qtTitle->setCheckable(true);
		QObject::connect(qtTitle, &QAction::toggled, this, [this](bool enabled) {
			if (!dock_) return;
			QWidget *previous = dock_->titleBarWidget();
			dock_->setTitleBarWidget(enabled ? new DockTitleBar(dock_) : nullptr);
			if (previous) previous->deleteLater();
			log("titlebar-toggled");
		});
		QObject::connect(menu->addAction(QStringLiteral("Log dock state")), &QAction::triggered, this, [this]() { log("manual"); });
		QObject::connect(dock, &QDockWidget::topLevelChanged, this, [this](bool) { log("top-level-changed"); });
		QObject::connect(dock, &QDockWidget::dockLocationChanged, this, [this](Qt::DockWidgetArea) { log("dock-location-changed"); });
		QObject::connect(dock, &QDockWidget::featuresChanged, this, [this](QDockWidget::DockWidgetFeatures) { log("features-changed"); });
		QObject::connect(dock, &QDockWidget::allowedAreasChanged, this, [this](Qt::DockWidgetAreas) { log("allowed-areas-changed"); });
		log("installed");
	}

	~MacDockSupport() override
	{
		if (dock_) dock_->removeEventFilter(this);
		if (rootAction_) delete rootAction_.data();
	}

	void finishedLoading()
	{
		if (startupScheduled_) return;
		startupScheduled_ = true;
		// Run after OBS's saved layout restoration. Position once for r17;
		// subsequent launches must retain the operator's saved dock layout.
		QTimer::singleShot(0, this, [this]() {
			QSettings settings(settingsPath_, QSettings::IniFormat);
			if (!settings.value(QStringLiteral("ui/macDockR17Placed"), false).toBool())
				dockIntoObs(true);
			else
				log("saved-layout-retained");
		});
	}

private:
	void dockIntoObs(bool firstRun)
	{
		if (!dock_ || !main_) return;
		log("before-dock-request");
		if (!dock_->features().testFlag(QDockWidget::DockWidgetMovable)) {
			log("dock-request-blocked-by-lock");
			if (!firstRun)
				QMessageBox::information(controls_, QStringLiteral("Dock locked"),
					QStringLiteral("Uncheck Docks → Lock Docks in OBS, then try again."));
			return;
		}
		// Do not change features/allowed areas or force a layout width.
		dock_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
		if (main_->dockWidgetArea(dock_) == Qt::NoDockWidgetArea) {
			if (!dock_->isAreaAllowed(Qt::RightDockWidgetArea)) {
				log("right-area-not-allowed");
				return;
			}
			main_->addDockWidget(Qt::RightDockWidgetArea, dock_);
		}
		dock_->setFloating(false);
		dock_->show();
		dock_->raise();
		if (firstRun && !dock_->isFloating()) {
			QSettings settings(settingsPath_, QSettings::IniFormat);
			settings.setValue(QStringLiteral("ui/macDockR17Placed"), true);
		}
		log("after-dock-request");
		QTimer::singleShot(100, this, [this]() { log("dock-request-settled"); });
	}

	void log(const char *stage)
	{
		if (!dock_) return;
		const QByteArray parent = dock_->parentWidget() ? dock_->parentWidget()->metaObject()->className() : "none";
		obs_log(LOG_INFO,
			"[SimpleCamPTZ MacDockDiag] build=r17 stage=%s parent=%s main=%d floating=%d area=%d allowed=%u features=%u "
			"flags=%llu nativeAttribute=%d qtTitle=%d relaxed=%d size=%dx%d min=%dx%d max=%dx%d buttons=%u modifiers=%u",
			stage, parent.constData(), main_ ? 1 : 0, dock_->isFloating() ? 1 : 0,
			main_ ? static_cast<int>(main_->dockWidgetArea(dock_)) : -1,
			static_cast<unsigned>(dock_->allowedAreas().toInt()), static_cast<unsigned>(dock_->features().toInt()),
			static_cast<unsigned long long>(dock_->windowFlags().toInt()), dock_->testAttribute(Qt::WA_NativeWindow) ? 1 : 0,
			dock_->titleBarWidget() ? 1 : 0, dock_->property("simplecamptz.macRelaxFloatingLimits").toBool() ? 1 : 0,
			dock_->width(), dock_->height(), dock_->minimumWidth(), dock_->minimumHeight(), dock_->maximumWidth(), dock_->maximumHeight(),
			static_cast<unsigned>(QApplication::mouseButtons().toInt()), static_cast<unsigned>(QApplication::keyboardModifiers().toInt()));
	}

	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (watched == dock_) {
			const char *stage = nullptr;
			switch (event->type()) {
			case QEvent::NonClientAreaMouseButtonPress: stage = "native-press"; break;
			case QEvent::NonClientAreaMouseButtonRelease: stage = "native-release"; break;
			case QEvent::MouseButtonPress: stage = "qt-press"; break;
			case QEvent::MouseButtonRelease: stage = "qt-release"; break;
			case QEvent::Show: stage = "show"; break;
			case QEvent::ParentChange: stage = "parent-change"; break;
			case QEvent::WinIdChange: stage = "native-id-change"; break;
			case QEvent::Move:
			case QEvent::NonClientAreaMouseMove:
				if (QApplication::mouseButtons().testFlag(Qt::LeftButton) && clock_.elapsed() - lastMoveLog_ >= 100) {
					lastMoveLog_ = clock_.elapsed();
					stage = "drag-move";
				}
				break;
			default: break;
			}
			if (stage) log(stage);
		}
		return QObject::eventFilter(watched, event);
	}

	QPointer<CameraControlDock> controls_;
	QPointer<QDockWidget> dock_;
	QPointer<QMainWindow> main_;
	QPointer<QAction> rootAction_;
	QString settingsPath_;
	QElapsedTimer clock_;
	qint64 lastMoveLog_ = -100;
	bool startupScheduled_ = false;
};

QPointer<MacDockSupport> support;
} // namespace

void installMacDockSupport(CameraControlDock *controls, const QString &settingsPath)
{
	QWidget *ancestor = controls->parentWidget();
	while (ancestor && !qobject_cast<QDockWidget *>(ancestor)) ancestor = ancestor->parentWidget();
	if (auto *dock = qobject_cast<QDockWidget *>(ancestor))
		support = new MacDockSupport(controls, dock, settingsPath);
}

void finishMacDockStartup()
{
	if (support) support->finishedLoading();
}

void shutdownMacDockSupport()
{
	if (support) delete support.data();
	support = nullptr;
}
} // namespace simplecam
