#pragma once

#include <cstdint>
#include <string>

namespace simplecam {

// UI-independent sampling rule. Preview edits never request a Studio snapshot.
class OnAirPolicy {
public:
	void initialize() { ready_ = true; pending_ = true; }
	void requestSnapshot() { pending_ = true; }
	void reset() { *this = OnAirPolicy{}; }
	void observeOutput(const std::string &identity, std::uint64_t transitionGeneration, bool transitioning)
	{
		if (!ready_) return;
		// Source identity, not its display name: same-named private Program
		// copies and instantaneous cuts can commit without a new scene name.
		if (!outputKnown_ || identity != outputIdentity_ || transitionGeneration != transitionGeneration_ ||
		    (wasTransitioning_ && !transitioning))
			pending_ = true;
		outputKnown_ = true;
		outputIdentity_ = identity;
		transitionGeneration_ = transitionGeneration;
		wasTransitioning_ = transitioning;
	}
	bool shouldSample(bool studio, bool transitioning)
	{
		if (!ready_) return false;
		if (!modeKnown_ || studio != studio_) {
			modeKnown_ = true;
			studio_ = studio;
			pending_ = true;
		}
		// TRANSITION_STOPPED may precede the end of transition audio. Keep
		// outgoing until libobs has promoted incoming B to settled SOURCE_A.
		if (transitioning) return false;
		return !studio || pending_;
	}
	void sampled(int camera) { camera_ = camera; pending_ = false; }
	int camera() const { return camera_; }
private:
	bool ready_ = false;
	bool pending_ = true;
	bool modeKnown_ = false;
	bool studio_ = false;
	int camera_ = -1;
	bool outputKnown_ = false;
	bool wasTransitioning_ = false;
	std::string outputIdentity_;
	std::uint64_t transitionGeneration_ = 0;
};
} // namespace simplecam
