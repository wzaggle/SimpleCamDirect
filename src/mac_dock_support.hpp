#pragma once

#include <QString>

namespace simplecam {
class CameraControlDock;
// Used only by the macOS plugin target. OBS retains ownership of the dock.
void installMacDockSupport(CameraControlDock *controls, const QString &settingsPath);
void finishMacDockStartup();
void shutdownMacDockSupport();
}
