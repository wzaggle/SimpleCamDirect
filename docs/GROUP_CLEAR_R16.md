# Group clearing and deletion — r16

## Behavior

- Settings help: **Ctrl-Clear for all presets. +Alt for all cameras.**
- macOS help: **⌘-Clear for all presets. +Option for all cameras.**
- Plain Clear enters single-button label clearing.
- Ctrl+Clear (Command+Clear on Mac) confirms clearing the selected camera's
  current group labels.
- Ctrl+Alt+Clear (Command+Option+Clear on Mac) confirms clearing that same group
  on every camera slot, including disabled cameras. The dialog names the group.
- Delete is available for groups beyond the original five, even with names.
  A nonempty group explains the problem and offers **Clear All and Delete** or
  **Cancel**, with Cancel as the default. A blank extra group deletes directly.
- Only names are cleared. No camera commands are issued; physical preset
  positions and surviving visual mappings are retained. Deleted groups lose
  their local button layout as before. The original five groups are protected.
- Product version stays 0.1.0. Startup marker:
  `build=r16-group-clear-delete`.

## Build and test evidence

- Local Ubuntu/GCC C++17, Qt 6.4.2: runtime-state test compiled and ran,
  **35 checks, zero failures**. It uses temporary INIs and localhost only.
  The local link substitutes only OBS logging with a no-op; production camera
  runtime/profile code is compiled unchanged.
- `camera_docks.cpp` passed syntax compilation with Qt 6.4.2 and official
  OBS 32.2.1 headers, using the normal no-missing-field-initializers exception.
- Maintainer reported the delivered r16 changes PASS in Windows OBS on
  2026-10-26 and authorized this GitHub checkpoint. M1 UI/modifier and camera
  interaction checks remain pending; cloud build checks are a separate gate.
- ZIP integrity checked; entries stamped in America/Denver local wall time.
- User INIs, camera profiles, stored camera positions and build configuration
  are not part of this overlay.

## Operator acceptance

Use disposable test names in a sixth group on two or more cameras.

1. Ctrl+Clear, Cancel: no names change. Accept: only selected camera/group clears.
2. Add names again; disable one camera. Ctrl+Alt+Clear, Cancel: no names change.
   Accept: all slots clear that group, including the disabled camera.
3. Confirm another group's names and swapped button mappings remain intact.
4. Create names in group six. Delete, Cancel: names and group remain.
   Delete, Clear All and Delete: group disappears from every camera.
5. Verify Delete stays unavailable for groups one through five.
6. Restart OBS; confirm independent catalogs persist and scene source names refresh.
7. On M1, repeat with Command and Command+Option. Physical Control-click may
   be a context-menu gesture; no global Qt modifier setting is changed.
