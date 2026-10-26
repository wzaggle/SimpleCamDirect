# Tested checkpoints

## 2026-10-24 — MARK IT: responsive Controls Dock through r9

### Accepted changes

- Replaced manual Single/Dual and top Settings toolbar with automatic one/two camera cards based on allocated content width. The black Settings gear follows the rightmost visible card.
- OBS owns docked sizing; removed explicit dock resize requests, global separator overrides, and custom resize grips. Maintainer confirmed resizing from both shared sides works.
- Cards remain centered and cap at 320 logical pixels; excess width is blank. Extra vertical space is below controls.
- Preset rows compress uniformly from 34 to 26 pixels and re-expand. Refresh retains the chosen height. Maintainer confirmed minimum-height behavior.
- Preserved growing/aligned OFF/ON controls and Ctrl-click power-off protection. Gear is painted directly black, bypassing icon tinting. Both changes passed maintainer testing.
- Floating-only outer maxima use the original compact dual width and full control height. Redocking releases them. Maintainer marked r8 PASS.
- r9 moves the gear to the rightmost visible card and marks the on-air camera title yellow; other titles stay green. Existing topmost-visible-feed matching and camera-button cues remain. Maintainer requested MARK IT after delivery.
- Product stays SimpleCamPTZ v0.1.0; repository remains private under its existing SimpleCamDirect name. Existing source id simplecamdirect_preset is preserved.

### Evidence and limits

- Maintainer compiled/installed iterations in Windows OBS and reported the passes above; these are user-reported runtime results.
- Workspace comparisons verified preservation of passing sizing, power and profile behavior across the final overlays. Delivery ZIP integrity and timestamps were checked.
- No Windows build or OBS runtime test was executed in this workspace. No new camera/protocol/platform compatibility claim is made. Shutdown-crash resolution was not separately confirmed in the final reports; check close/reopen before production rollout.
- Earlier source ZIP timestamps were one hour behind the maintainer local time, causing incremental MSBuild to skip replacements. ZIP entries are now written using America/Denver local wall time for this workflow. Verify camera_docks.cpp compilation and startup marker on each update.
- Current startup marker: build=r9-right-gear-yellow-onair-title.
- Final source: src/camera_docks.cpp + .hpp from r9; src/plugin-main.cpp from native-separators-r1. Other production files retained from the SimpleCamPTZ production-preparation source package.

### Superseded historical rules

The old manual Single/Dual toolbar, forced dual-width Settings, fixed 34px preset heights, and fixed outer dock heights are historical. The responsive contract above supersedes those rules wherever older planning documents still describe them.

## 2026-10-24 — SimpleCamPTZ production-test preparation

- Renamed the build/product to SimpleCamPTZ while retaining the OBS source type id `simplecamdirect_preset` and migrating the prior module's camera settings when needed.
- Marked the built-in AViPAS AV-1280W raw VISCA/TCP profile as `hardware-tested` for the maintainer-tested unit only. Firmware/hardware revision were not recorded; digital zoom remains inconclusive, and preset recall speed remains unsupported.
- Added a Windows x64 Inno Setup package workflow. The installer itself has not been compiled or run in this workspace; Windows build, package, upgrade, and production-equipment checks are pending.

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

## 2026-10-26 — r12–r14 production follow-up and cloud setup

- Maintainer confirmed camera Enabled checkbox and recovered independent preset
  names working after importing labels/maps from the production Qt registry
  export into production INIs. Those user settings are not repository content.
- r13 adds plain source labels/names, red on-air accents, stronger selected blue,
  inline renaming, local Ctrl+Clear, longer recall completion bounds and final
  zoom inquiries/caches. Individual new behaviors still require their operator
  acceptance checks; do not infer every pass from a successful build.
- r14 compact Properties passed maintainer testing. The supplied log showed
  720x464 initially, an accepted 720x280 resize, stable through 1500ms, and final
  minimum height 256px. Keep 280px. The key fix is deferred field identification
  after OBS's queued ReloadProperties, not a dock geometry change.
- Cloud setup removes that temporary Properties tracing and adds native build
  orchestration. Platform build results and first M1/Ubuntu OBS load remain
  separate gates. See CLOUD_BUILDS.md for exact operation and limitations.

## 2026-10-26 — r16 group-clear/delete accepted in Windows

- Maintainer reported the r16 overlay passes and authorized GitHub save/rebuild.
- Ctrl+Alt+Clear clears the active group on all camera slots after confirmation,
  including disabled slots; Ctrl+Clear remains local to the selected camera.
- Nonempty extra-group deletion offers Clear All and Delete or Cancel.
- Physical camera memory and surviving mappings stay unchanged; five base
  groups remain protected. Mac help uses Command/Option.
- Local runtime-state regression: 35 checks, zero failures. Dock source syntax
  check passed against Qt 6.4.2 and OBS 32.2.1. M1 OBS acceptance is pending.
- Prior cloud run 40358713833 compiled all platforms and passed all four suites
  on each. Mac packaging alone failed because lipo arguments were in the wrong
  order. Correct the packaging invocation and produce one same-revision set.

### Same-revision native cloud packages

Run 40365409463 completed successfully for e0963690927afaea393f9aa985bbd9a6482e5a34.
Windows x64, macOS arm64 and Ubuntu 24.04 x64 each compiled and passed four
automated suites (12 suite executions total). All installers were created,
and the collector checked the revision and test result of each OS package.
Mac ad-hoc signing and arm64 verification passed. M1 OBS-load, Mac modifier
behavior and physical-camera acceptance remain pending. See CLOUD_BUILDS.md.

## 2026-10-26 — r17 Mac-only diagnostic package

The maintainer confirmed M1 OBS loads the original plugin after moving it to
the user's Library, but it cannot redock. r17 investigates that issue without
claiming a root cause. Run 40393283631 compiled arm64, passed four suites,
created the home-domain PKG, actually installed it under /Users/runner/Library/
Application Support/obs-studio/plugins, compared binaries and verified signing.
Windows/Linux were deliberately not scheduled. M1 dock/undock and camera checks
remain pending. See MAC_DOCK_R17.md for controls, evidence and operator steps.

## 2026-10-26 — M1 r17 operator follow-up

The maintainer confirmed the control dock now docks and undocks with the normal
Mac title bar; the optional Qt title bar was not enabled. The OBS main window
is slightly too tall for the screen. Native-title-bar versus floating-constraint
causation is not isolated; retain r17 behavior for now and investigate height
separately. r18 adds only the requested startup zoom initialization.

## 2026-10-26 — r18 startup zoom native validation

Run 40402713949 for 1f6374e9f887dba8fd3ae5bff8ff5d9a555023cc passed
Windows/Mac/Linux compile, four suites on each, all installers and same-revision
collection. Mac user-domain install verification also passed. Local concurrency
regression reports 34 checks, zero failures, including the startup query cases.
Real camera startup slider acceptance remains pending. See STARTUP_ZOOM_R18.md.
