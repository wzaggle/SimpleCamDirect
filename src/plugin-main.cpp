#include "camera_docks.hpp"
#include "camera_runtime.hpp"
#include "preset_source.hpp"
#include "profile_test_tool.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/base.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <array>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr char kCameraControlDockTitle[] = "SimpleCamPTZ v0.1.0";

simplecam::CameraRuntimeArray runtimes{};
QWidget *cameraControlWidget = nullptr;
QPointer<simplecam::CameraControlDock> cameraControlDock;
QPointer<simplecam::CamerasTool> camerasTool;
QPointer<simplecam::CameraProfileTestDialog> profileTestDialog;
QPointer<QTimer> sourceNameRefreshTimer;
QString builtInProfilePath;
QString builtInProfileDirectory;
QString userProfileDirectory;

QString userProfilesPath()
{
	return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
		       QStringLiteral("/simplecamdirect/profiles"); // Keep existing user profiles across the product rename.
}

void migratePreviousModuleSettings(const QString &settingsPath)
{
	const QFileInfo currentInfo(settingsPath);
	QDir configRoot = currentInfo.dir();
	configRoot.cdUp();
	const QDir previousDir(configRoot.filePath(QStringLiteral("simplecamdirect")));
	if (!previousDir.exists() || previousDir.absolutePath() == currentInfo.absolutePath())
		return;

	const QStringList legacyFiles = previousDir.entryList(
		QStringList{QStringLiteral("simplecamdirect*.ini")}, QDir::Files);
	for (const QString &fileName : legacyFiles) {
		const QString targetPath = currentInfo.dir().filePath(fileName);
		if (!QFileInfo::exists(targetPath))
			QFile::copy(previousDir.filePath(fileName), targetPath);
	}
}

void openProfileTestTool(void *)
{
	if (profileTestDialog) {
		profileTestDialog->show();
		profileTestDialog->raise();
		profileTestDialog->activateWindow();
		return;
	}
	profileTestDialog = new simplecam::CameraProfileTestDialog(builtInProfilePath);
	profileTestDialog->setAttribute(Qt::WA_DeleteOnClose);
	profileTestDialog->show();
}

void openCamerasTool(void *)
{
	if (camerasTool) {
		camerasTool->show();
		camerasTool->raise();
		camerasTool->activateWindow();
		return;
	}
	camerasTool = new simplecam::CamerasTool(runtimes, builtInProfileDirectory,
						 userProfileDirectory, []() {
							 if (cameraControlDock)
								 cameraControlDock->refreshView();
						 });
	camerasTool->setAttribute(Qt::WA_DeleteOnClose);
	camerasTool->show();
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	for (simplecam::CameraRuntime *runtime : runtimes) {
		if (!runtime)
			return;
	}
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

namespace simplecam {

obs_source_t *currentProgramSceneSource()
{
	return obs_frontend_get_current_scene();
}

} // namespace simplecam

bool obs_module_load(void)
{
	char *configPath = obs_module_config_path("simplecamdirect.ini");
	if (!configPath) {
		obs_log(LOG_ERROR, "SimpleCamPTZ could not resolve its settings path");
		return false;
	}
	const QString settingsPath = QString::fromUtf8(configPath);
	bfree(configPath);
	QDir().mkpath(QFileInfo(settingsPath).absolutePath());
	migratePreviousModuleSettings(settingsPath);

	char *profilePath = obs_module_file("cameras/avipas-av-1280w.visca.json");
	builtInProfilePath = profilePath ? QString::fromUtf8(profilePath) : QString();
	bfree(profilePath);
	builtInProfileDirectory = builtInProfilePath.isEmpty() ? QString() : QFileInfo(builtInProfilePath).absolutePath();
	userProfileDirectory = userProfilesPath();
	QDir().mkpath(userProfileDirectory);
	runtimes[0] = new simplecam::CameraRuntime(settingsPath, QStringLiteral("Camera 1"), builtInProfilePath);
	for (int index = 1; index < simplecam::CameraSlotCount; ++index) {
		const QString settingsFile = QStringLiteral("simplecamdirect-camera%1.ini").arg(index + 1);
		const QString slotSettingsPath = QFileInfo(settingsPath).dir().filePath(settingsFile);
		runtimes[index] = new simplecam::CameraRuntime(slotSettingsPath,
								      QStringLiteral("Camera %1").arg(index + 1), builtInProfilePath);
		runtimes[index]->setSharedPresetOwner(*runtimes[0]);
	}
	for (int index = 0; index < simplecam::CameraSlotCount; ++index)
		simplecam::setCameraRuntime(index, runtimes[index]);
	simplecam::registerPresetSource();

	cameraControlDock = new simplecam::CameraControlDock(runtimes);
	cameraControlWidget = cameraControlDock;
	const bool controlAdded = obs_frontend_add_dock_by_id("simplecamdirect.camera-control", kCameraControlDockTitle, cameraControlWidget);
	if (!controlAdded) {
		obs_log(LOG_ERROR, "SimpleCamPTZ could not add its OBS dock widgets");
		if (controlAdded)
			obs_frontend_remove_dock("simplecamdirect.camera-control");
		cameraControlWidget = nullptr;
		cameraControlDock = nullptr;
		for (simplecam::CameraRuntime *runtime : runtimes)
			delete runtime;
		runtimes = {};
		for (int index = 0; index < simplecam::CameraSlotCount; ++index)
			simplecam::setCameraRuntime(index, nullptr);
		return false;
	}
	// Isolation test: OBS owns its native dock separators. Do not install a
	// main-window stylesheet override, proxy style, or custom resize grip.
	obs_log(LOG_INFO, "[SimpleCamPTZ DockIsolation] build=native-separators-r1 override=OFF customGrip=OFF");
	obs_frontend_add_tools_menu_item("SimpleCamPTZ Profile Test", openProfileTestTool, nullptr);
	obs_frontend_add_tools_menu_item("SimpleCamPTZ Cameras", openCamerasTool, nullptr);
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	sourceNameRefreshTimer = new QTimer(cameraControlWidget);
	sourceNameRefreshTimer->setInterval(1000);
	QObject::connect(sourceNameRefreshTimer.data(), &QTimer::timeout,
			 []() { simplecam::refreshPresetSourceNames(); });
	sourceNameRefreshTimer->start();
	obs_log(LOG_INFO, "SimpleCamPTZ %s loaded", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	simplecam::shutdownPresetSourceUi();
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	// The timer is parented to the control dock. OBS may already have destroyed
	// that dock during shutdown, so let QObject ownership destroy the timer and
	// use QPointer to avoid a stale global pointer.
	if (sourceNameRefreshTimer)
		sourceNameRefreshTimer->stop();
	sourceNameRefreshTimer = nullptr;
	if (profileTestDialog) {
		profileTestDialog->close();
		delete profileTestDialog.data();
	}
	profileTestDialog = nullptr;
	if (camerasTool) {
		camerasTool->close();
		delete camerasTool.data();
	}
	camerasTool = nullptr;
	// Remove the UI objects before destroying the runtimes they reference.
	obs_frontend_remove_dock("simplecamdirect.camera-control");
	cameraControlWidget = nullptr;
	cameraControlDock = nullptr;
	for (int index = 0; index < simplecam::CameraSlotCount; ++index)
		simplecam::setCameraRuntime(index, nullptr);
	for (simplecam::CameraRuntime *runtime : runtimes)
		delete runtime;
	runtimes = {};
	builtInProfilePath.clear();
	builtInProfileDirectory.clear();
	userProfileDirectory.clear();
	obs_log(LOG_INFO, "SimpleCamPTZ unloaded");
}
