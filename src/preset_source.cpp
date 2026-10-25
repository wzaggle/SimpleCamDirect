#include "preset_source.hpp"

#include "camera_runtime.hpp"

#include <plugin-support.h>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/base.h>

#include <QCoreApplication>
#include <QMetaObject>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <QString>
#include <vector>

namespace simplecam {
namespace {

constexpr char kSourceId[] = "simplecamdirect_preset";
constexpr int kProgramMode = 0;
constexpr int kPreviewMode = 1;
constexpr int kBothMode = 2;
constexpr int kAllModes = 3;

int cameraIndexFromValue(const char *value)
{
	const QString camera = QString::fromUtf8(value ? value : "1").trimmed();
	bool ok = false;
	int numeric = camera.toInt(&ok);
	if (!ok) {
		const int separator = camera.lastIndexOf(QLatin1Char(' '));
		if (separator >= 0)
			numeric = camera.mid(separator + 1).toInt(&ok);
	}
	return ok && numeric >= 1 && numeric <= CameraSlotCount ? numeric - 1 : 0;
}

std::mutex liveSourcesMutex;
std::vector<struct PresetSourceData *> liveSources;

QString makeDisplayName(const struct PresetSourceData &data);

struct ReferenceCountContext {
	obs_source_t *target = nullptr;
	size_t count = 0;
};

bool countSceneReferenceItem(obs_scene_t *, obs_sceneitem_t *item, void *opaque)
{
	auto *context = static_cast<ReferenceCountContext *>(opaque);
	if (obs_sceneitem_is_group(item)) {
		if (obs_scene_t *group = obs_sceneitem_group_get_scene(item))
			obs_scene_enum_items(group, countSceneReferenceItem, context);
	} else if (obs_sceneitem_get_source(item) == context->target) {
		++context->count;
	}
	return true;
}

size_t countSceneReferences(obs_source_t *target)
{
	if (!target)
		return 0;

	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	ReferenceCountContext context{target, 0};
	for (size_t i = 0; i < scenes.sources.num; ++i) {
		if (obs_scene_t *scene = obs_scene_from_source(scenes.sources.array[i]))
			obs_scene_enum_items(scene, countSceneReferenceItem, &context);
	}
	obs_frontend_source_list_free(&scenes);
	return context.count;
}

struct PresetSourceData {
	obs_source_t *source = nullptr;
	bool initializing = false;
	int logicalPreset = 1;
	int triggerMode = kProgramMode;
	int cameraIndex = 0;
	QString cameraName;
	QString presetLabel;
};

void refreshSourceName(PresetSourceData *data, bool referencedMoreThanOnce = false)
{
	if (!data || !data->source)
		return;
	if (CameraRuntime *runtime = cameraRuntime(data->cameraIndex)) {
		data->cameraName = runtime->cameraName();
		data->presetLabel = runtime->presets().presetLabel(runtime->activePage(), data->logicalPreset - 1);
	}
	QString display = makeDisplayName(*data);
	if (referencedMoreThanOnce)
		display = QStringLiteral("<span style=\"color:#FF6050;\">%1 *</span>").arg(display);
	const QByteArray displayName = display.toUtf8();
	const char *currentName = obs_source_get_name(data->source);
	if (!currentName || display != QString::fromUtf8(currentName))
		obs_source_set_name(data->source, displayName.constData());
}

struct DeferredSourceRename {
	obs_source_t *source = nullptr;
	PresetSourceData *data = nullptr;
};

void scheduleSourceNameRefreshAfterCreate(PresetSourceData *data)
{
	auto *rename = new DeferredSourceRename{obs_source_get_ref(data->source), data};
	auto *application = QCoreApplication::instance();
	if (!application) {
		obs_source_release(rename->source);
		delete rename;
		return;
	}

	// OBS's OBS_TASK_UI handler uses Qt::AutoConnection, which executes
	// immediately when called from OBS's UI thread. Force a queued Qt event so
	// the source-creation callback and OBS name registration have both finished.
	const bool queued = QMetaObject::invokeMethod(
		application,
		[rename]() {
			refreshSourceName(rename->data);
			obs_source_release(rename->source);
			delete rename;
		},
		Qt::QueuedConnection);
	if (!queued) {
		obs_source_release(rename->source);
		delete rename;
	}
}

void sourceUpdate(void *opaque, obs_data_t *settings);

void *sourceCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *data = new PresetSourceData;
	data->source = source;
	data->initializing = true;
	sourceUpdate(data, settings);
	data->initializing = false;
	{
		std::lock_guard<std::mutex> lock(liveSourcesMutex);
		liveSources.push_back(data);
	}
	// OBS invokes create() before the source has been inserted into its name
	// registry. Renaming here can crash in obs_source_set_name.
	scheduleSourceNameRefreshAfterCreate(data);
	return data;
}

QString makeDisplayName(const PresetSourceData &data)
{
	const QString camera = data.cameraName.trimmed().isEmpty() ? QStringLiteral("Camera 1") : data.cameraName;
	const QString preset = data.presetLabel.trimmed().isEmpty()
			       ? QStringLiteral("Preset %1").arg(data.logicalPreset)
			       : data.presetLabel;
	return QStringLiteral("Move %1 to <i><b>%2</b></i>")
		.arg(camera.toHtmlEscaped(), preset.toHtmlEscaped());
}

void sourceUpdate(void *opaque, obs_data_t *settings)
{
	auto *data = static_cast<PresetSourceData *>(opaque);
	if (!data)
		return;
	std::lock_guard<std::mutex> lock(liveSourcesMutex);
	data->logicalPreset = static_cast<int>(std::clamp<std::int64_t>(obs_data_get_int(settings, "preset"), 1, 14));
	data->triggerMode = static_cast<int>(std::clamp<std::int64_t>(obs_data_get_int(settings, "trigger_mode"), 0, 2));
	const char *camera = obs_data_get_string(settings, "camera");
	data->cameraIndex = cameraIndexFromValue(camera);
	if (CameraRuntime *runtime = cameraRuntime(data->cameraIndex)) {
		data->cameraName = runtime->cameraName();
		const int page = runtime->activePage();
		data->presetLabel = runtime->presets().presetLabel(page, data->logicalPreset - 1);
	} else {
		data->cameraName = QStringLiteral("Camera %1").arg(data->cameraIndex + 1);
		data->presetLabel.clear();
	}
	if (!data->initializing)
		refreshSourceName(data, countSceneReferences(data->source) > 1);
}

void sourceDestroy(void *opaque)
{
	auto *data = static_cast<PresetSourceData *>(opaque);
	{
		std::lock_guard<std::mutex> lock(liveSourcesMutex);
		const auto found = std::find(liveSources.begin(), liveSources.end(), data);
		if (found != liveSources.end())
			liveSources.erase(found);
	}
	delete data;
}

const char *sourceName(void *)
{
	return obs_module_text("SimpleCamPTZ.PresetSource");
}

void sourceDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "camera", "Camera 1");
	obs_data_set_default_int(settings, "preset", 1);
	obs_data_set_default_int(settings, "trigger_mode", kProgramMode);
	obs_data_set_default_int(settings, "delay_seconds", 0);
}

void populatePresetList(obs_property_t *preset, int cameraIndex)
{
	obs_property_list_clear(preset);
	const CameraRuntime *runtime = cameraRuntime(cameraIndex);
	const int activePage = runtime ? runtime->activePage() : 0;
	for (int i = 0; i < PresetCatalog::SlotsPerPage; ++i) {
		const bool usable = !runtime || runtime->hasUsablePreset(activePage, i);
		QString label = runtime ? runtime->presets().presetLabel(activePage, i) : QString();
		if (!usable)
			label = QStringLiteral("---");
		else if (label.trimmed().isEmpty())
			label = QStringLiteral("Preset %1").arg(i + 1);
		const QByteArray utf8 = label.toUtf8();
		const size_t item = obs_property_list_add_int(preset, utf8.constData(), i + 1);
		if (!usable)
			obs_property_list_item_disable(preset, item, true);
	}
}

bool cameraSelectionChanged(obs_properties_t *properties, obs_property_t *, obs_data_t *settings)
{
	const int cameraIndex = cameraIndexFromValue(obs_data_get_string(settings, "camera"));
	populatePresetList(obs_properties_get(properties, "preset"), cameraIndex);
	return true;
}

obs_properties_t *sourceProperties(void *opaque)
{
	const auto *sourceData = static_cast<const PresetSourceData *>(opaque);
	const int currentCamera = sourceData ? sourceData->cameraIndex : 0;
	auto *properties = obs_properties_create();
	auto *camera = obs_properties_add_list(properties, "camera", obs_module_text("SimpleCamPTZ.Camera"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	for (int index = 0; index < CameraSlotCount; ++index) {
		const CameraRuntime *runtime = cameraRuntime(index);
		const QByteArray label = (runtime ? runtime->cameraName() : QStringLiteral("Camera %1").arg(index + 1)).toUtf8();
		const QByteArray value = QStringLiteral("Camera %1").arg(index + 1).toUtf8();
		obs_property_list_add_string(camera, label.constData(), value.constData());
	}
	obs_property_set_modified_callback(camera, cameraSelectionChanged);

	auto *preset = obs_properties_add_list(properties, "preset", obs_module_text("SimpleCamPTZ.Preset"),
						OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	populatePresetList(preset, currentCamera);

	auto *mode = obs_properties_add_list(properties, "trigger_mode", obs_module_text("SimpleCamPTZ.Trigger"),
						OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, obs_module_text("SimpleCamPTZ.Program"), kProgramMode);
	obs_property_list_add_int(mode, obs_module_text("SimpleCamPTZ.Preview"), kPreviewMode);
	obs_property_list_add_int(mode, obs_module_text("SimpleCamPTZ.Both"), kBothMode);
	obs_properties_add_int_slider(properties, "delay_seconds", obs_module_text("SimpleCamPTZ.Delay"), 0, 600, 1);
	return properties;
}

struct DispatchContext {
	int triggerMode;
};

bool dispatchSceneItem(obs_scene_t *scene, obs_sceneitem_t *item, void *opaque)
{
	auto *context = static_cast<DispatchContext *>(opaque);
	if (!obs_sceneitem_visible(item))
		return true;
	if (obs_sceneitem_is_group(item)) {
		if (obs_scene_t *groupScene = obs_sceneitem_group_get_scene(item))
			obs_scene_enum_items(groupScene, dispatchSceneItem, context);
		return true;
	}

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source || std::strcmp(obs_source_get_id(source), kSourceId) != 0)
		return true;
	obs_data_t *settings = obs_source_get_settings(source);
	const int mode = static_cast<int>(obs_data_get_int(settings, "trigger_mode"));
	const bool matches = context->triggerMode == kAllModes || mode == kBothMode || mode == context->triggerMode;
	const int preset = static_cast<int>(obs_data_get_int(settings, "preset"));
	const int delay = static_cast<int>(obs_data_get_int(settings, "delay_seconds")) * 1000;
	const int cameraIndex = cameraIndexFromValue(obs_data_get_string(settings, "camera"));
	obs_data_release(settings);
	if (matches) {
		if (CameraRuntime *runtime = cameraRuntime(cameraIndex)) {
			if (cameraIndex == 0 && runtime->activePage() == 0 && preset == 1) {
				obs_source_t *sceneSource = obs_scene_get_source(scene);
				const char *sceneName = sceneSource ? obs_source_get_name(sceneSource) : "<unknown>";
				const char *presetSourceName = obs_source_get_name(source);
				obs_log(LOG_INFO,
					"[SimpleCamPTZ PresetFadeDiag] stage=obs-source-request camera=1 group=1 button=1 "
					"scene=\"%s\" source=\"%s\" delay_ms=%d trigger_mode=%d",
					sceneName ? sceneName : "<unknown>", presetSourceName ? presetSourceName : "<unknown>", delay, mode);
			}
			runtime->recallLogicalPreset(preset, delay, [preset](bool ok, const QString &message) {
				if (!ok)
					obs_log(LOG_WARNING, "SimpleCamPTZ preset %d failed: %s", preset, message.toUtf8().constData());
			}, CameraRuntime::PresetRecallOrigin::ObsSource);
		}
	}
	return true;
}

} // namespace

void registerPresetSource()
{
	static obs_source_info info{};
	info.id = kSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = 0;
	info.get_name = sourceName;
	info.create = sourceCreate;
	info.destroy = sourceDestroy;
	info.update = sourceUpdate;
	info.get_defaults = sourceDefaults;
	info.get_properties = sourceProperties;
	obs_register_source(&info);
}

void dispatchScenePresets(obs_source_t *sceneSource, int triggerMode)
{
	if (!sceneSource)
		return;
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (!scene)
		return;
	DispatchContext context{triggerMode};
	obs_scene_enum_items(scene, dispatchSceneItem, &context);
}

void refreshPresetSourceNames()
{
	std::vector<std::pair<PresetSourceData *, bool>> refreshList;
	std::lock_guard<std::mutex> lock(liveSourcesMutex);
	refreshList.reserve(liveSources.size());
	for (auto *data : liveSources)
		refreshList.emplace_back(data, countSceneReferences(data->source) > 1);
	for (const auto &entry : refreshList)
		refreshSourceName(entry.first, entry.second);
}

} // namespace simplecam
