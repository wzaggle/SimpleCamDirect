# SimpleCam Direct development roadmap

## Approach

Build a useful, testable product in stages over several months rather than designing every protocol and profile tool before the first user test. Each milestone must leave the previous one usable. Promote behavior only after its test gate passes; do not treat a schema check or successful compile as proof of camera support.

The version labels below are planning markers, not release commitments.

## Milestone 1 — SimpleCam Direct 0.1.x: AViPAS VISCA vertical slice

**Scope:** Windows only; one AViPAS AV-1280W; its proven VISCA connection and commands; no generic profile editor or camera-profile library UI.

**Implementation status (2026-10-21):** the first code pass now contains the Windows CMake/Visual Studio scaffold, Camera Control and Cameras docks, a native preset source, asynchronous TCP VISCA queue, and packet fixtures. It has not yet been compiled in Windows/OBS or tested against the camera. The current UI is a deliberately reduced first vertical slice: advanced focus/white-balance and camera power controls remain unimplemented. This is not yet an exit-gate or release claim.

Deliver one OBS plugin that can replace the current SimpleCamQt-plus-OBS-Lua workflow for this camera:

- Camera Control dock with the direct controls and named presets users rely on. Keep preset naming and visual button swapping in its Settings mode.
- Separate dock named Cameras for camera/profile and connection configuration; initially expose only the fields required for the AViPAS VISCA target.
- Scene-embedded preset source with camera and logical preview selection (1–14); Program/Preview/Both behavior; delay; clear source naming; and duplicate-source handling. Source editing requests the preset names for the currently selected Preview Group; each source stores its preview number but not the group.
- Preset naming, five default pages, visual order, and supported speed behavior needed for the current workflow. The top `PG` button cycles pages. In Settings, put `+` and trash beside the active page-description field; trash removes only a fully blank page when at least five pages remain. Put Clear beside Swap in Settings; Clear blanks a label without erasing the camera's stored preset. Control mode keeps Set/Rename; Settings provides Swap/Clear/Rename.
- Camera setup limited to the settings required for this one Windows/VISCA/AViPAS target; no general profile authoring interface.
- Direct camera commands through one in-process action service shared by the plugin's scene sources and camera dock. Keep SimpleCamQt and the legacy file-bridge plugin available for side-by-side comparison; SimpleCam Direct does not depend on either one.

**Shared action path and independent-client rules:** give the Direct OBS source type a distinct stable id so OBS does not conflate it with the Lua/file-bridge source. Within Direct, scene sources and Camera Control dock buttons call the exact same typed camera action functions and enter the same per-camera queue; no file bridge or other inter-component messaging exists. Each preset source stores its camera, logical preview number (1–14), trigger mode, and delay, but not its group or physical mapping. The source editor lists names from the currently selected Preview Group. At runtime the shared preview action reads that active group and resolves the selected button through the configured camera's page map. The default map is sequential and maps group 2, Preview 1 to physical preset 15; custom layouts may distribute presets differently. Begin with five pages of 14 slots; add pages for larger capacities and remove only empty pages beyond the first five. Honor the camera's effective preset capacity; disable and label unassigned/out-of-range entries `---`. Preserve each Direct source's action in OBS dispatch order and never suppress, merge, or deduplicate repeated preset recalls. SimpleCamQt, the legacy plugin, and the camera web page remain separate clients that can each control the same network camera, but the plugin does not coordinate or guarantee a global order across those clients.

**Exit gate:** execute [`SIMPLECAMQT_PARITY_CHECKLIST.md`](SIMPLECAMQT_PARITY_CHECKLIST.md) against the current SimpleCamQt/Lua workflow on the same OBS version, Windows system, and AViPAS camera. Verify each function independently; test Direct sources and dock controls through the shared service, including repeated and ordered source actions; confirm the web page, SimpleCamQt, and SimpleCam Direct can each control the same camera; save/reload scenes; and record any intentional difference. Do not start generalized profile editing until this vertical slice passes the user's hardware test.

## Milestone 2 — Generic camera communication engine

Separate the AViPAS-specific command definitions from reusable communication mechanics. Build the command queue, per-camera serialization, asynchronous transport lifecycle, response/ACK/completion handling, timeout and error reporting, cancellation/stop behavior, and send/receive diagnostics as testable components.

Keep this milestone on the same Windows + VISCA + AViPAS hardware target. The purpose is to make the known camera path more reliable and separable, not to add another protocol yet.

**Exit gate:** all 0.1.x camera actions still work on hardware; packet/reply fixtures pass; rapid actions, stop-on-release, timeout, disconnect/reconnect, and OBS shutdown do not hang or send stale commands.

## Milestone 3 — Generalized VISCA profile JSON and variables

Move camera-specific facts that are expected to vary by model into the profile: command bindings, VISCA address/framing, native value ranges, speed mappings, preset limits, response policy, serial/network variants, capabilities, and source evidence. Apply the plugin-owned typed variable registry and bounded VISCA formatters. Keep generic transport and action behavior in the engine.

Initially create and validate the AViPAS profile directly as JSON. Do not require the editor to be complete first. Re-run the same single-camera VISCA test plan after moving each command from compiled code into the profile.

**Exit gate:** the profile-driven AViPAS build produces the same packet bytes and action behavior as Milestone 2. Include schema-positive and negative fixtures, unsupported-action behavior, min/max/boundary value tests, and an AViPAS hardware test.

## Milestone 4 — Camera profile editor and library; ONVIF trial

Add the profile catalog and editor for direct JSON and guided structured editing, validation, simulation, selected-camera testing, Save/Save As, import/export of `.camera.cfg`, and profile status/evidence. Keep user connection settings out of shareable model profiles.

Then test whether the AViPAS AV-1280W's exact firmware exposes usable ONVIF PTZ services. If it does, create an ONVIF configuration through the editor where practical and test the same semantic PTZ actions. If it does not, document the discovery result and use another camera for ONVIF testing rather than implying support.

**Exit gate:** profile JSON can be created, validated, saved, exported, reimported, and tested without changing the VISCA fixture results. ONVIF support is reported per exact model/firmware and only marked hardware-tested after real-camera tests.

## Milestone 5 — Second camera brand

Select one camera model from another manufacturer based on available hardware, reliable manufacturer command references, and a useful protocol mix. Create a VISCA profile and, where the camera supports it, an ONVIF PTZ profile. Use this camera to verify that differences belong in profiles/adapters rather than AViPAS-specific conditionals in the UI or action service.

**Exit gate:** repeat camera discovery/configuration, command tests, source tests, profile round-trip, and documentation for the second model. Preserve prior AViPAS VISCA and ONVIF regression results.

## Milestone 6 — External camera beta and profile contributions

Recruit external beta testers only after the core source/control workflow, first editor, and at least two camera profiles are stable enough to diagnose. Provide a compact intake checklist for model/firmware, protocol mode, transport, platform, OBS/plugin version, logs, exact test results, and profile provenance.

In parallel, review manufacturer manuals for additional high-following models and draft candidate profile files. Treat a manual-derived profile as `protocol-reviewed` only after command meanings and framing are checked against exact model documentation; mark it `hardware-tested` only after a tester reports the specific camera, firmware, transport, software versions, and results. Do not advertise a model as supported merely because a draft profile validates.

## Later protocol phase — NDI PTZ

Add NDI PTZ as a separate adapter after the staged VISCA and ONVIF work is stable and a supported NDI PTZ device/API is available for testing. NDI video capability alone does not establish PTZ control. Preserve the prior protocol test suites and add NDI-specific capability, metadata, packaging, and runtime dependency tests.

## Cross-milestone rules

- Keep the shared semantic camera action contract stable; make extensions additive and versioned.
- Keep protocol command encoding, transport, UI, and scene-source behavior independently testable.
- Every new protocol or command format reruns all earlier protocol fixtures and hardware regression checks.
- Maintain the fork-local and upstream coding-agent instructions, profile evidence, test metrics, and platform results as each milestone changes.
- Prefer a small complete milestone over beginning several unfinished protocols or configuration tools at once.
