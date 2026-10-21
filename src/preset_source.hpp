#pragma once

extern "C" {
#include <obs.h>
}

namespace simplecam {

void registerPresetSource();
void dispatchScenePresets(obs_source_t *sceneSource, int triggerMode);
void refreshPresetSourceNames();

} // namespace simplecam
