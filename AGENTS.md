# Instructions for coding agents

## v0.2.0 four-platform packaging

Read docs/RELEASE_020.md. The maintainer now authorizes GitHub synchronization
and all four native installer builds, superseding earlier no-cloud iteration
restrictions for this task. Preserve the v0.1.14 runtime baseline. Intel and M1
need separate native builds/dependencies/packages. Do not deploy or publish a
public release. Production remains on v0.1.8; native acceptance is pending.


## Preset editing (v0.1.14; Windows acceptance pending)

Read docs/PRESET_EDITING_014.md. Save Rename when moving between buttons; Enter
saves/exits and Escape discards current edit. Blank names block dock/source
recall, including queued TCP/UDP sends, but Set and Rename remain available.
Do not auto-name newly stored mappings. Preserve camera memory and mappings.
Keep one local receive-port checkbox caption. No GitHub writes until MARK IT.


## Profile/installation split (v0.1.13; Windows acceptance pending)

Read docs/PROFILE_LAYOUT_013.md. Keep all profile metadata/ports/address/tuning in
Profiler upper Profile area; Test Connection edits only temporary IP. Cameras has
no port/address editors, only a read-only connection summary. Preserve legacy TCP
ports/addresses through user-profile migration before startup IO, including reuse,
restart and failure without endpoint loss. Keep installation names/IP/feed and
preset catalogs intact. Local UDP port may follow the camera port or be fixed;
never confuse it with reply source port. Profile revision is independent of plugin/
schema versions. Sony wire behavior is unchanged. No GitHub writes or cloud jobs
until requested; full source project / local Windows RelWithDebInfo first.

## Independent Camera Profiler (v0.1.12; Windows acceptance pending)

Read docs/CAMERA_PROFILER_012.md. Profiler is independent of slots: custom display
name/model, New draft, load/render/test/Save As; no Apply to Slot. Keep the three
explicit methods raw TCP, raw UDP and Sony-framed UDP separate from profile-owned
ports/timings. Never infer method from port or impose SRG-300H ranges on every
Sony-framed profile. Preserve the tested SRG profile, legacy TCP slot endpoints,
test ownership/Stop & Release, async pacing and draft/evidence boundaries. New
raw profiles are selectable by Cameras and run through the shared runtime.
No GitHub write until MARK IT; no cloud builds; full project for local VS first.

## Sony UDP Profile Test (v0.1.11; local Windows testing pending)

Read docs/PROFILE_TOOL_021.md. Profile Test now edits profile-owned Sony ports/
timings and uses the shared UDP adapter. Preserve temporary per-camera ownership,
STOP/release/cancel and draft Save As boundaries; never persist test suspension or
replay blocked scene requests. Individual Sony tests never auto-wake. TCP retains
its existing behavior. Maintainer reports v0.1.10 works well; new Profile Test
hardware checks pending. No cloud builds or GitHub writes until requested.

## Sony profile-owned connection requirements (v0.1.10; Windows testing pending)

Read docs/SONY_UDP_020.md. Maintainer confirms v0.1.9 zoom in OBS. Engineered UDP
ports/address/framing/timing/ranges belong to the reusable Sony profile; installation
slots provide name/IP/profile/video mapping only. Do not restore v0.1.9 editable UDP
overrides. Separate transport variants use separate profiles. Preserve raw TCP.
New SimpleCam VISCA bindings await hardware validation. IP preset capacity 255 is
maintainer-reported, high-index store/recall untested; do not call it independently
manufacturer-verified. Clear stays label-only. Keep explicit priority STOP and
track active pan/tilt/focus/zoom for failure/disable/unload stops. No GitHub write
until MARK IT, no cloud builds; source overlay for local VS first.


## Sony UDP first integration (v0.1.9; Windows acceptance pending)

Read docs/SONY_UDP_019.md. Keep raw TCP transport unchanged. Named Sony UDP adapter
uses shared listeners by local port, sender-IP/sequence filtering, any reply source
port, reset ACK + 100 ms setup, configurable per-slot gap/timeout, and priority STOP.
Only probe-backed zoom/STOP/readback and power inquiry bindings are enabled in the
Sony profile. Other actions are absent and unavailable. Do not claim full Sony
support from the standalone probe or simulator. Do not add sleeps to OBS threads.
Keep the v0.1.8 allocator and accepted ON-AIR/Properties behavior. Source overlay/local
VS first; no GitHub writes until MARK IT and no cloud jobs.


## Lowest unused preset suffix (v0.1.8; local VS/OBS validation pending)

New preset sources needing a suffix and Make independent copy share the gap-filling
allocator in preset_name_allocator.hpp. Start at (2); reserve stored identities,
plain names, orange names and legacy suffixes outside markup. References to one
source do not consume extra numbers. Never compact existing source numbers.
Global sharing/name refreshes do not allocate numbers. Loaded sources retain
their stored number; preserve legacy OBS suffixes on load. The copy action uses
its captured edited settings to choose the naming family. Preserve v0.1.7 title
cleanup, accepted ON-AIR, shared layout/610px width and camera IO.
27 allocator and 29 ON-AIR checks pass locally with GCC; full Qt/OBS compilation
and native VS/OBS acceptance are not run in this environment. Source overlay
only, Denver entry timestamps; no GitHub write until MARK IT, no cloud jobs.
See docs/PRESET_SUFFIX_018.md.

## Plain Properties title (v0.1.7)

After identifying our source Properties by marker/UUID, clean only the dialog
windowTitle, including subsequent WindowTitleChange events. Strip our generated
orange span and shared marker, decode escaped display characters once, retain
source numbers and localized host prefix. Guard reentrant title events. Never
rename the actual source or alter its orange marking for this display fix.
Keep accepted ON-AIR, sharing controls and 610px opening width unchanged.
Eight local title tests and source syntax checks pass; native VS/OBS check pending.
Local overlay only, no cloud build. See docs/PRESET_TITLE_017.md.

## Shared Properties opening width (v0.1.6)

Maintainer selected 610px initial client width for shared-source Properties.
Apply once per identified dialog, cap to available screen width, keep manual
resizing and leave unshared opening width to OBS. Preserve adaptive height and
accepted ON-AIR code. Local source overlay/VS only; no cloud jobs.

## Preset sharing caption (v0.1.5)

Maintainer confirms v0.1.4 ON-AIR works in all tested modes. Preserve its
implementation unchanged. Preset Properties now uses the independent-copy
button as the list heading, with no separate title. Exclude the captured
editing scene by root scene UUID from display only. Actual sharing detection
and replacement guards still count every reference. A single other scene
must keep the section visible; no other scene names can also mean repeated
references within the editing scene, so retain the button in that edge case.
Keep adaptive height and borderless scrolling. See docs/PRESET_LIST_015.md.
Local VS overlay only; do not trigger cloud builds.

## Current mode-specific tree (v0.1.4; local Windows validation pending)

Read docs/ON_AIR_MODE_014.md. Studio uses actual output transition/Program tree;
non-Studio uses obs_frontend_get_current_scene(). ON-AIR must not acquire or
enumerate Preview, including diagnostic scans. Preserve the v0.1.3 held-state
and same-scene commit detection. Other Preview preset-source functionality is
unchanged. Local policy tests: 29 checks pass; GCC/Qt syntax checks pass.
Send source overlays for local VS; do not start cloud builds.

## Same-scene commits (v0.1.3; operator test pending)

Read docs/ON_AIR_SAME_SCENE_013.md. Observe channel-0 transition signals using
an atomic generation only; never access Qt/camera IO in those callbacks. Observe
actual Program source identity and transition falling edges so unchanged scene
names still refresh the held indication. OBS 32 no-op GUI Transition/quick-button
clicks also request a snapshot through narrowly identified Studio controls.
Preserve outgoing until settled; Preview edits alone must not release the latch.
Independent-copy button now precedes scene names. Source overlays/local VS only;
no cloud jobs. Dock/build metadata is v0.1.3. Tests: 29 policy, 13 widget checks
pass locally; native VS/OBS acceptance remains pending.

## Current ON-AIR policy (v0.1.2; Windows operator test pending)

Read docs/ON_AIR_STUDIO_012.md. Studio Mode holds the startup/last completed
Program camera despite Preview edits; non-Studio Mode follows live state.
Mode switches and scene-collection initialization request a fresh snapshot.
Do not capture while the libobs output transition is active. Preview events
never request snapshots. The camera action dispatch path remains unchanged.
Local policy test passes 20 checks; plugin syntax passes GCC/Qt checks.
Maintainer builds in Visual Studio; do not trigger cloud jobs.

## Adaptive sharing section (r22)

Read docs/SHARED_PROPERTIES_R22.md. Maintainer passed the independent-copy
function, but requested a hidden unshared section, borderless plain scene names
and adaptive, screen-capped initial height. Build Windows only first; automatic
CI selection is windows until acceptance. Refit only on list/state changes, never
every timer tick. Keep replacement and naming behavior unchanged.

## Shared source copies (r21, awaiting operator validation)

Read docs/SHARED_SOURCES_R21.md before Properties/reference work. Capture scene
context at dialog Show, bind our dialog by source UUID, and replace only an
unambiguous direct item after closing. Use OBS Cancel to restore shared settings,
then duplicate with captured edits. Preserve item state and numeric copy suffix.
Never remove the original source or silently modify nested scene contents. All
queued work/timers must be cancelled before module unload. Keep existing source
id and camera action/IO behavior. No custom replacement Undo is provided yet.

## Project intent

This repository builds **SimpleCamPTZ**, an independent OBS PTZ-control plugin inspired by proven SimpleCam behavior. It is not the separate SimpleCamQt desktop controller and should not depend on that application, its Windows registry settings, or its temporary-file request protocol. The intended direction is direct in-process control from OBS source events and dock controls to a camera transport service. The private GitHub repository retains its original `SimpleCamDirect` name for now.

The plugin is human-maintained and may be developed with ChatGPT, Codex, or other coding agents. Treat these instructions and the linked design documents as project context. Read them before proposing or changing architecture.

## Required reading order

1. `README.md`
2. `docs/PROJECT_MAP.md`
3. `docs/CAMERA_PROFILE_REVIEW.md`, `docs/CAMERA_PROFILE_EDITOR.md`, `docs/CAMERA_ACTION_CONTRACT.md`, `docs/PROFILE_VARIABLES.md`, `docs/PROTOCOL_EVOLUTION.md`, `docs/UI_THEMING.md`, and `schemas/camera-profile.schema.json` for camera/profile/action work
4. `docs/DEVELOPMENT_ROADMAP.md` and `docs/SIMPLECAMQT_PARITY_CHECKLIST.md` for current milestone scope and its first exit gate
5. `docs/PLATFORM_TEST_CHECKLIST.md` for platform, packaging, or communication work
6. `docs/USER_GUIDE_PLAN.md` for user-facing changes

Also inspect the current source, build files, tests, and git history before editing. Documentation can become stale; source and tests establish current implementation behavior. If a source change conflicts with a stated product decision, report the conflict before silently choosing one.

## Product decisions to preserve

- Follow the product principle: hardware should be inexpensive, lightweight, and durable; software should be pretty, easy, and fully functional. Prefer useful, polished behavior over unnecessary feature or dependency weight.
- Preserve the active milestone scope in `docs/DEVELOPMENT_ROADMAP.md`. The first deliverable is Windows + VISCA + AViPAS AV-1280W parity, with no general profile editor in that milestone.
- Preserve scene-embedded named preset sources with camera selection, preset selection, preview/program/both behavior, delay, source-name clarity, and duplicate-source awareness unless an issue explicitly changes the behavior.
- Keep the SimpleCamQt-style live controls and preset management in the **Controls Dock**: camera selection for the visible cards, movement, preset recall/store, preset groups, naming, and visual swapping. Keep per-installation slot assignment and connection details in the **Camera Selection Tool**. Keep reusable model capabilities, protocol-specific behavior, and profile JSON editing in the **Camera Definition and Configuration Tool**. The current Cameras dock is an interim surface for the first vertical slice; it is not the target location for the full profile editor. A scene preset source stores its camera and logical preview number (1–14), not its own group. It reads the active group's names for source editing; the shared preview action applies the active group mapping when it calls the camera.
- Preset pages contain 14 button slots each. Start with five pages. The existing top `PG` button cycles through pages; do not add selector radio buttons. In Settings, put `+` and trash controls beside the active page-description field to add a page or remove one. Removal is allowed only when every button label on that page is blank and at least five pages will remain. Put `Clear` beside `Swap` in Settings; Clear blanks the selected button's label and never erases a preset from the camera. In Control mode, provide Set and Rename only. Resolve a 1-based preview number through the configured camera's page map. The default sequential map is `Preview + ((Preview Group - 1) * 14)`, but user layouts may distribute physical presets across pages. Respect camera capacity and show unavailable slots as disabled `---` buttons.
- Scene sources and dock controls must call the same in-process camera action service and per-camera queue. Do not add file-based or other IPC between these plugin components, or depend on a separate SimpleCamQt process.
- Do not reserve a network camera exclusively for SimpleCamPTZ. SimpleCamQt and the camera web interface remain independent clients. Preserve Direct source dispatch order and never coalesce repeated scene preset requests; do not claim global ordering across separate clients.
- Preserve the OBS preset source type id `simplecamdirect_preset` during the product rename so existing scenes continue to resolve. The installer replaces the old SimpleCam Direct module to avoid duplicate registration of this id.
- Separate UI, high-level camera actions, profile/catalog loading, protocol drivers, and transport I/O.
- Camera profiles are data. A JSON profile must never execute shell commands, scripts, arbitrary dynamic code, or unvalidated URLs. An optional ordered startup sequence may use only validated, driver-supported command operations and typed parameters. It runs after connection readiness and on reconnect; its steps must set explicit states rather than toggle them. A model exception becomes a separate profile (for example, AViPAS 1280X), not a per-camera capability override.
- Network transport, serial transport, VISCA, ONVIF, and NDI PTZ are distinct concerns. Do not assume that VISCA-over-TCP, VISCA-over-UDP, a VISCA-over-IP packet envelope, ONVIF, or NDI control are interchangeable.
- Profiles and command templates must identify model/firmware applicability, transport/framing, capabilities, and evidence. Unsupported operations must be disabled or reported as unsupported rather than guessed.
- Use Windows as the first implementation and behavior reference. Port only after the Windows implementation is tested. Test Apple Silicon separately from Intel macOS; then test Linux.
- Preserve an explicit migration decision for existing OBS scene sources. Never accidentally reuse the old `Camera_Preset2` source id in a way that makes the Lua script and native plugin compete for the same source type.

## Coding rules

- Keep work incremental and reviewable. Make one cohesive change at a time.
- Follow the repository's existing C++ standard, formatting rules, OBS plugin template, and dependency policy. Do not introduce Qt or another large dependency without a documented reason and maintainer approval.
- Never block OBS UI, frontend, source callbacks, or render threads on camera I/O. Use a serialized asynchronous command queue per physical camera, with explicit ownership and cancellation/shutdown behavior.
- Handle ACK, completion, inquiry, protocol-error, timeout, and partial/disconnected transport conditions according to the selected protocol driver. Do not replace response handling with arbitrary sleeps.
- Keep all camera-specific command bytes, parameter ranges, transport variants, and quirks in a validated profile or in a named protocol driver. Avoid scattered model-name conditionals.
- Profile templates may use only the plugin's registered typed variables and allowlisted formatters. Do not implement C `printf`, expression evaluation, or user-defined variables as executable behavior.
- Implement semantic camera actions before protocol bindings. Initial protocol authoring is VISCA; later ONVIF and NDI support is PTZ-only and must not add image-control features by assumption.
- Implement protocols incrementally behind separate adapters. New protocol work must preserve the existing VISCA profile grammar, encoded packet fixtures, action semantics, and hardware-tested behavior. Run all earlier protocol regression suites after shared-layer changes.
- Keep the camera dock an OBS-managed dock when floating. Docked content must follow the OBS theme exactly; optionally allow `SimpleCam Classic` accents only while floating. Redocking restores OBS styling. Scope plugin styling to its own widgets and never change the application-wide stylesheet or palette.
- Extend variable registries and serializers additively, with namespaced typed values and explicit format versions. Never change the meaning of an existing placeholder or formatter silently; reject unsupported format versions rather than guessing.
- Keep manual movement speed separate from per-preset transition speed. Preserve per-source transition-speed settings and only enable a speed control when the selected profile declares support.
- Validate untrusted profile input: schema version, required fields, protocol/transport combinations, byte templates, value ranges, duplicate identifiers, maximum sizes, and supported operations. Reject invalid profiles with a readable diagnostic; do not crash or partially activate them.
- Keep credentials and per-install network addresses out of distributed profiles. Store secrets using platform-appropriate secure storage where needed; redact them from logs.
- Log which profile id/version, transport type, and command operation failed, but never log passwords or sensitive credentials. Offer a safe diagnostic export that omits secrets.
- Changes to a camera profile require provenance links, compatibility claims, validation, and test-status labels. Never claim hardware validation from a schema check alone.
- Do not claim an OS/CPU architecture works until its build and runtime checks have passed. Distinguish compile-only, OBS-load, simulated transport, and real-camera test evidence.
- Keep public APIs, serialized OBS settings, source ids, and profile schema changes backward-compatible where practical. If a migration is needed, add a versioned migration and test it.

## Before editing

1. Summarize the requested behavior and locate the existing code path.
2. Identify affected UI, source lifecycle, driver, transport, profile, packaging, and documentation components.
3. State assumptions and risks, especially camera movement, preset overwrite, camera power, and backward compatibility.
4. Make the smallest coherent implementation.

## Before declaring work complete

- Build the affected target with the documented configuration.
- Run the closest automated checks (profile schema/catalog tests, command encoding/decoding tests, source settings migration tests, or platform build) and report what did and did not run.
- For camera protocol changes, include packet-level tests using documented examples and specify required real-camera checks.
- Update the relevant docs and the platform checklist when behavior, settings, profile fields, or supported platforms change.
- Report exact files changed, build/test results, unverified items, and any manual camera test the maintainer should run.

## Community contribution review

For a fork or pull request, identify which parts are generally useful to the core and which are fork-specific. Review profile provenance and real-hardware evidence. Do not merge generated command strings solely because they compile or pass JSON validation. Keep a contributor's fork-specific workflow isolated unless the core maintainer chooses to adopt it.

Every fork must maintain its own fork-local agent context in its root `AGENTS.md`. Start from this upstream file, retain the shared rules, and add a clearly marked `Fork-local context` section with the fork's purpose, behavior differences, affected modules, platform targets, and required validation metrics. Keep this section current as the fork changes. If a coding agent is used, it must read and update this section before changing code.

The fork-local instructions guide work in the fork; they are not automatically changes proposed for upstream. A pull request must include the fork-local context (or a link to the exact fork commit/file) and the evidence block in `docs/PULL_REQUEST_EVIDENCE_TEMPLATE.md`. Add fork-specific material to upstream `AGENTS.md` only when it is generalizable and the maintainer accepts it.

Human-led submissions may use the minimum evidence level. A PR missing a required entry is incomplete; use `N/A` with a reason when a metric does not apply. AI coding agents must follow the enhanced evidence level in `docs/FORK_AND_AGENT_CONTRIBUTIONS.md`; they must report test counts and platform results accurately, mark unavailable hardware tests as not run, and never infer success from a build or a profile parse.

## Current responsive dock checkpoint (2026-10-24, r9)

Read docs/TESTED_CHECKPOINTS.md before UI changes. Its current responsive
contract supersedes older fixed-size/Single-Dual toolbar descriptions. OBS
owns docked geometry; never reintroduce global separator styles or manual
resizeDocks calls to implement card selection. Content width selects one/two
cards, each capped at 320px; presets compress from 34 to 26px; the black gear
follows the rightmost visible card. Outer maximums apply ONLY while floating
and are released synchronously on redocking. Preserve the accepted OFF row.
Yellow camera titles indicate the existing on-air match, not guaranteed
compositing coverage. No match means normal green. Preserve source IDs.

The maintainer requires CODE IT before implementing a newly discussed change.
MARK IT authorizes saving the accepted source to GitHub with substantive
comments. Package overlays under unique ZIP names, with relative src paths.
For the current Denver Windows workflow, ZIP entries need America/Denver
local timestamps; ZIP timestamps have no time-zone metadata. Older timestamps
can silently skip MSBuild recompilation. Do not treat Install success alone
as evidence that changed camera_docks.cpp compiled or that OBS loaded it.

## Production revisions (2026-10-26, r13; awaiting Windows validation)

Read docs/PRODUCTION_REVISIONS_R13.md for current behavior and evidence.
On-air accents are now light red, selected camera squares muted blue; yellow
continues to mean a preset-action flash. Keep source labels readable when an
older locale is installed. Inline renaming is a child overlay, never a layout
row. Ctrl+Clear only blanks the selected camera/group labels after confirmation.
Do not overwrite the recovered per-camera catalogs or rerun a legacy import.
Recall completion precedes zoom inquiry in that camera's asynchronous queue;
other camera queues proceed independently. The source-specific Properties
adapter must never resize unrelated host dialogs or depend on private OBS ABI.

## Cloud build setup (2026-10-26)

Read docs/CLOUD_BUILDS.md before changing workflow/dependencies/packaging.
Build All Platforms runs native Windows x64, macOS arm64 and Ubuntu 24.04 x64
jobs for one source SHA, with tests before packaging and a verified combined
artifact. Seven-day retention and native job timeouts bound usage. Preserve
local Windows Install. Ordinary source commits do not trigger three builds;
CI infrastructure edits do. Never upload production INIs/registry exports.
Mac is ad-hoc signed for testing, not notarized; Linux targets native OBS,
not Flatpak/Snap. A successful CI build does not prove OBS load/camera behavior.
The current Properties adapter retains the r14 deferred probe/280px baseline
but removes its temporary geometry logs. Do not restore the Show-time-only
identification check: OBS queues its property field construction.

## Group clearing and deletion (2026-10-26, r16)

The maintainer authorized Ctrl+Alt+Clear to blank the current group's labels
across all camera slots, including disabled slots. Ctrl+Clear stays local to
the selected camera. Use Command / Command+Option on macOS under Qt's normal
modifier mapping. Do not change host modifier settings. Clear never sends a
camera preset erase or changes physical mappings.
Delete remains unavailable for the first five groups. Nonblank extra groups
now allow a Clear All and Delete / Cancel confirmation; clear names across
all cameras before calling the existing blank-only deletion guard. Cancel
must leave names and group count unchanged. Preserve five groups minimum.
See docs/GROUP_CLEAR_R16.md for validation and operator checks.

## macOS diagnostic boundary (2026-10-26, r17)

Read docs/MAC_DOCK_R17.md before Mac dock or package work. The maintainer
authorized a Mac-only dock experiment: one-time initial dock after OBS restores
layout, released floating maxima, passive diagnostics and optional Qt title bar.
Respect Lock Docks and saved layouts after that first placement. No Windows
window/size policy changes, global style overrides or native-handle creation.
The native-title-bar and size-limit causes remain unconfirmed until M1 tests.
The Mac package must target CurrentUserHomeDirectory, disable bundle relocation
and be installed/verified by CI before delivery. Preserve production INIs.
Manual Actions platform selection includes all/macos/windows/linux. Automatic
CI infrastructure pushes currently use ci/push-platform.txt (macos); restore all
for shared infrastructure work. Intel support remains a separate upcoming gate.

## Startup zoom state (2026-10-26, r18)

Startup/profile/endpoint refresh uses only the selected profile's zoomInquiry,
queued independently per camera after OBS FINISHED_LOADING. Preserve the
per-camera cache, generation guards and signal-blocked UI updates. Initialization
never moves/recalls/powers on a camera and does not disable controls on failure.
The constructor remains free of network activity; setters coalesce refreshes
only after startup initialization is enabled. See STARTUP_ZOOM_R18.md.
Shared-runtime validation uses all platforms again. Mac geometry is not changed.

## Linux floating dock diagnostic (2026-10-27, r19)

Read docs/LINUX_DOCK_R19.md before Linux dock work. Release dynamic floating
wrapper maxima on Linux by default, with reversible native/Qt title-bar and
limits diagnostics. Preserve OBS ownership, Lock Docks, saved layout and existing
minimum content constraints. Do not auto-dock or modify Windows/Mac paths.
Windows and M1 r18 startup zoom passed maintainer tests. Ubuntu functions pass
so far, but floating drag/edge behavior remains under investigation. Automatic
CI selection is linux for this scoped change; use all for future shared work.

## Program-only ON-AIR (r20)

Read docs/PROGRAM_ON_AIR_R20.md. The indicator must scan channel 0's actual
scene (including private Studio Mode copies), not the editable frontend scene.
Follow transition SOURCE_A to retain outgoing indication until completion;
get_active_source switches early to B. Never fall back to Preview on empty
output. Keep the 250ms poll and existing UUID/layer scan and camera IO unchanged.
Native libobs regression covers editable/private scene separation. Restore
all-platform CI for this shared change. Live Studio Mode acceptance is pending.
Linux drag lock reproduces with plugin disabled; xcb launch repairs it locally.
Do not keep changing SimpleCam geometry to fix that host/session interaction.

## Local iteration workflow (v0.1.1)

Maintainer now requests source overlays and local Windows VS builds for these
iterations. Do not launch further cloud/platform jobs until requested. Increment
the final component of the displayed version for each delivered iteration, using
buildspec.json as the compiled dock-title version and matching installer metadata.
v0.1.1 adds passive OnAirDiag evidence; Preview-sensitive ON-AIR remains unresolved.
See SHARED_PROPERTIES_R22.md.
