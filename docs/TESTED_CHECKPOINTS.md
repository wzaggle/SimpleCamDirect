# Tested checkpoints

## 2026-10-22 — Stable control/settings layout

### Changes

- Deferred dock height measurement until Qt had completed the control/settings widget reparenting and layout activation.
- Removed stale size constraints before measuring, then applied a single measured height to the dock content and dock.
- Added transition/request identifiers to the layout diagnostics so late or superseded resize work can be recognized.
- Kept shared preset-group display and the single/dual camera-card arrangement aligned across Controls and Settings.
- Documented that the dock title bar belongs to OBS/Qt; plugin styles remain scoped to its own content.

### Verification

- Maintainer rebuilt and tested the updated source in Windows OBS 32.2.1.
- Maintainer confirmed the Controls/Settings layout no longer compresses on repeated switching.
- Maintainer confirmed that a Restream dock shows the same close-button active/inactive color behavior. The close X is a host/theme state, not a SimpleCamDirect visibility defect.
- This workspace did not run a Windows build; the validation above is the maintainer's observed build and runtime test.

### Scope

- The checkpoint does not claim dual physical-camera hardware validation or support for protocols beyond the existing VISCA/TCP path.
- Serial, ONVIF, NDI, and camera profile editing remain future work.


## 2026-10-22 — Preset flash layout and color

### Changes

- Keep preset flash color painting independent from the preset button's style-derived size constraints.
- Restore the card layout after the flash so the first card remains correct without requiring a Single/Dual toggle.
- Change the flash start color to pure yellow, #FFFF00 (RGB 255, 255, 0); keep the established fade duration and steps.

### Verification

- Maintainer confirmed the card-layout/button-jump fix.
- The pure-yellow start color is saved in source but still awaits the maintainer's build and visual check.
