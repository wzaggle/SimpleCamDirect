# Camera slot enable and per-camera catalogs, r12

Each CameraRuntime persists camera/enabled, default true for existing slots.
Disabled commands fail before preset observers/mapping writes; the shared
transport also rejects queued requests and cancels existing requests. Delayed
start callbacks carry a generation token to prevent replay after re-enable.
Slots are retained in Cameras Tool for re-enabling; selector visibility and
on-air feed matching follow enabled state. Visible cards are reassigned to
distinct enabled slots; fewer than two enabled slots force one card. An empty
installation has disabled controls but remains configurable through Tools.

Previously sharedPresetOwner_ delegated both groups and whole preset catalogs
to Camera 1, preventing per-camera labels/maps. Runtime catalogs now remain
local while page selection delegates to the owner. PresetCatalog group linking
synchronizes page structure/names only; page deletion checks every member for
nonblank labels. Recall uses activePage(), fixing the old private-index bypass.

Migration version 2 seeds each secondary local catalog from the previously
exposed shared owner view. Ignored legacy local catalog values are backed up
under legacyLocalPresetPages. No claimed recovery of names never saved per
camera. Subsequent startup preserves each camera's own labels and maps.

Includes runtime-state test source and optional EXCLUDE_FROM_ALL test target;
not built or run in this workspace. Manual acceptance is listed in README.txt.
Preserves r11 geometry and diagnostics, floating limits, rightmost gear and
source identities. Locale supplies SimpleCamPTZ keys while retaining old ones.
