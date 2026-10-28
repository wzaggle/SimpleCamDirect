#include "source_references.hpp"
#include <iostream>
#include <cstring>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *message)
{
	++checks;
	if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
const char *name(void *) { return "Test preset"; }
void *create(obs_data_t *, obs_source_t *source) { return source; }
void destroy(void *) {}
}

int main()
{
	if (!obs_startup("en-US", nullptr, nullptr)) return 1;
	obs_source_info info{};
	info.id = "simplecam_reference_test";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.get_name = name;
	info.create = create;
	info.destroy = destroy;
	obs_register_source(&info);
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "preset", 7);
	obs_data_set_int(settings, "delay_seconds", 3);
	obs_source_t *source = obs_source_create(info.id, "Move Camera 1 to Stage Left", settings, nullptr);
	obs_source_t *other = obs_source_create(info.id, "Other source", settings, nullptr);
	obs_scene_t *first = obs_scene_create("First scene");
	obs_scene_t *second = obs_scene_create("Second scene");
	obs_scene_t *parent = obs_scene_create("Parent scene");
	obs_scene_add(first, other);
	obs_sceneitem_t *oldItem = obs_scene_add(first, source);
	obs_scene_add(first, other);
	obs_scene_add(second, source);
	obs_scene_add(parent, obs_scene_get_source(second));
	check(simplecam::sourceReferencePaths(source).size() == 3, "List direct and indirect scene paths");
	bool nested = false;
	for (const auto &path : simplecam::sourceReferencePaths(source))
		if (path.path == "Parent scene / Second scene" && path.indirect) nested = true;
	check(nested, "Nested path names its actual containing scene");
	check(simplecam::sourceReferencePaths(other).size() == 2, "Count repeated occurrences in one scene");
	check(simplecam::independentCopyTarget(parent, source) == nullptr, "Never silently replace contents of a nested scene");
	check(simplecam::independentCopyTarget(first, other) == nullptr, "Ambiguous unselected occurrences are rejected");
	check(simplecam::independentCopyTarget(first, source) == oldItem, "One direct occurrence is unambiguous");
	obs_sceneitem_select(oldItem, true);
	obs_sceneitem_set_visible(oldItem, false);
	obs_sceneitem_set_locked(oldItem, true);
	obs_transform_info transform{};
	obs_sceneitem_get_info2(oldItem, &transform);
	transform.pos.x = 19.0f;
	transform.pos.y = 23.0f;
	obs_sceneitem_set_info2(oldItem, &transform);
	obs_sceneitem_crop crop{1, 2, 3, 4};
	obs_sceneitem_set_crop(oldItem, &crop);
	const int order = obs_sceneitem_get_order_position(oldItem);
	const int64_t oldId = obs_sceneitem_get_id(oldItem);
	obs_data_t *privateSettings = obs_sceneitem_get_private_settings(oldItem);
	obs_data_set_string(privateSettings, "custom", "retained");
	obs_data_release(privateSettings);
	obs_data_t *copySettings = obs_data_create();
	obs_data_apply(copySettings, settings);
	obs_data_set_int(copySettings, "preset", 9);
	obs_source_t *copy = simplecam::replaceWithIndependentSource(first, oldId, source, copySettings,
		"Move Camera 1 to Stage Left (2)");
	check(copy && copy != source, "Create a genuinely independent source");
	if (copy) {
		check(std::strcmp(obs_source_get_uuid(copy), obs_source_get_uuid(source)) != 0, "Copy gets a different UUID");
		check(std::strcmp(obs_source_get_name(copy), "Move Camera 1 to Stage Left (2)") == 0, "Unique numbered name retained");
		obs_sceneitem_t *item = simplecam::independentCopyTarget(first, copy);
		check(item && obs_sceneitem_get_order_position(item) == order, "Stack position preserved");
		check(item && !obs_sceneitem_visible(item) && obs_sceneitem_locked(item) && obs_sceneitem_selected(item),
			"Visibility, lock and selection preserved");
		obs_transform_info actual{};
		obs_sceneitem_get_info2(item, &actual);
		check(actual.pos.x == 19.0f && actual.pos.y == 23.0f, "Transform preserved");
		obs_sceneitem_crop actualCrop{};
		obs_sceneitem_get_crop(item, &actualCrop);
		check(actualCrop.left == 1 && actualCrop.bottom == 4, "Crop preserved");
		privateSettings = obs_sceneitem_get_private_settings(item);
		check(std::strcmp(obs_data_get_string(privateSettings, "custom"), "retained") == 0, "Private item settings preserved");
		obs_data_release(privateSettings);
		obs_data_t *actualSettings = obs_source_get_settings(copy);
		check(obs_data_get_int(actualSettings, "preset") == 9 && obs_data_get_int(actualSettings, "delay_seconds") == 3,
			"Independent settings copied");
		obs_data_release(actualSettings);
		actualSettings = obs_source_get_settings(source);
		check(obs_data_get_int(actualSettings, "preset") == 7, "Other scene's source settings unchanged");
		obs_data_release(actualSettings);
		check(simplecam::sourceReferencePaths(source).size() == 2, "Original stays in second and parent scene");
		check(simplecam::sourceReferencePaths(copy).size() == 1, "Copy belongs only to intended scene");
		check(simplecam::replaceWithIndependentSource(first, oldId, source, copySettings, "Stale copy") == nullptr,
			"Stale captured item cannot remove another reference");
	}
	check(simplecam::replaceWithIndependentSource(second, 999999, source, settings, "Missing") == nullptr,
		"Missing target creates no replacement");
	obs_data_release(copySettings);
	obs_data_release(settings);
	for (obs_scene_t *scene : {parent, first, second}) {
		obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *) {
			obs_sceneitem_remove(item);
			return true;
		}, nullptr);
		obs_source_remove(obs_scene_get_source(scene));
		obs_scene_release(scene);
	}
	for (obs_source_t *entry : {copy, source, other}) {
		if (entry) obs_source_remove(entry);
		obs_source_release(entry);
	}
	obs_wait_for_destroy_queue();
	std::cout << checks << " checks, " << failures << " failures" << std::endl;
	obs_shutdown();
	return failures ? 1 : 0;
}
