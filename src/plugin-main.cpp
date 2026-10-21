#include "camera_docks.hpp"
#include "camera_runtime.hpp"
#include "preset_source.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/base.h>

#include <QDir>
#include <QFileInfo>
#include <QWidget>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

simplecam::CameraRuntime *runtime = nullptr;
QWidget *cameraControlWidget = nullptr;
QWidget *camerasWidget = nullptr;

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (!runtime)
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

	runtime = new simplecam::CameraRuntime(settingsPath);
	simplecam::setCameraRuntime(runtime);
	simplecam::registerPresetSource();

	cameraControlWidget = new simplecam::CameraControlDock(*runtime);
	camerasWidget = new simplecam::CamerasDock(*runtime);
	const bool controlAdded = obs_frontend_add_dock_by_id("simplecamdirect.camera-control", "Camera Control", cameraControlWidget);
	const bool camerasAdded = obs_frontend_add_dock_by_id("simplecamdirect.cameras", "Cameras", camerasWidget);
	if (!controlAdded || !camerasAdded) {
		obs_log(LOG_ERROR, "SimpleCam Direct could not add its OBS dock widgets");
		if (controlAdded)
			obs_frontend_remove_dock("simplecamdirect.camera-control");
		if (camerasAdded)
			obs_frontend_remove_dock("simplecamdirect.cameras");
		cameraControlWidget = nullptr;
		camerasWidget = nullptr;
		delete runtime;
		runtime = nullptr;
		simplecam::setCameraRuntime(nullptr);
		return false;
	}
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	obs_log(LOG_INFO, "SimpleCam Direct %s loaded", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	simplecam::setCameraRuntime(nullptr);
	delete runtime;
	runtime = nullptr;
	obs_frontend_remove_dock("simplecamdirect.camera-control");
	obs_frontend_remove_dock("simplecamdirect.cameras");
	cameraControlWidget = nullptr;
	camerasWidget = nullptr;
	obs_log(LOG_INFO, "SimpleCam Direct unloaded");
}
