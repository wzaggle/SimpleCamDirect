#pragma once

#include <cstdlib>

namespace simplecam {

inline bool verboseUiDiagnosticsEnabled()
{
	static const bool enabled = [] {
		const char *value = std::getenv("SIMPLECAMPTZ_UI_DIAGNOSTICS");
		return value && value[0] == '1' && value[1] == '\0';
	}();
	return enabled;
}

} // namespace simplecam
