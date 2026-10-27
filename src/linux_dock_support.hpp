#pragma once
namespace simplecam {
class CameraControlDock;
// Linux-only experiment. OBS retains dock ownership and saved placement.
void installLinuxDockSupport(CameraControlDock *controls);
void shutdownLinuxDockSupport();
}
