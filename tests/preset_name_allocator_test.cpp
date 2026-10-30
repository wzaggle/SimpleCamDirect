#include "preset_name_allocator.hpp"
#include <iostream>
#include <vector>

int main()
{
	using namespace simplecam;
	int checks = 0, failures = 0;
	auto check = [&](bool ok, const char *label) {
		++checks;
		if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
	};
	const std::string base = "Move Camera 1 to Stage";
	const auto orange = [](const std::string &text) {
		return "<span style=\"color:#FF6050;\">" + text + " *</span>";
	};
	PresetNameReservations empty(base);
	check(empty.lowestUnused(false) == 0, "First source can be unsuffixed");
	check(empty.lowestUnused(true) == 2, "Independent copy starts at two");
	PresetNameReservations gaps(base);
	for (int n : {2, 4, 7}) gaps.reserveIdentity(base, n);
	check(gaps.lowestUnused(true) == 3, "2,4,7 chooses 3");
	check(gaps.occupied(2) && gaps.occupied(4) && gaps.occupied(7), "Allocation preserves reservations");
	gaps.reserveIdentity(base, 3);
	check(gaps.lowestUnused(true) == 5, "Consecutive allocations fill next gap");
	PresetNameReservations shared(base);
	shared.reserveName(orange(base));
	check(shared.occupied(0), "Orange unsuffixed identity occupies base");
	check(shared.lowestUnused(false) == 2, "New source numbers when only orange base exists");
	for (int i = 0; i < 10; ++i) shared.reserveName(orange(base + " (2)"));
	check(shared.lowestUnused(true) == 3, "Repeated references reserve one number");
	shared.reserveName(orange(base) + " (3)");
	check(shared.lowestUnused(true) == 4, "Legacy suffix outside markup reserves number");
	shared.reserveName(base + " (4)");
	check(shared.lowestUnused(true) == 5, "Plain and orange both reserve");
	shared.reserveIdentity(base, 6);
	check(shared.occupied(6), "Pending rename reserves persisted number");
	shared.reserveIdentity("Move Camera 2 to Stage", 5);
	shared.reserveName("Move Camera 2 to Stage (5)");
	check(shared.lowestUnused(true) == 5, "Other request name does not consume a gap");
	check(presetSourceNumber(base, base + " (99999)") == 99999, "Upper bound accepted");
	for (const auto &tail : {" ()", " (-2)", " (0)", " (2x)", " (100000)", " (999999999999999)", " (2) extra"})
		check(presetSourceNumber(base, base + tail) == -1, "Malformed suffix ignored safely");
	check(presetSourceNumber(base, base + " (1)") == 1, "Legacy one recognized but never newly allocated");
	const std::string special = "Move A&B <C> to \"D\" &lt;";
	const std::string escaped = "Move A&amp;B &lt;C&gt; to &quot;D&quot; &amp;lt;";
	check(plainPresetSourceName(orange(escaped + " (2)")) == special + " (2)", "Entities decoded exactly once");
	PresetNameReservations escapedNames(special);
	escapedNames.reserveName(orange(escaped + " (2)"));
	check(escapedNames.lowestUnused(true) == 3, "Escaped orange suffix reserves identity");
	check(plainPresetSourceName("literal &amp;") == "literal &amp;", "Plain literal entities preserved");
	check(plainPresetSourceName("<span style=\"color:#FF6050;\">broken") == "<span style=\"color:#FF6050;\">broken", "Malformed wrapper preserved");
	PresetNameReservations exhausted(base);
	for (int n = 2; n <= 99999; ++n) exhausted.reserveIdentity(base, n);
	check(exhausted.lowestUnused(true) == -1, "Exhaustion does not reuse occupied number");
	check(exhausted.lowestUnused(false) == 0, "No suffix needed if only numbered copies exist");
	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
