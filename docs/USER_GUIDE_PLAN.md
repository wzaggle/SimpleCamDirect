# User guide plan

The product name is **SimpleCam Direct**. Use this name consistently in future user-facing documentation, while distinguishing it from the existing SimpleCamQt desktop app and file-bridge plugin. The OBS dock for live control and preset management is called **Camera Control**; camera profiles and communication setup belong in a separate dock called **Cameras** (working label).

The full user guide should be written against the implemented UI, not guessed in advance. The outline below defines the required coverage and gives coding agents a stable target for later user-facing documentation.

## Planned end-user guide sections

1. **What it does** — native OBS plugin; direct PTZ control; named scene preset sources; supported feature and platform status.
2. **Before installing** — supported OBS versions, architecture, camera connection choices, supported profile status, safe first-test setup, and legacy Lua/plugin conflict warning.
3. **Install, upgrade, remove** — official release artifacts, manual install if appropriate, security prompts, troubleshooting plugin load, preserve OBS scenes/settings.
4. **First launch** — open/detach the camera dock, choose SimpleCam Classic or Match OBS appearance, add a camera card, choose a camera model/profile, select network/serial/NDI connection, save and test.
5. **Camera communication setup** — profile and effective preset capacity; network IP, per-transport port, protocol mode; serial COM/tty, interface type, baud, data/parity/stop/flow and address; how to verify each independently. Preset buttons are grouped in pages of 14; entries above the configured camera capacity are disabled and shown as `---`.
6. **Camera cards, direct controls, and dock appearance** — select camera, pan/tilt/home, zoom/focus, manual movement speed, supported camera functions, status/errors, safe behavior, and unsupported controls. Explain that docked content follows OBS exactly and SimpleCam Classic is an optional floating-only appearance; the frame remains OBS-managed when floating.
7. **Preset groups and visual ordering** — group naming/color, logical/visual position mapping versus physical camera preset slots, Set/Rename/Swap/Manage behavior, and persistence.
8. **Create a scene preset source** — camera and named preset, Preview/Program/Both, delay, optional per-source preset transition speed when supported, visible source naming, duplicate marking, what happens on scene transitions.
9. **Manufacturer camera profiles** — built-in/user catalog locations, import/rescan/validation, JSON-formatted `.camera.cfg` files, profile status labels, model/firmware compatibility, and sharing/export.
10. **Edit and debug a camera profile** — open a profile read-only, make a draft, edit supported command parameters/steps, validate and simulate, safely test on a selected camera, inspect the live Sent/Received transaction trace and protocol responses, and Save As a new reusable profile.
11. **Technical appendix: create a camera profile** — schema version, identity/firmware, VISCA transport variants, semantic action bindings, capabilities/limits including manual and preset transition speed, sources/evidence, tests, validation command, package layout, and contribution PR checklist.
12. **Troubleshooting** — use the layered checklist in `PLATFORM_TEST_CHECKLIST.md`; explain how to capture a redacted diagnostic bundle and what to include in an issue.
13. **Privacy and safety** — local camera commands, credential handling, no profile code execution, warnings for preset overwrite/clear/power/reset, and limitations.
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
