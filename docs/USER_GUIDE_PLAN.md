# User guide plan

The product name is **SimpleCamPTZ**. Use this name consistently in future user-facing documentation, while distinguishing it from the existing SimpleCamQt desktop app and file-bridge plugin. The OBS dock for live control and preset management is called **Camera Control**; camera profiles and communication setup belong in a separate dock called **Cameras** (working label).

The full user guide should be written against the implemented UI, not guessed in advance. The outline below defines the required coverage and gives coding agents a stable target for later user-facing documentation.

## Planned end-user guide sections

1. **What it does** — native OBS plugin; direct PTZ control; named scene preset sources; supported feature and platform status.
2. **Before installing** — supported OBS versions, architecture, camera connection choices, supported profile status, safe first-test setup, and legacy Lua/plugin conflict warning.
3. **Install, upgrade, remove** — official release artifacts, manual install if appropriate, security prompts, troubleshooting plugin load, preserve OBS scenes/settings.
4. **First launch** — open/detach the camera dock, choose SimpleCam Classic or Match OBS appearance, add a camera card, choose a camera model/profile, select network/serial/NDI connection, save and test.
5. **Camera communication setup** — profile and effective preset capacity; network IP, per-transport port, protocol mode; serial COM/tty, interface type, baud, data/parity/stop/flow and address; how to verify each independently. Camera Control starts with five pages of 14 preset slots; the top `PG` button cycles pages. In Settings, `+` and trash sit beside the active page-description field; trash removes only a page with all 14 labels blank when at least five pages remain. Clear beside Swap blanks a button label without erasing the camera's stored preset. Physical presets can be distributed across pages; unassigned and unavailable slots display as disabled `---`. Control offers Set/Rename; Settings offers Swap/Clear/Rename.
6. **Camera cards, direct controls, and dock appearance** — select camera, pan/tilt/home, zoom/focus, manual movement speed, supported camera functions, status/errors, safe behavior, and unsupported controls. Explain that docked content follows OBS exactly and SimpleCam Classic is an optional floating-only appearance; the frame remains OBS-managed when floating.
7. **Preset groups and visual ordering** — group naming/color, logical/visual position mapping versus physical camera preset slots, Set/Rename/Swap/Manage behavior, and persistence.
8. **Create a scene preset source** — camera and named preset, Preview/Program/Both, delay, optional per-source preset transition speed when supported, visible source naming, duplicate marking, what happens on scene transitions.
9. **Manufacturer camera profiles** — built-in/user catalog locations, import/rescan/validation, JSON-formatted `.camera.cfg` files, profile status labels, model/firmware compatibility, and sharing/export.
10. **Edit and debug a camera profile** — open a profile read-only, make a draft, edit supported command parameters/steps, validate and simulate, safely test on a selected camera, inspect the live Sent/Received transaction trace and protocol responses, and Save As a new reusable profile.
11. **Technical appendix: create a camera profile** — schema version, identity/firmware, VISCA transport variants, semantic action bindings, capabilities/limits including manual and preset transition speed, sources/evidence, tests, validation command, package layout, and contribution PR checklist.
12. **Troubleshooting** — use the layered checklist in `PLATFORM_TEST_CHECKLIST.md`; explain how to capture a redacted diagnostic bundle and what to include in an issue.
13. **Privacy and safety** — local camera commands, credential handling, no profile code execution, immediate preset overwrite behavior in Camera Control, and safeguards for clear/power/reset operations.
14. **Glossary and support** — VISCA dialect/framing, ONVIF, NDI PTZ, RS-232/422/485, profile status, issue/PR routes.

## Writing rules for the finished guide

- Use the visible UI label exactly; include screenshots only after the interface is stable.
- State the tested OBS/plugin/OS/camera versions beside platform instructions.
- Separate “profile available,” “protocol reviewed,” and “hardware tested.”
- Never say a brand is supported generally when evidence applies only to one model/firmware/transport.
- Include safe test procedure before instructions that can overwrite presets or move a live camera.
- Cross-link error explanations to a test layer, not just a generic “check settings” message.

## Current limitation

This is an outline, not a release-ready user manual. The final text depends on the new project's camera configuration UI, profile directories, install method, and source migration decision.

## Current r13 operator changes

Document the implemented controls in PRODUCTION_REVISIONS_R13.md after Windows
acceptance: plain source labels/names, compact Properties, light-red on-air vs
muted-blue selected camera, inline Rename with Enter/Escape, post-recall zoom
inquiry, independent camera queues, and Ctrl+Clear for selected-camera/group
labels only. Physical camera memory is not erased by Clear. Do not describe
Ctrl+Clear as clearing all cameras.

## r21 shared preset sources

Document the Used in scenes list and Make independent copy action, with numbered
source identity and direct-versus-nested reference examples. Explain ordinary
shared edits versus independent copies. See SHARED_SOURCES_R21.md for current
behavior, boundaries and acceptance checks.

## r16 clear/delete update

Ctrl+Clear remains selected-camera/current-group only; Ctrl+Alt+Clear clears
that group on all camera slots after confirmation. On Mac use Command and
Command+Option. A nonempty extra group's Delete action offers Clear All and
Delete or Cancel. The first five groups remain protected. See GROUP_CLEAR_R16.md.

## v0.1.8 numbering update (operator validation pending)

New preset sources needing numbers and Make independent copy reuse the lowest
free suffix, starting at (2), within the same preset-request name. References to
one source keep one identity, and orange names still occupy their numbers.
Existing identities are not compacted. See PRESET_SUFFIX_018.md.


## Sony UDP v0.1.9 first integration

See [SONY_UDP_019.md](SONY_UDP_019.md). Windows local build/OBS/camera acceptance
is pending. Only zoom/STOP/readback and read-only power connection tests are in
scope. Per-slot listening port, minimum request gap and reply timeout are separate
UDP settings. Mac M1/Ubuntu validation follows Windows. No cloud jobs requested.


## Sony v0.1.10 profile-owned requirements

Supersedes v0.1.9 editable UDP settings. See [SONY_UDP_020.md](SONY_UDP_020.md).
Zoom in OBS is maintainer-confirmed. Added functions and 255-preset IP range await
hardware acceptance; capacity is maintainer-reported. Windows first, then M1/Ubuntu.


## v0.1.11 Sony UDP Profile Test

See [PROFILE_TOOL_021.md](PROFILE_TOOL_021.md). Test Open JSON, Sony port/timing
draft fields, Test UDP / Power, Render/Send, Save As and explicit catalog refresh/
profile selection. Verify normal controls are temporarily blocked for the tested
camera, other IPs remain independent, and Stop & Release/close restores control.
Begin with Power and Zoom Inquiry, then intentional movement and one disposable
preset. Test All requires its existing movement/state/preset confirmation. Check
Norton access for OBS itself; close the standalone probe. Repeat TCP smoke tests.
Windows VS/OBS first; Mac M1 and Ubuntu follow Windows acceptance.


## v0.1.12 Camera Profiler update

See [CAMERA_PROFILER_012.md](CAMERA_PROFILER_012.md). Camera Profiler replaces the
slot selector/Apply controls with independent name/model, load/New draft/Save As,
three explicit network methods and separate profile tuning. Saved profiles are
chosen in Cameras. Test IP stays private to the test session. New Profile uses
the loaded bindings/ranges as a starting template; full JSON/range authoring and
community submissions remain future work. Test Windows RelWithDebInfo/OBS first,
then M1 and Ubuntu. Repeat Sony and legacy TCP smoke tests; raw UDP requires its
own hardware acceptance. Preserve test ownership, Stop & Release and saved names.


## v0.1.13 profile/installation layout

See [PROFILE_LAYOUT_013.md](PROFILE_LAYOUT_013.md). Profile name/revision/file,
manufacturer/model/method/ports/address/tuning live in the upper Profiler Profile
area; Test Connection edits only temporary IP. Cameras has no port/address editors.
Verify read-only connection summary, legacy endpoint migration/restart/reuse and
preservation of names/IP/video/presets. Check UDP same/fixed receive-port policy
and method-dependent visibility, and Sony port/timing regression. Windows local
RelWithDebInfo/OBS first, then M1 and Ubuntu. Serial remains future work.

See PRESET_EDITING_014.md for v0.1.14 Rename and unnamed-preset acceptance checks.
