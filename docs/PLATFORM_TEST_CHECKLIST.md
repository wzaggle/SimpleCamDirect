# Platform and camera test checklist

Use this checklist to locate a failure at the right layer: OBS/plugin load, profile/configuration, protocol, transport, camera behavior, or scene-event logic. A working web page only proves that the camera's web service works; it does not prove the selected PTZ control protocol, port, framing, or credentials are correct.

## SimpleCamPTZ 0.1.0 first-pass status (2026-10-21)

- Windows x64 CMake/Visual Studio build: **not run in this workspace**.
- OBS 32.2.1 plugin load and dock/source registration: **not run**.
- VISCA packet fixture tests: **passed locally** with GCC 13.3.0; these test packet bytes only.
- AViPAS AV-1280W transport and real-camera actions: **not run in this pass**.
- macOS and Linux: **out of scope for the Windows first pass**.

## 2026-10-24 — production-test package preparation

- Maintainer reports that the current SimpleCam Direct workflow is operating well enough to begin production-equipment testing.
- The AViPAS profile is marked `hardware-tested` for the maintainer-tested AV-1280W unit over raw VISCA/TCP. Firmware/revision were not recorded. Digital zoom behavior on that unit remains inconclusive and is explicitly outside the confirmation claim.
- The SimpleCamPTZ rename, migration handling, and Inno Setup installer are new in this source package. A Windows/OBS rebuild, installer compile, and install/upgrade test remain required on the maintainer's Windows development system before production use.

## Test record header

Capture this before each test run:

- Plugin release, git commit, build type, and package source.
- OBS version and architecture (x64, macOS arm64/x86_64, Linux x86_64/aarch64).
- Operating system/version, CPU architecture, locale, and permissions context.
- Camera manufacturer/model, hardware revision, firmware, configured protocol mode, camera address, and camera-side port/baud settings.
- Selected profile id/version/status and profile validation result.
- Selected connection id/transport/framing; network IP/port or serial device/baud/data/parity/stop/flow settings.
- Cable/adapter/switch path; power/PoE status; subnet/VLAN/firewall notes.
- Exact action, expected result, actual result, timestamp, and redacted OBS/plugin log excerpt.

Never put passwords, authentication tokens, public IPs, or sensitive network details into a public issue/log bundle.

## Gate 0 — protect production behavior

- [ ] Back up OBS profile and scene collection; first test in a copied OBS profile or disposable collection.
- [ ] Test Direct scene sources and dock controls together; confirm both call the same action service and per-camera queue without file or IPC communication between them.
- [ ] Confirm there is only one registration for each OBS source id/type. The Direct source id remains distinct from the legacy Lua/file-bridge source id.
- [ ] Use a safe camera direction and low speed for first movement tests; confirm the camera is not supporting a live production shot.
- [ ] Do not test power-off, reset, preset-clear, or preset-overwrite until the profile and destination camera are confirmed.

## Gate 1 — platform and plugin loading

### All platforms

- [ ] Plugin package matches the installed OBS version/ABI and CPU architecture.
- [ ] Package has the documented directory layout and all runtime dependencies.
- [ ] OBS starts with the plugin installed; plugin appears in logs without load/link errors.
- [ ] Dock/source registration is visible; open, close, detach, reattach, and restart OBS.
- [ ] Uninstall/upgrade does not delete user camera configuration or scene collections.

### Windows

- [ ] Verify x64 build, runtime DLLs, plugin search path, and Windows Defender/Norton/other security actions.
- [ ] Check firewall only for the chosen outgoing camera control path; document inbound requirements only if used.
- [ ] For serial, verify COM port is present and opened by only one process.

### macOS Apple Silicon

- [ ] Verify OBS and plugin are both arm64 or universal; confirm plugin is not accidentally x86_64-only.
- [ ] Check Gatekeeper/quarantine and signing/notarization behavior for the actual install route.
- [ ] Check macOS privacy prompts only where the feature requires them; serial device visibility/permissions.
- [ ] Validate dock behavior, text scaling, keyboard focus, and camera selection using the native OBS theme.

### Linux

- [ ] Record distro, release, packaging route (Flatpak/PPA/other), OBS build, and plugin ABI.
- [ ] Verify plugin search path matches the OBS installation type; don't mix host and Flatpak paths.
- [ ] For serial, verify device node exists, user group/ACL permits access, and no competing process owns it.
- [ ] Test package install/remove and logs from a clean user account.

## Gate 2 — profile and configuration

- [ ] Catalog validator sees valid built-in and user profiles.
- [ ] A malformed JSON file is rejected with file + JSON path; other good profiles remain available.
- [ ] Duplicate profile ids, unknown schema versions, unknown driver names, unsupported framing and out-of-range values are rejected.
- [ ] A profile with unsupported optional features displays those features as unavailable, not as failing buttons.
- [ ] The selected profile, camera connection variant, and camera identity persist after OBS restart.
- [ ] Profile selection is available from **Tools → SimpleCamPTZ Cameras**; the Camera Control dock stays present while the tool opens and closes.
- [ ] Selecting a compatible profile updates Camera Control and scene source actions without restarting OBS; missing commands fail with a readable status instead of sending a built-in fallback packet.
- [ ] Configure all five AViPAS slots independently, restart OBS, and verify each profile/name/IP/port/address persists. Confirm new slots start with blank IPs.
- [ ] Map each camera slot to its raw OBS video input, restart OBS, and confirm the mapping persists. Rename the OBS source and verify UUID-based mapping still resolves; remove/replace the source and verify the slot becomes unmarked rather than selecting a different source.
- [ ] Confirm no camera indicator is marked when no raw feeds are configured or none of the mapped camera feeds is visible in the active Program scene.
- [ ] In Dual mode, select different pairs from the five camera buttons; choosing a slot already shown on the other card moves that other card to the previous selection so no slot appears twice.
- [ ] Create preset sources for each of the five slots and verify the source property selector and recall target use the selected slot.
- [ ] A profile with fewer presets dims/unmaps entries beyond its declared capacity for that selected camera. Shared visual mappings must not be silently rewritten when a different-capacity profile is selected.
- [ ] User credentials/endpoints are not stored in public profile JSON and are redacted from logs.
- [ ] Rescan/import reports accepted profiles and rejected profiles separately.

## Gate 3 — connection and transport

### Network

- [ ] Confirm the camera's web UI responds, then separately confirm camera control protocol is enabled in its own settings.
- [ ] Verify host IP, protocol (TCP/UDP/HTTP(S)/NDI), port for that exact model/transport, subnet, route, VLAN, and firewall.
- [ ] Use a protocol-specific diagnostic/query where available; a TCP port test does not validate UDP or camera command framing.
- [ ] Confirm VISCA mode and address, or ONVIF/NDI discovery and supported-control flag.
- [ ] Confirm correct IP packet envelope/header/sequence counter if the model requires one.
- [ ] Observe reply path: no reply, ACK, completion, protocol error, or timeout; match it to the profile's declared response policy.
- [ ] In the Profile Test tool, run **Test All** only on the intended test camera; verify every configured command row reaches PASS/FAIL, the sweep continues after a failed command, and the trace preserves TX bytes, raw replies, and errors. Confirm the selected preset slot before allowing preset clear/store commands.

### Serial

- [ ] Verify the camera is set to the same control protocol and serial port mode.
- [ ] Confirm adapter/cable pinout and whether interface is RS-232, RS-422, or RS-485; don't infer wiring from connector shape.
- [ ] Confirm OS device path, baud, data bits, parity, stop bits, flow control, camera address, and duplex/topology.
- [ ] For serial, verify whether the OS/adapter permits more than one client to open the port; document an actual port-sharing limitation if present. Do not apply a serial-port restriction to TCP/IP camera clients.
- [ ] Confirm bytes are transmitted and, when supported, replies are received and parsed.

## Gate 4 — device command behavior

Run from a safe position; record each independently:

- [ ] Pan left, right, stop; tilt up, down, stop; diagonal movement; speed minimum, middle, maximum.
- [ ] Home.
- [ ] Zoom in/out/stop; distinguish optical versus digital range only if the camera reports it.
- [ ] Manual focus toggle and focus near/far/stop; restore auto focus.
- [ ] Preset recall for a known safe preset.
- [ ] Verify the selected profile's preset capacity; unavailable buttons are disabled and display `---`. If the physical model/firmware differs, select or create a separate matching profile rather than applying a per-camera capability override.
- [ ] For a partial group (for example, 25 usable presets), confirm the final group shows only 11 active entries and three disabled `---` entries.
- [ ] Verify group offset mapping at the boundaries: group 1 presets 1–14, group 2 presets 15–28, and the last enabled preset for the configured capacity.
- [ ] Preset store only to a test slot; wait for ACK/completion or explicit result; then recall and compare.
- [ ] Backlight/exposure compensation toggles only when profile declares them supported.
- [ ] Unsupported operations are absent/disabled with a useful explanation.
- [ ] Optional inquiries reflect real camera state; if not supported, UI does not show invented state.
- [ ] Multiple fast commands are serialized per physical camera; no dropped button-up/stop action.
- [ ] Disconnect/reconnect, timeout, malformed reply and camera rejection report a clear diagnostic without hanging OBS.

### Protocol addition regression

- [ ] Record the current baseline profile, command fixtures, and hardware-tested camera/firmware before adding another protocol adapter.
- [ ] Run all existing protocol profile-validation, encoding, response-parsing, and action-service tests after shared code changes.
- [ ] For each new protocol, verify discovery/capability claims on a specific model and firmware; do not infer PTZ support from video support or an open port.
- [ ] Verify new variables and serializers are protocol-scoped, typed, range-checked, and cannot shadow or change existing VISCA variables/formatters.
- [ ] Confirm old profile files without a template version continue to use version 1 with identical VISCA output; unsupported versions fail with a clear diagnostic.
- [ ] Record protocol-reviewed versus hardware-tested status separately for each profile and connection variant.

## Gate 5 — OBS source and scene behavior

- [ ] Add a named preset source; choose camera, logical preset, and configured trigger mode.
- [ ] Rename the source to confirm the displayed name clearly identifies camera and preset; duplicates are marked consistently.
- [ ] Test Program-only, Preview-only, and Both using a copied scene.
- [ ] With two mapped camera feeds stacked in the Program scene, verify only the highest visible mapped feed gets the dark-red number circle. Hide it and confirm the next visible mapped camera becomes marked; test nested scenes/groups and a scene with no visible camera feed.
- [ ] Verify a successful OBS recall displays “{Camera Name} moved to {Preset Name} by OBS” for five seconds, that another move replaces it, and that a failed recall does not claim success.
- [ ] Test configured delay and confirm a later source waits behind an earlier delayed request; verify Preview delay behavior against the intended new contract.
- [ ] Rapid scene transitions do not queue stale moves or send unintended commands twice.
- [ ] Scene collection save/reload preserves source configuration and camera association.
- [ ] SimpleCamQt, the legacy Lua/file-bridge plugin, SimpleCamPTZ, and the camera web page can independently command the same network camera without an exclusive ownership lock.
- [ ] Direct scene actions preserve their own OBS dispatch order, including repeated requests. Do not treat operations from separate clients as globally ordered.
- [ ] The camera web page remains usable alongside SimpleCamQt and SimpleCamPTZ; no plugin-imposed exclusive camera ownership or command lock blocks it.
- [ ] Reorder/swap visual positions changes the display mapping without changing physical preset slots unless the user explicitly chooses a physical preset operation.
- [ ] An unavailable camera/source fails visibly and safely; no command is sent to a different camera as a fallback.

## Gate 6 — profile/catalog contribution

- [ ] Run schema validator and catalog command-line check on the new profile.
- [ ] Verify command encoders with byte-for-byte fixtures from the linked manufacturer reference.
- [ ] Add negative tests for invalid hex/tokens, parameter boundaries, unknown driver/framing and missing response variants.
- [ ] State exact model, firmware, connection, OS, plugin and OBS versions used for real hardware testing.
- [ ] Require a maintainer review before changing the profile to hardware-tested/maintained.

## Failure isolation table

| Symptom | Most likely layer | Next discriminating check |
|---|---|---|
| Plugin missing from OBS | Package, ABI, architecture or plugin path | Inspect OBS log and installed file layout; confirm OBS/plugin architectures match |
| Dock/source loads but camera list is empty | Profile discovery/validation | Run profile validator; check directory, schema version, duplicate ids and driver availability |
| Camera web page works but no PTZ response | Control protocol/port/framing/configuration | Confirm camera-side VISCA/ONVIF/NDI mode, exact transport and port, then use a protocol-specific query |
| TCP connect succeeds but camera does not move | App-level protocol framing or command encoding | Compare transmitted packet and response with exact model reference; TCP success alone is insufficient |
| Serial port opens but no response | Electrical wiring, camera mode, address or serial settings | Verify pinout/interface, baud/data/parity/stop/flow, protocol mode, and competing port owner |
| ACK received but move never completes | Driver response/state sequencing or device rejection | Check completion/error response parser and command queue; don't substitute a fixed sleep |
| Dock buttons work but OBS preset source does not | OBS source lifecycle/event binding | Test source callback, trigger mode, camera/preset mapping and logs independently of dock |
| Program/Preview/Both mismatch | OBS transition event semantics | Record callback sequence for the test transition and assert the expected event policy |
| Labels stale but camera moves correctly | Profile/name sync or source presentation | Check preset-name snapshot/catalog refresh separately from transport |
| Works on Windows, fails on Mac/Linux | Portability/package/device permissions | Compare architecture, dependency ABI, package path, OS serial permissions and transport logs |

## Minimal release evidence

For each supported platform, report separate results for: configure/build, plugin load, profile validation, simulated/protocol fixture test, manual UI control, scene-source trigger behavior, serial (if supported), and real hardware. A platform is not “supported” based only on a successful compile.

## r13 production revision gate (2026-10-26)

See [PRODUCTION_REVISIONS_R13.md](PRODUCTION_REVISIONS_R13.md) for the full
operator checklist. Windows compile/OBS/camera results remain pending.

- [ ] Simple source labels with a current locale, and readable fallback with the older locale.
- [ ] No italic/bold tags in source names; referenced-source orange/* cue retained.
- [ ] Compact SimpleCam Properties, no clipping; other source dialogs unaffected; clean shutdown.
- [ ] Light-red on-air cues, muted-blue selection, accepted responsive/floating geometry retained.
- [ ] Inline rename Enter/Escape, refresh, camera/group switching and resize behavior.
- [ ] Two physical cameras move concurrently; same-camera recall/query order retained.
- [ ] Slider follows post-completion zoom and camera reassignment without feedback moves.
- [ ] Ctrl+Clear cancel/accept scope, retained physical maps, global page deletion guard.
- [ ] Recovered camera-specific names persist unchanged except deliberate edits.

## r17 macOS diagnostic gate

- [ ] Per-user PKG installs into the current user's OBS plugin directory.
- [ ] M1 initial placement, undock/redock, native/Qt title-bar comparison.
- [ ] Floating-limit toggle isolates whether bounds affect drag eligibility.
- [ ] Lock Docks remains respected; subsequent startup restores saved layout.
- [ ] Clean OBS shutdown/relaunch with diagnostics and optional Qt title bar.
- [ ] Camera/source behavior retains r16 results.

See MAC_DOCK_R17.md. Intel OBS-load/hardware checks remain separate.
