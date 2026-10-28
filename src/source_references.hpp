#pragma once

#include <obs.h>
#include <algorithm>
#include <string>
#include <vector>

namespace simplecam {

// Public scene paths only. Count occurrences, including hidden items, without
// treating a nested scene as an independent copy of its contents.
struct SourceReferencePath {
	std::string path;
	bool indirect = false;
};

inline std::vector<SourceReferencePath> sourceReferencePaths(obs_source_t *target)
{
	struct Walk {
		obs_source_t *target;
		std::vector<SourceReferencePath> result;
		std::vector<obs_scene_t *> ancestors;
		std::string path;
		bool indirect = false;
		static bool item(obs_scene_t *, obs_sceneitem_t *entry, void *opaque)
		{
			auto &walk = *static_cast<Walk *>(opaque);
			obs_source_t *source = obs_sceneitem_get_source(entry);
			if (source == walk.target)
				walk.result.push_back({walk.path, walk.indirect});
			obs_scene_t *child = obs_sceneitem_is_group(entry) ? obs_sceneitem_group_get_scene(entry)
								    : obs_scene_from_source(source);
			if (!child || walk.ancestors.size() >= 64 ||
			    std::find(walk.ancestors.begin(), walk.ancestors.end(), child) != walk.ancestors.end())
				return true;
			const auto savedPath = walk.path;
			const bool savedIndirect = walk.indirect;
			walk.path += " / ";
			walk.path += obs_source_get_name(source);
			walk.indirect = true;
			walk.ancestors.push_back(child);
			obs_scene_enum_items(child, item, &walk);
			walk.ancestors.pop_back();
			walk.path = savedPath;
			walk.indirect = savedIndirect;
			return true;
		}
		static bool scene(void *opaque, obs_source_t *source)
		{
			auto &walk = *static_cast<Walk *>(opaque);
			obs_scene_t *scene = obs_scene_from_source(source);
			if (!scene) return true;
			walk.path = obs_source_get_name(source);
			walk.indirect = false;
			walk.ancestors = {scene};
			obs_scene_enum_items(scene, item, &walk);
			return true;
		}
	};
	Walk walk{target, {}, {}, {}, false};
	if (target) obs_enum_scenes(Walk::scene, &walk);
	return walk.result;
}

// Only direct items in the captured editor scene are eligible. Nested scenes
// must be opened explicitly, avoiding changes to all their parent scenes.
inline obs_sceneitem_t *independentCopyTarget(obs_scene_t *scene, obs_source_t *target)
{
	struct Matches {
		obs_source_t *target;
		std::vector<obs_sceneitem_t *> all, selected;
	} matches{target, {}, {}};
	if (!scene || !target) return nullptr;
	obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *opaque) {
		auto &m = *static_cast<Matches *>(opaque);
		if (obs_sceneitem_get_source(item) == m.target) {
			m.all.push_back(item);
			if (obs_sceneitem_selected(item)) m.selected.push_back(item);
		}
		return true;
	}, &matches);
	if (matches.selected.size() == 1) return matches.selected.front();
	return matches.all.size() == 1 ? matches.all.front() : nullptr;
}

// Caller owns the returned source. Revalidate the captured item before any
// allocation/removal. Add first, remove last, inside OBS's scene update lock.
inline obs_source_t *replaceWithIndependentSource(obs_scene_t *scene, int64_t itemId,
						obs_source_t *original, obs_data_t *settings,
						const char *uniqueName)
{
	obs_sceneitem_t *oldItem = scene ? obs_scene_find_sceneitem_by_id(scene, itemId) : nullptr;
	if (!oldItem || obs_sceneitem_get_source(oldItem) != original) return nullptr;
	obs_source_t *copy = obs_source_duplicate(original, uniqueName, false);
	if (!copy || copy == original) {
		obs_source_release(copy);
		return nullptr;
	}
	obs_source_reset_settings(copy, settings);
	struct Replacement {
		obs_sceneitem_t *oldItem;
		obs_source_t *copy;
		bool success = false;
	} replacement{oldItem, copy, false};
	obs_scene_atomic_update(scene, [](void *opaque, obs_scene_t *parent) {
		auto &r = *static_cast<Replacement *>(opaque);
		obs_sceneitem_t *item = obs_scene_add(parent, r.copy);
		if (!item) return;
		obs_transform_info info{};
		obs_sceneitem_get_info2(r.oldItem, &info);
		obs_sceneitem_crop crop{};
		obs_sceneitem_get_crop(r.oldItem, &crop);
		obs_sceneitem_set_info2(item, &info);
		obs_sceneitem_set_crop(item, &crop);
		obs_sceneitem_set_scale_filter(item, obs_sceneitem_get_scale_filter(r.oldItem));
		obs_sceneitem_set_blending_method(item, obs_sceneitem_get_blending_method(r.oldItem));
		obs_sceneitem_set_blending_mode(item, obs_sceneitem_get_blending_mode(r.oldItem));
		obs_sceneitem_set_visible(item, obs_sceneitem_visible(r.oldItem));
		obs_sceneitem_set_locked(item, obs_sceneitem_locked(r.oldItem));
		obs_sceneitem_select(item, obs_sceneitem_selected(r.oldItem));
		obs_data_t *oldPrivate = obs_sceneitem_get_private_settings(r.oldItem);
		obs_data_t *newPrivate = obs_sceneitem_get_private_settings(item);
		obs_data_apply(newPrivate, oldPrivate);
		obs_data_release(oldPrivate);
		obs_data_release(newPrivate);
		obs_sceneitem_set_order_position(item, obs_sceneitem_get_order_position(r.oldItem));
		obs_sceneitem_remove(r.oldItem);
		r.success = true;
	}, &replacement);
	if (!replacement.success) {
		obs_source_remove(copy);
		obs_source_release(copy);
		return nullptr;
	}
	return copy;
}
} // namespace simplecam
