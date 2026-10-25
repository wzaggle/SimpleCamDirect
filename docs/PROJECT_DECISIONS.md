# SimpleCamPTZ project decisions

This document records the product decisions from the initial SimpleCamPTZ planning conversation. Update it when the maintainer changes scope or behavior.

## Project identity

- GitHub repository name: `SimpleCamDirect`.
- Product display name: **SimpleCamPTZ for OBS**.
- The project is independent and human-maintained, with optional AI/coding-agent assistance. Public documentation must not imply ChatGPT endorsement.
- This is a new OBS plugin. It is distinct from the SimpleCamQt desktop application and the existing SimpleCam OBS file-bridge plugin.
- License selection is still open; do not invent a license or add one without a maintainer decision.

## Product intent

Hardware should be inexpensive, lightweight, and durable. Software should be pretty, easy, and fully functional. Preserve the proven SimpleCam workflow while moving camera actions into the OBS plugin so scene sources can command cameras directly.

## First milestone

- Windows only.
- VISCA only.
- AViPAS AV-1280W only.
- Match the working SimpleCamQt + OBS Lua workflow before adding general camera-profile editing.
- Scene-embedded preset sources retain named presets, camera/preset selection, preview/program/both behavior, delay, and clear source labels.
- The **Controls Dock** is the live Camera Preset Manager: select which configured camera slots its cards control, move cameras, recall/store presets, and manage preset groups, names, and visual order. Keep it an OBS dock.
- The **Camera Selection Tool** assigns a reusable profile to one of up to five installation slots and stores only installation-specific details for that physical camera, such as display name, network address/port, or serial device and settings. It tests and reports the connection. The existing Cameras dock is an interim first-sprint surface for these settings, not the target design.
- The **Camera Definition and Configuration Tool** manages reusable model profiles: model capabilities, supported protocol and transport definitions, command bindings, preset capacity, zoom limits, and optional startup sequences. It supports direct JSON and structured editing, validation, duplicate/Save As, import/export, and command testing. This is an OBS Tools window, separate from both the Controls Dock and Camera Selection Tool.
- SimpleCam's camera tools are not a general camera setup console. Users continue to manage digital zoom enablement, color/image tuning, move curves, and the camera's many other model-specific setup values through its own web/direct interface. SimpleCam profiles contain only the model/protocol facts and settings needed for SimpleCam's supported actions; a test such as Zoom Calibrate reads the current camera configuration but does not change it.
- An OBS preset source stores its camera and logical preview number (1–14), not a group or physical preset mapping. While editing, it obtains names for the currently selected Preview Group. At runtime, the shared preview action reads the active Preview Group and resolves the button through that camera's page map. The default map is sequential and equivalent to `Preview + ((Preview Group - 1) * 14)`; custom layouts may distribute presets across pages. The user selects the group they intend the source to recall.
- A camera profile defines its usable preset capacity and other model capabilities. Do not add per-camera capability overrides: if an exception is needed, duplicate the profile as a distinct model/variant profile (for example, AViPAS 1280X). Start with five 14-slot pages. The existing top `PG` button cycles through pages; do not add selector radio buttons. In Settings, `+` and a trash icon sit beside the active page-description field to add or remove a page. Remove a page only when all 14 button labels are blank and at least five pages will remain. A `Clear` button beside `Swap` in Settings blanks the selected button label; it does not erase the camera's stored preset. Control mode offers Set and Rename only. Users can distribute available physical presets across any page slots; unassigned/out-of-capacity slots are dimmed, disabled, and labeled `---`. A 25-preset profile can use any distribution across five pages. A 255-preset profile can use 19 pages.
- In the Controls Dock, Set immediately stores the current camera position in the selected physical preset, replacing any existing position without a confirmation dialog. Profile Test remains separately confirmation-gated for its destructive test commands.
- Compare SimpleCamQt and SimpleCamPTZ side by side during testing. SimpleCamPTZ's dock and scene sources are one plugin and share one in-process action service; they do not communicate with each other through files or another IPC mechanism.

## Shared in-process action path and independent clients

- SimpleCamPTZ scene sources and its camera dock invoke the same in-process camera action service and per-camera queue. No files, sockets, or other IPC connect these plugin components.
- SimpleCamQt, SimpleCamPTZ, and the camera's own web interface remain independent valid clients and may command the same network camera. SimpleCamPTZ must not place an exclusive ownership lock on a network camera. Separate clients are not globally serialized with each other; only Direct's own transactions are ordered by its queue.
- OBS scene-source events are submitted to the Direct service in OBS dispatch order. Never suppress, merge, or deduplicate repeated Direct preset recalls; a later source may intentionally move the camera again or back to an earlier preset. Do not claim deterministic ordering between Direct and legacy/file-bridge requests from separate clients.
- Serial-port sharing is platform/adapter dependent and is a separate OS-level limitation; it does not constrain TCP/IP camera clients.

## Configuration and operation surfaces

- Keep the Controls Dock as the live Camera Preset Manager. It owns camera-card selection, movement, preset recall/store, preset-group selection, naming, and visual reorder/swap.
- Use the Camera Selection Tool for the installation map: assign one exact profile to each of up to five slots and configure that unit's network/serial connection. It does not alter model capabilities or command definitions.
- Use a separate Camera Definition and Configuration Tool for reusable camera JSON profiles, profile duplication, validation, command testing, and import/export. A model/firmware exception is saved as a new profile (for example, AViPAS 1280X), never as a per-slot capability override.
- Allow an optional ordered startup sequence in a profile for model-specific setup not represented by normal dock actions. Run it when a slot's connection becomes ready and after reconnect; allow manual reapply and show sent/reply details. Keep it declarative, driver-validated, and repeat-safe by using explicit state-setting commands instead of toggles.
- The profile editor's exact startup-step schema remains open until the generic command engine and VISCA profile format are designed. This is a future profile feature, not a claim about the current plugin implementation.

## Planned milestones

1. Windows + VISCA + AViPAS AV-1280W parity and side-by-side testing.
2. Generic camera communication engine.
3. Expand the profile JSON with per-camera command data using a typed variable/format library; rerun single-camera VISCA tests.
4. Add a profile configuration editor and profile library that can validate, edit, save, and export JSON / `.camera.cfg` files.
5. Create and test an ONVIF profile for the AViPAS AV-1280W if the camera supports the required operations.
6. Add another camera brand and VISCA/ONVIF profiles where supported.
7. Recruit external beta testers; use manufacturer manuals to draft additional profiles, clearly marked untested until hardware validation.

The work is expected to progress over several months. Do not claim camera or platform support beyond recorded evidence.

## UI and platform direction

- Docked in OBS: follow the OBS theme closely so the dock looks native.
- Floating outside the OBS main window: optionally allow a SimpleCam Classic appearance for existing users; new users may choose an OBS-like appearance.
- Establish a tested Windows implementation first. Then test Apple Silicon separately from Intel macOS, followed by Linux. Do not report a platform as supported based only on compilation.

## Profile and protocol direction

- Camera profiles are JSON data, discoverable from a camera-profile directory and validated before being offered in the Camera Selection Tool. Users can edit/import JSON directly or use the Camera Definition and Configuration Tool, and export a tested profile as a standalone `.camera.cfg` file.
- Profiles describe model-specific capabilities, commands, transport variants, preset capacity, zoom limits, and optional startup sequences. A model exception is a distinct profile, not a physical-camera override. Physical slot connectivity remains in OBS user settings.
- A profile may include an optional ordered startup sequence for model-specific configuration that is outside the standard Controls Dock actions. It runs when the slot's camera connection is ready and on reconnect, can be manually reapplied, uses validated driver operations and the per-camera queue, and reports sent/reply results. Steps set explicit states rather than toggle them.
- VISCA is first. ONVIF and NDI, when added, are PTZ-only initially. Protocols remain separate adapters over semantic camera actions.
- Command templates can use only registered typed variables and allowlisted formatters. JSON is configuration, not an executable programming language.
- Define and test the initial semantic actions: home, pan/tilt, zoom, preset store/recall, focus mode and direction, white-balance mode, backlight, exposure compensation, and camera power/standby as supported by the model. Manual movement and per-preset transition speed should be represented only where the profile and driver support them.
- Manual pan/tilt speed does not imply preset recall speed. The AV-1280W test found that varying the speed fields in the Pan/Tilt Stop packet did not change the speed of a subsequent preset recall; keep preset recall speed unsupported for this profile unless a separately tested method is found.
- **Future design note — software-paced preset transitions:** When a user stores a preset through SimpleCam, query the camera's current pan/tilt position and persist those coordinates alongside that camera's physical preset mapping. For a later paced recall, use the saved coordinates as the estimated destination and send interpolated absolute pan/tilt positions at a speed-controlled rate. Treat the interpolated path as the main move and aim to reach roughly 99% of the target; issue the camera's native preset recall as the final lock-in step, expected to finish the small remaining distance without a noticeable jump. The camera's stored preset remains authoritative. Once movement completes, query the actual pan/tilt position and refresh the saved coordinates, allowing the estimate to self-correct over repeated recalls. If the estimate is stale enough to make the final correction visible, that is a known failure mode to test and reduce. When coordinates are unavailable or the profile lacks the required absolute-move/query support, use ordinary native recall. This is a future capability, not part of the current AV-1280W implementation.
- A profile editor should show the most recent command sent and reply received while testing.

## Coding-agent and contribution expectations

- Every fork maintains its own root `AGENTS.md`, based on the upstream instructions, with fork-specific purpose, behavior changes, affected modules, platforms, and required evidence/metrics.
- Human pull requests use the minimum evidence template. AI coding agents provide enhanced evidence, including test counts and platform results; unavailable hardware checks are marked not run.
- Review fork-specific changes separately from generally useful improvements. A maintainer decides which useful changes return upstream.

## 2026-10-24 accepted responsive Controls Dock

The r9 checkpoint replaces manual Single/Dual switching and the top Settings
row. Available width selects one/two centered cards, capped at 320px each.
Presets shrink uniformly 34–26px. Excess space is blank. The gear follows the
rightmost visible card. Docked outer size is OBS-owned; floating outer size
is capped to compact dual width and full control height, releasing caps on
redocking. Yellow camera titles augment existing on-air selection cues.
See TESTED_CHECKPOINTS.md for evidence and validation limits.
