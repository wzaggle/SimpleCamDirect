#include "preset_source_rename.hpp"
#include <obs.h>

#include <iostream>
#include <string>

namespace {
int checks = 0;
int failures = 0;

void check(bool result, const char *message)
{
	++checks;
	if (!result) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

const char *sourceName(void *) { return "Rename regression fixture"; }
void *createSource(obs_data_t *, obs_source_t *source) { return source; }
void destroySource(void *) {}

bool refresh(obs_source_t *source, const std::string &requested,
	     simplecam::PresetSourceRenameState &state, bool fixed)
{
	const std::string current(obs_source_get_name(source));
	if (fixed ? !state.needsRename(requested, current) : current == requested)
		return false;
	obs_source_set_name(source, requested.c_str());
	state.remember(requested, obs_source_get_name(source));
	return true;
}
} // namespace

int main()
{
	if (!obs_startup("en-US", nullptr, nullptr))
		return 1;
	obs_source_info info{};
	info.id = "simplecam_rename_fixture";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.get_name = sourceName;
	info.create = createSource;
	info.destroy = destroySource;
	obs_register_source(&info);

	const std::string requested = "Move Camera 1 to Front Wide (2)";
	auto *occupant = obs_source_create(info.id, requested.c_str(), nullptr, nullptr);
	auto *source = obs_source_create(info.id, "Rename test target", nullptr, nullptr);
	simplecam::PresetSourceRenameState oldState;
	int oldAttempts = 0;
	for (int tick = 0; tick < 4; ++tick)
		oldAttempts += refresh(source, requested, oldState, false);
	check(oldAttempts == 4, "Original refresh retries on every tick");
	check(std::string(obs_source_get_name(source)) == requested + " 2",
	      "Real libobs produces the log's (2) 2 name");

	simplecam::PresetSourceRenameState fixedState;
	int fixedAttempts = 0;
	for (int tick = 0; tick < 2621; ++tick)
		fixedAttempts += refresh(source, requested, fixedState, true);
	check(fixedAttempts == 1, "2621 refreshes perform one collision-resolved rename");
	check(std::string(obs_source_get_name(source)) == requested + " 2",
	      "Fixed refresh preserves OBS's accepted name");
	check(std::string(obs_source_get_name(occupant)) == requested,
	      "Other source keeps its occupied name");

	const std::string orange = "<span style=\"color:#FF6050;\">" + requested + " *</span>";
	check(refresh(source, orange, fixedState, true), "Shared markup change still renames");
	check(!refresh(source, orange, fixedState, true), "Shared name settles");
	obs_source_set_name(source, "Operator rename");
	check(refresh(source, orange, fixedState, true), "External rename invalidates accepted result");
	check(!refresh(source, orange, fixedState, true), "Restored shared name settles");
	check(refresh(source, "Move Camera 1 to Another Preset", fixedState, true),
	      "Preset or group label changes still rename");
	check(!refresh(source, "Move Camera 1 to Another Preset", fixedState, true),
	      "Changed label settles");
	check(refresh(source, requested, fixedState, true), "Unsharing can resolve a collision again");
	check(!refresh(source, requested, fixedState, true), "Unshared collision settles");
	obs_source_release(source);
	obs_source_release(occupant);
	obs_shutdown();
	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
