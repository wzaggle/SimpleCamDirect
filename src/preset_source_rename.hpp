#pragma once

#include <string>

namespace simplecam {

// OBS can accept a different name after resolving a collision. Keep that
// result stable until the generated request or the source's actual name changes.
class PresetSourceRenameState {
public:
	bool needsRename(const std::string &requested, const std::string &current) const
	{
		return current != requested &&
		       !(requested == lastRequested_ && current == lastAccepted_);
	}

	void remember(const std::string &requested, const std::string &accepted)
	{
		lastRequested_ = requested;
		lastAccepted_ = accepted;
	}

private:
	std::string lastRequested_;
	std::string lastAccepted_;
};

} // namespace simplecam
