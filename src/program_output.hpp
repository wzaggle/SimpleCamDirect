#pragma once

#include <obs.h>

namespace simplecam {

// Caller owns the returned reference. Never substitute the frontend's editable
// scene: Studio Mode may be broadcasting a private copy with different layers.
inline obs_source_t *acquireProgramOutputScene()
{
	obs_source_t *source = obs_get_output_source(0);
	for (int depth = 0; source && depth < 24; ++depth) {
		if (obs_source_get_type(source) != OBS_SOURCE_TYPE_TRANSITION) {
			if (obs_scene_from_source(source))
				return source;
			obs_source_release(source);
			return nullptr;
		}
		// A is the outgoing/settled picture. get_active_source() would select
		// B immediately while transitioning. OBS promotes B to A on completion.
		obs_source_t *next = obs_transition_get_source(source, OBS_TRANSITION_SOURCE_A);
		obs_source_release(source);
		source = next;
	}
	obs_source_release(source);
	return nullptr; // Empty output, fade to black, or excessive nesting.
}

} // namespace simplecam
