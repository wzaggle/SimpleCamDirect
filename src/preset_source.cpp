#include "preset_source.hpp"

#include "camera_runtime.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace simplecam {
namespace {

constexpr char kSourceId[] = "simplecamdirect_preset";
constexpr int kProgramMode = 0;
constexpr int kPreviewMode = 1;
constexpr int kBothMode = 2;
constexpr int kAllModes = 3;

std::mutex liveSourcesMutex;
std::vector<struct PresetSourceData *> liveSources;

QString makeDisplayName(const struct PresetSourceData &data);

struct PresetSourceData {
	obs_source_t *source = nullptr;
	int logicalPreset = 1;
	int triggerMode = kProgramMode;
	QString cameraName;
	QString presetLabel;
};

void refreshSourceName(PresetSourceData *data)
{
	if (!data || !data->source)
		return;
	if (CameraRuntime *runtime = cameraRuntime()) {
		data->cameraName = runtime->cameraName();
		data->presetLabel = runtime->presets().presetLabel(runtime->activePage(), data->logicalPreset - 1);
	}
	const QByteArray displayName = makeDisplayName(*data).toUtf8();
	obs_source_set_name(data->source, displayName.constData());
}

void sourceUpdate(void *opaque, obs_data_t *settings);

PresetSourceData *sourceCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *data = new PresetSourceData;
	data->source = source;
	sourceUpdate(data, settings);
	{
		std::lock_guard<std::mutex> lock(liveSourcesMutex);
		liveSources.push_back(data);
	}
	return data;
}

QString makeDisplayName(const PresetSourceData &data)
{
	const QString camera = data.cameraName.trimmed().isEmpty() ? QStringLiteral("Camera 1") : data.cameraName;
	const QString preset = data.presetLabel.trimmed().isEmpty()
			       ? QStringLiteral("Preset %1").arg(data.logicalPreset)
			       : data.presetLabel;
	return QStringLiteral("Move %1 to %2").arg(camera, preset);
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
	data->cameraName = QString::fromUtf8(camera && *camera ? camera : "Camera 1");
	if (CameraRuntime *runtime = cameraRuntime()) {
		data->cameraName = runtime->cameraName();
		const int page = runtime->activePage();
		data->presetLabel = runtime->presets().presetLabel(page, data->logicalPreset - 1);
	} else {
		data->presetLabel.clear();
	}
	refreshSourceName(data);
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
	return obs_module_text("SimpleCamDirect.PresetSource");
}

void sourceDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "camera", "Camera 1");
	obs_data_set_default_int(settings, "preset", 1);
	obs_data_set_default_int(settings, "trigger_mode", kProgramMode);
	obs_data_set_default_int(settings, "delay_seconds", 0);
}

obs_properties_t *sourceProperties(void *)
{
	auto *properties = obs_properties_create();
	auto *camera = obs_properties_add_list(properties, "camera", obs_module_text("SimpleCamDirect.Camera"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(camera, "Camera 1", "Camera 1");

	auto *preset = obs_properties_add_list(properties, "preset", obs_module_text("SimpleCamDirect.Preset"),
						OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	const CameraRuntime *runtime = cameraRuntime();
	const int activePage = runtime ? runtime->activePage() : 0;
	for (int i = 0; i < PresetCatalog::SlotsPerPage; ++i) {
		const bool usable = !runtime || runtime->presets().hasUsablePreset(activePage, i);
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

	auto *mode = obs_properties_add_list(properties, "trigger_mode", obs_module_text("SimpleCamDirect.Trigger"),
						OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, obs_module_text("SimpleCamDirect.Program"), kProgramMode);
	obs_property_list_add_int(mode, obs_module_text("SimpleCamDirect.Preview"), kPreviewMode);
	obs_property_list_add_int(mode, obs_module_text("SimpleCamDirect.Both"), kBothMode);
	obs_properties_add_int_slider(properties, "delay_seconds", obs_module_text("SimpleCamDirect.Delay"), 0, 600, 1);
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
	obs_data_release(settings);
	if (matches) {
		if (CameraRuntime *runtime = cameraRuntime())
			runtime->recallLogicalPreset(preset, delay, [preset](bool ok, const QString &message) {
				if (!ok)
					obs_log(LOG_WARNING, "SimpleCam Direct preset %d failed: %s", preset, message.toUtf8().constData());
			});
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
	std::lock_guard<std::mutex> lock(liveSourcesMutex);
	for (auto *data : liveSources)
		refreshSourceName(data);
}

} // namespace simplecam
