#include "program_output.hpp"

#include <iostream>

namespace {
int checks = 0;
int failures = 0;
void check(bool ok, const char *description)
{
	++checks;
	if (!ok) {
		++failures;
		std::cerr << "FAIL: " << description << '\n';
	}
}
const char *name(void *) { return "Test camera"; }
void *create(obs_data_t *, obs_source_t *source) { return source; }
void destroy(void *) {}
uint32_t dimension(void *) { return 64; }
void render(void *, gs_effect_t *) {}
bool audioRender(void *, uint64_t *, obs_source_audio_mix *, uint32_t, size_t, size_t) { return false; }
obs_source_t *topSource(obs_source_t *sceneSource)
{
	obs_source_t *top = nullptr; // Borrowed; the scene owns its items.
	obs_scene_enum_items(obs_scene_from_source(sceneSource),
		[](obs_scene_t *, obs_sceneitem_t *item, void *opaque) {
			if (obs_sceneitem_visible(item))
				*static_cast<obs_source_t **>(opaque) = obs_sceneitem_get_source(item);
			return true;
		}, &top);
	return top;
}
void checkOutput(obs_source_t *expected, obs_source_t *expectedTop, const char *description)
{
	obs_source_t *actual = simplecam::acquireProgramOutputScene();
	check(actual == expected, description);
	if (expected)
		check(actual && topSource(actual) == expectedTop, "Output retains the correct visible camera stack");
	obs_source_release(actual);
}
}

int main()
{
	if (!obs_startup("en-US", nullptr, nullptr)) {
		std::cerr << "OBS core startup failed\n";
		return 1;
	}
	obs_source_info cameraInfo{};
	cameraInfo.id = "simplecam_test_camera";
	cameraInfo.type = OBS_SOURCE_TYPE_INPUT;
	cameraInfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE;
	cameraInfo.get_name = name;
	cameraInfo.create = create;
	cameraInfo.destroy = destroy;
	cameraInfo.get_width = dimension;
	cameraInfo.get_height = dimension;
	cameraInfo.video_render = render;
	obs_register_source(&cameraInfo);
	obs_source_info transitionInfo{};
	transitionInfo.id = "simplecam_test_transition";
	transitionInfo.type = OBS_SOURCE_TYPE_TRANSITION;
	transitionInfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	transitionInfo.get_name = name;
	transitionInfo.create = create;
	transitionInfo.destroy = destroy;
	transitionInfo.video_render = render;
	transitionInfo.audio_render = audioRender;
	obs_register_source(&transitionInfo);

	obs_source_t *camera1 = obs_source_create_private(cameraInfo.id, "Camera 1", nullptr);
	obs_source_t *camera2 = obs_source_create_private(cameraInfo.id, "Camera 2", nullptr);
	obs_scene_t *editable = obs_scene_create("Working scene");
	obs_sceneitem_t *item1 = obs_scene_add(editable, camera1);
	obs_sceneitem_t *item2 = obs_scene_add(editable, camera2);
	if (!camera1 || !camera2 || !editable || !item1 || !item2) {
		std::cerr << "OBS fixture creation failed\n";
		return 1;
	}
	obs_sceneitem_set_order(item1, OBS_ORDER_MOVE_TOP);
	obs_scene_t *program = obs_scene_duplicate(editable, "Working scene", OBS_SCENE_DUP_PRIVATE_REFS);
	obs_source_t *programSource = obs_scene_get_source(program);
	check(program && programSource != obs_scene_get_source(editable), "Program is a separate private scene copy");
	checkOutput(nullptr, nullptr, "No output never substitutes editable Preview");
	obs_set_output_source(0, programSource);
	checkOutput(programSource, camera1, "Direct Program output is scanned");
	obs_sceneitem_set_order(item2, OBS_ORDER_MOVE_TOP);
	check(topSource(obs_scene_get_source(editable)) == camera2, "Operator moved camera 2 to top in Preview");
	checkOutput(programSource, camera1, "Preview reorder does not change Program indicator");
	obs_sceneitem_set_visible(item1, false);
	checkOutput(programSource, camera1, "Preview visibility edit does not change Program indicator");

	obs_source_t *transition = obs_source_create_private(transitionInfo.id, "Program transition", nullptr);
	if (!transition || obs_source_get_type(transition) != OBS_SOURCE_TYPE_TRANSITION) {
		std::cerr << "Program transition fixture registration failed\n";
		return 1;
	}
	check(true, "Program transition created without camera IO");
	obs_transition_set(transition, programSource);
	obs_set_output_source(0, transition);
	checkOutput(programSource, camera1, "Transition resolves actual private Program copy");
	obs_scene_t *incoming = obs_scene_duplicate(editable, "Working scene", OBS_SCENE_DUP_PRIVATE_REFS);
	obs_source_t *incomingSource = obs_scene_get_source(incoming);
	check(obs_transition_start(transition, OBS_TRANSITION_MODE_MANUAL, 1000, incomingSource), "Transition to edited scene starts");
	checkOutput(programSource, camera1, "Transition start retains outgoing camera");
	obs_transition_set_manual_time(transition, 0.5f);
	checkOutput(programSource, camera1, "Transition progress retains outgoing camera");
	obs_transition_set(transition, incomingSource); // Settled A after a cut/completion.
	checkOutput(incomingSource, camera2, "Settled transition switches to newly broadcast camera");
	obs_transition_set(transition, nullptr);
	checkOutput(nullptr, nullptr, "Black output removes camera indication");
	obs_set_output_source(0, camera1);
	checkOutput(nullptr, nullptr, "Non-scene output is not mistaken for editable scene");
	obs_set_output_source(0, nullptr);
	obs_source_release(transition);
	obs_scene_release(incoming);
	obs_scene_release(program);
	obs_scene_release(editable);
	obs_source_release(camera1);
	obs_source_release(camera2);
	obs_shutdown();
	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
