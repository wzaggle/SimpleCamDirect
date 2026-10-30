#include "on_air_policy.hpp"
#include <iostream>

int main()
{
	int checks = 0, failures = 0;
	auto check = [&](bool ok, const char *label) {
		++checks;
		if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
	};
	simplecam::OnAirPolicy policy;
	check(!policy.shouldSample(true, false), "Do not capture before OBS finishes startup");
	policy.initialize();
	check(policy.shouldSample(true, false), "Initialize Studio indication from Program");
	policy.sampled(0);
	check(!policy.shouldSample(true, false), "Studio polling holds the snapshot");
	check(policy.camera() == 0, "Preview changes cannot alter the held camera");
	check(!policy.shouldSample(true, true), "Retain outgoing during transition");
	policy.requestSnapshot();
	check(!policy.shouldSample(true, true), "A stop event cannot sample before libobs settles");
	check(policy.camera() == 0, "Outgoing stays indicated until completion");
	check(policy.shouldSample(true, false), "Pending Program event is consumed after settling");
	policy.sampled(3);
	check(policy.camera() == 3, "Completed transition updates to new camera");
	check(!policy.shouldSample(true, false), "Further Preview edits remain ignored");
	policy.requestSnapshot();
	check(policy.shouldSample(true, false), "Cut / same-name Program scene can resample");
	policy.sampled(-1);
	check(policy.camera() == -1, "Program with no mapped camera clears indication");
	check(policy.shouldSample(false, false), "Leaving Studio captures live state");
	policy.sampled(1);
	check(policy.shouldSample(false, false), "Non-Studio polls live visibility and order");
	policy.sampled(2);
	check(policy.camera() == 2, "Live edit updates camera");
	check(!policy.shouldSample(false, true), "Non-Studio transition also retains outgoing");
	check(policy.shouldSample(true, false), "Entering Studio initializes a new snapshot");
	policy.sampled(4);
	check(!policy.shouldSample(true, false), "Studio resumes holding");
	policy.reset();
	check(policy.camera() == -1 && !policy.shouldSample(true, false), "Scene collection cleanup clears and suspends");
	policy.initialize();
	check(policy.shouldSample(true, false), "New scene collection initializes again");
	policy.observeOutput("Program-copy-A", 1, false);
	policy.sampled(0);
	policy.observeOutput("Program-copy-A", 1, false);
	check(!policy.shouldSample(true, false), "Preview changes without output signals do not resample");
	policy.observeOutput("Program-copy-A", 2, true);
	check(!policy.shouldSample(true, true), "Direct output start signal still holds outgoing");
	policy.observeOutput("Program-copy-A", 3, false);
	check(policy.shouldSample(true, false), "Completion resamples same Program identity without frontend scene event");
	policy.sampled(1);
	check(policy.camera() == 1, "Same-scene commit can change ON-AIR camera");
	policy.observeOutput("Program-copy-B", 3, false);
	check(policy.shouldSample(true, false), "New same-named private output copy resamples even without transition signal");
	policy.sampled(2);
	policy.observeOutput("Program-copy-B", 5, false);
	check(policy.shouldSample(true, false), "Instantaneous transition between polls is detected by generation");
	policy.sampled(3);
	policy.observeOutput("Program-copy-B", 5, true);
	check(!policy.shouldSample(true, true), "Observed active transition holds even if signal was missed");
	policy.observeOutput("Program-copy-B", 5, false);
	check(policy.shouldSample(true, false), "Falling transition edge refreshes without frontend event");
	policy.sampled(-1);
	policy.observeOutput("Program-copy-B", 5, false);
	check(!policy.shouldSample(true, false), "After commit, Preview resumes leaving indication alone");
	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
