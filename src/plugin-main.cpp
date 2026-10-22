#include "camera_docks.hpp"
#include "camera_runtime.hpp"
#include "preset_source.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/base.h>

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <array>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr char kCameraControlDockTitle[] = "Camera Control v0.1.0-layoutfix1";

std::array<simplecam::CameraRuntime *, 2> runtimes{};
QWidget *cameraControlWidget = nullptr;
QWidget *camerasWidget = nullptr;
QPointer<QTimer> sourceNameRefreshTimer;

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (!runtimes[0] || !runtimes[1])
		return;
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		obs_source_t *scene = obs_frontend_get_current_scene();
		const int trigger = obs_frontend_preview_program_mode_active() ? 0 : 3;
		simplecam::dispatchScenePresets(scene, trigger);
		if (scene)
			obs_source_release(scene);
	} else if (event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED) {
		obs_source_t *scene = obs_frontend_get_current_preview_scene();
		simplecam::dispatchScenePresets(scene, 1);
		if (scene)
			obs_source_release(scene);
	} else if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		obs_source_t *program = obs_frontend_get_current_scene();
		obs_source_t *preview = obs_frontend_get_current_preview_scene();
		if (preview && preview != program) {
			simplecam::dispatchScenePresets(program, 0);
			simplecam::dispatchScenePresets(preview, 1);
		} else {
			simplecam::dispatchScenePresets(program, 3);
		}
		if (program)
			obs_source_release(program);
		if (preview)
			obs_source_release(preview);
	} else if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED ||
		   event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED ||
		   event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		simplecam::refreshPresetSourceNames();
	}
}

} // namespace

bool obs_module_load(void)
{
	char *configPath = obs_module_config_path("simplecamdirect.ini");
	if (!configPath) {
		obs_log(LOG_ERROR, "SimpleCam Direct could not resolve its settings path");
		return false;
	}
	const QString settingsPath = QString::fromUtf8(configPath);
	bfree(configPath);
	QDir().mkpath(QFileInfo(settingsPath).absolutePath());

	runtimes[0] = new simplecam::CameraRuntime(settingsPath, QStringLiteral("Camera 1"));
	const QString secondSettingsPath = QFileInfo(settingsPath).dir().filePath(QStringLiteral("simplecamdirect-camera2.ini"));
	runtimes[1] = new simplecam::CameraRuntime(secondSettingsPath, QStringLiteral("Camera 2"));
	runtimes[1]->setSharedPresetOwner(*runtimes[0]);
	simplecam::setCameraRuntime(0, runtimes[0]);
	simplecam::setCameraRuntime(1, runtimes[1]);
	simplecam::registerPresetSource();

	cameraControlWidget = new simplecam::CameraControlDock(*runtimes[0], *runtimes[1]);
	camerasWidget = new simplecam::CamerasDock(*runtimes[0], *runtimes[1]);
	const bool controlAdded = obs_frontend_add_dock_by_id("simplecamdirect.camera-control", kCameraControlDockTitle, cameraControlWidget);
	const bool camerasAdded = obs_frontend_add_dock_by_id("simplecamdirect.cameras", "Cameras", camerasWidget);
	if (!controlAdded || !camerasAdded) {
		obs_log(LOG_ERROR, "SimpleCam Direct could not add its OBS dock widgets");
		if (controlAdded)
			obs_frontend_remove_dock("simplecamdirect.camera-control");
		if (camerasAdded)
			obs_frontend_remove_dock("simplecamdirect.cameras");
		cameraControlWidget = nullptr;
		camerasWidget = nullptr;
		delete runtimes[1];
		delete runtimes[0];
		runtimes = {};
		simplecam::setCameraRuntime(0, nullptr);
		simplecam::setCameraRuntime(1, nullptr);
		return false;
	}
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	sourceNameRefreshTimer = new QTimer(cameraControlWidget);
	sourceNameRefreshTimer->setInterval(1000);
	QObject::connect(sourceNameRefreshTimer.data(), &QTimer::timeout,
			 []() { simplecam::refreshPresetSourceNames(); });
	sourceNameRefreshTimer->start();
	obs_log(LOG_INFO, "SimpleCam Direct %s loaded", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	// The timer is parented to the control dock. OBS may already have destroyed
	// that dock during shutdown, so let QObject ownership destroy the timer and
	// use QPointer to avoid a stale global pointer.
	if (sourceNameRefreshTimer)
		sourceNameRefreshTimer->stop();
	sourceNameRefreshTimer = nullptr;
	// Remove the UI objects before destroying the runtimes they reference.
	obs_frontend_remove_dock("simplecamdirect.camera-control");
	obs_frontend_remove_dock("simplecamdirect.cameras");
	cameraControlWidget = nullptr;
	camerasWidget = nullptr;
	simplecam::setCameraRuntime(0, nullptr);
	simplecam::setCameraRuntime(1, nullptr);
	delete runtimes[1];
	delete runtimes[0];
	runtimes = {};
	obs_log(LOG_INFO, "SimpleCam Direct unloaded");
}
