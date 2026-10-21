# SimpleCam Direct project decisions

This document records the product decisions from the initial SimpleCam Direct planning conversation. Update it when the maintainer changes scope or behavior.

## Project identity

- GitHub repository name: `SimpleCamDirect`.
- Product display name: **SimpleCam Direct for OBS**.
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
- The **Camera Control** dock follows the current SimpleCamQt control experience: camera cards, live movement, and preset buttons. Its existing Settings mode keeps preset naming and visual button swapping.
- Camera model/profile selection and connection details belong in a separate dock named **Cameras** for now. It will cover brand/model, protocol, IP, port, serial port, baud, framing, and supported TCP/UDP choices as profiles require them. The UI can be renamed later.
- Each OBS preset source stores its own selected preset group and logical preset/button position. The action service resolves that selection through the group's visual map before recalling a physical camera preset. With 14 buttons per group and the default map, group 2/button 1 resolves to physical preset 15; visual swaps update the map without physically moving stored camera presets.
- Compare SimpleCamQt and SimpleCam Direct side by side during testing. SimpleCam Direct's dock and scene sources are one plugin and share one in-process action service; they do not communicate with each other through files or another IPC mechanism.

## Shared in-process action path and independent clients

- SimpleCam Direct scene sources and its camera dock invoke the same in-process camera action service and per-camera queue. No files, sockets, or other IPC connect these plugin components.
- SimpleCamQt, SimpleCam Direct, and the camera's own web interface remain independent valid clients and may command the same network camera. SimpleCam Direct must not place an exclusive ownership lock on a network camera. Separate clients are not globally serialized with each other; only Direct's own transactions are ordered by its queue.
- OBS scene-source events are submitted to the Direct service in OBS dispatch order. Never suppress, merge, or deduplicate repeated Direct preset recalls; a later source may intentionally move the camera again or back to an earlier preset. Do not claim deterministic ordering between Direct and legacy/file-bridge requests from separate clients.
- Serial-port sharing is platform/adapter dependent and is a separate OS-level limitation; it does not constrain TCP/IP camera clients.

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

- Camera profiles are JSON data, discoverable from a camera-profile directory and validated before being offered in the camera list. Users can edit/import JSON directly or use an in-plugin editor, and export a tested profile as a standalone `.camera.cfg` file.
- Profiles describe model-specific commands and transport details; physical-camera overrides and reusable model profiles use the same format.
- VISCA is first. ONVIF and NDI, when added, are PTZ-only initially. Protocols remain separate adapters over semantic camera actions.
- Command templates can use only registered typed variables and allowlisted formatters. JSON is configuration, not an executable programming language.
- Define and test the initial semantic actions: home, pan/tilt, zoom, preset store/recall, focus mode and direction, white-balance mode, backlight, exposure compensation, and camera power/standby as supported by the model. Manual movement and per-preset transition speed should be represented only where the profile and driver support them.
- A profile editor should show the most recent command sent and reply received while testing.

## Coding-agent and contribution expectations

- Every fork maintains its own root `AGENTS.md`, based on the upstream instructions, with fork-specific purpose, behavior changes, affected modules, platforms, and required evidence/metrics.
- Human pull requests use the minimum evidence template. AI coding agents provide enhanced evidence, including test counts and platform results; unavailable hardware checks are marked not run.
- Review fork-specific changes separately from generally useful improvements. A maintainer decides which useful changes return upstream.
