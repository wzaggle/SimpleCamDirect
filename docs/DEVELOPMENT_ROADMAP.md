# SimpleCam Direct development roadmap

## Approach

Build a useful, testable product in stages over several months rather than designing every protocol and profile tool before the first user test. Each milestone must leave the previous one usable. Promote behavior only after its test gate passes; do not treat a schema check or successful compile as proof of camera support.

The version labels below are planning markers, not release commitments.

## Milestone 1 — SimpleCam Direct 0.1.x: AViPAS VISCA vertical slice

**Scope:** Windows only; one AViPAS AV-1280W; its proven VISCA connection and commands; no generic profile editor or camera-profile library UI.

Deliver one OBS plugin that can replace the current SimpleCamQt-plus-OBS-Lua workflow for this camera:

- Camera dock with the direct controls and named presets users rely on.
- Scene-embedded preset source with camera/preset selection, Program/Preview/Both behavior, delay, clear source naming, and duplicate-source handling.
- Preset naming, groups, visual order, and supported speed behavior needed for the current workflow.
- Camera setup limited to the settings required for this one Windows/VISCA/AViPAS target; no general profile authoring interface.
- Direct camera commands from the plugin, while keeping the SimpleCamQt application and current source plugin installable for side-by-side evaluation.

**Coexistence rules:** give the new OBS source type a distinct stable id; do not reuse an id that would make OBS conflate it with the Lua/file-bridge source. Keep settings, scene-source names, request-file behavior, and startup behavior separate. SimpleCamQt, SimpleCam Direct, and the camera's web interface are all valid command clients and must be able to control the same network-connected camera without an exclusive ownership lock. OBS must honor each preset source as it reaches it in source-list order, including mixed scenes containing legacy and native sources; do not suppress, merge, or deduplicate repeated requests. For example, a SimpleCamQt source can recall preset 1, a Direct source can recall preset 2, and another source can recall preset 1 again. The camera's web controls can also be used between or around those operations.

**Exit gate:** execute [`SIMPLECAMQT_PARITY_CHECKLIST.md`](SIMPLECAMQT_PARITY_CHECKLIST.md) against the current SimpleCamQt/Lua workflow on the same OBS version, Windows system, and AViPAS camera. Verify each function independently; test mixed legacy/native source requests in OBS order; confirm the web page, SimpleCamQt, and SimpleCam Direct can each control the same camera; save/reload scenes; and record any intentional difference. Do not start generalized profile editing until this vertical slice passes the user's hardware test.

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
