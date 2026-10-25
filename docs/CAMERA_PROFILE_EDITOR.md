# Camera profile editor and command test workflow

## Goal

The OBS plugin should include a profile editor so a user can tune an existing camera definition, verify custom command sequences against real hardware, and keep a successful variation as a new profile. This shortens the debug loop for a new camera model while protecting the shipped definition and the user's saved camera connection.

This is a product and safety contract for the later profile-editor milestone in [`DEVELOPMENT_ROADMAP.md`](DEVELOPMENT_ROADMAP.md). It does not imply the editor exists in the current SimpleCamQt or file-bridge plugin, or that it is part of the first SimpleCamPTZ 0.1.x vertical slice.

## OBS tool boundaries

Keep three user-facing responsibilities distinct:

- **Controls Dock:** live camera movement and preset management, including selecting which configured camera slot each card controls, preset groups, set/rename, and visual swap operations.
- **Camera Selection Tool:** installation-specific setup for up to five camera slots. Assign one exact profile to a slot, enter only that camera's network or serial connection details, and test/retest the connection. It does not edit model command behavior.
- **Camera Definition and Configuration Tool:** maintain reusable camera model profiles. Provide structured and direct JSON editing, validation, profile duplication, import/export, command testing, and optional startup-sequence editing.

The Camera Selection Tool and Camera Definition and Configuration Tool are separate OBS Tools windows. They may link to each other, but profile data and installation connection data stay separate. The current Cameras dock is an interim implementation surface for the initial vertical slice; the target design moves installation selection/configuration to its tool and model-profile authoring to its own tool.

The Camera Definition and Configuration Tool is not a general replacement for the camera's web/direct setup interface. It captures only the model data SimpleCam needs for its own supported controls and scene-source actions. Camera setup such as digital-zoom enablement, color/image tuning, and vendor-specific move curves remains under the user's camera-side configuration unless a specific setting is later made an explicit SimpleCam action.

## Keep three kinds of data separate

| Data | Example | Storage and edit behavior |
|---|---|---|
| Shipped model profile | Manufacturer/model, supported protocol variants, command bindings, capabilities (including default usable preset capacity) and evidence | Installed profile catalog; read-only in the editor |
| User profile or custom variant | A tested adjustment to command steps or model quirks | User-writable camera profile directory; user may edit it or Save As another profile |
| Camera slot settings | Slot number, display name, selected exact profile id, IP/port or serial device and serial framing/settings, credentials, and connection variant | OBS user settings for that installation slot; never copied into the reusable profile |

There is one profile format for shipped and user-created profiles. If a model or firmware needs any capability or command exception, create a distinct profile (for example, duplicate AViPAS 1280 and save it as AViPAS 1280X). Do not add per-camera capability overrides. Camera addresses and other instance-specific settings remain in the slot configuration. The editor can be opened from the profile catalog or from the Camera Selection Tool. When editing a shipped profile, it creates a draft copy. **Save As** creates a distinct stable profile id and records the source profile id/version as its parent. It does not change the profile already selected by any slot. A user-owned profile may be saved in place only after validation; write it atomically and require explicit confirmation before changing a profile currently in use. Slots do not change profiles until the user explicitly applies one.

## Editor workflow

1. **Choose a profile and test camera.** Show exact manufacturer/model, firmware applicability, protocol, transport variants, framing, and profile maturity. Select a configured slot in the Camera Selection Tool for hardware tests.
2. **Create a draft.** Preserve the original profile. Show modified fields and retain provenance links and compatibility notes.
3. **Edit supported operations.** Provide both a guided structured editor and a direct JSON editor. Both edit the same draft, show changes in the other view, and run identical syntax, schema, semantic, and safety validation. The structured editor can create a complete profile from scratch; advanced users and camera manufacturers can paste, import, or edit JSON directly.
4. **Inspect encoding.** Show the effective parameter values, packet bytes or protocol request, transport framing, and expected reply behavior before sending. Make substitutions explicit and range checked.
5. **Validate.** Run JSON syntax, schema, semantic, duplicate-id, driver/transport compatibility, value-range, sequence-reference, and safety checks. Errors identify the JSON path or operation and block saving/activation where required.
6. **Simulate.** Render the sequence and expected response flow without sending to the camera. Simulation must be clearly labeled and must not raise the profile's hardware-test status.
7. **Test on camera.** Require a selected slot, connection variant, and explicit per-test action. Send through the same serialized camera action service as normal control. Display each step's encoded request, send result, ACK/completion/reply, timeout, or failure. Do not bypass transport framing or response parsing.
8. **Save or Save As.** Save a validated user profile atomically. Save As creates a new id, keeps source profile provenance, and starts at `draft` or `protocol-reviewed` according to its evidence; it cannot inherit `hardware-tested` or `maintained` status merely by copying metadata.
9. **Apply explicitly.** Let the user choose which configured camera instance uses the saved profile. Do not switch a running camera instance or scene source silently.
10. **Export/share.** Keep the profile in the local user catalog and allow export as a JSON-formatted `*.camera.cfg` file for sharing or manufacturer distribution. Also allow importing a selected `.camera.cfg` or `.json` file through the same validation and duplicate-id workflow. Export strips camera-instance addresses, serial device paths, credentials, and local test logs.

The in-plugin authoring form starts with VISCA. Its first operation set is defined in [`CAMERA_ACTION_CONTRACT.md`](CAMERA_ACTION_CONTRACT.md); users map those functions to supported VISCA commands and sequences. VISCA command definitions may use fixed byte literals plus registered typed placeholders and output formats; see [`PROFILE_VARIABLES.md`](PROFILE_VARIABLES.md). ONVIF and NDI PTZ are later editor modes and remain limited to PTZ functions. Their forms use protocol-specific structured request fields (including NDI metadata structures) rather than asking users to enter VISCA-style raw hex or raw SOAP.

## First-pass Zoom Calibrate test

Include a bounded **Zoom Calibrate** hardware test in the first Camera Definition and Configuration Tool pass. Run it against an explicitly selected, connected camera slot, but save the resulting model limit into the profile draft rather than silently creating a per-slot capability override.

For VISCA cameras that support zoom inquiry, the first pass should:

1. Show the target camera and the current digital-zoom mode; require the operator to confirm the camera can safely zoom to telephoto.
2. Query and retain the starting zoom position.
3. Start a continuous Zoom Tele command once at the configured calibration speed.
4. Continue issuing serialized zoom-position inquiries while zooming. When valid replies stop increasing for the configured stability window, immediately issue Zoom Stop and make one final inquiry to capture the endpoint.
5. Restore the original zoom position after recording the result. If restore fails, report that clearly and leave the camera stopped.
6. Save the measured native endpoint only to the unsaved profile draft, alongside firmware, digital-zoom mode, connection variant, timestamp, and the sent/reply trace. The user explicitly saves or discards the profile change.

The polling loop must have a conservative timeout, a visible Cancel/Stop control, and cleanup that sends Zoom Stop on every exit path, including a timeout, inquiry error, connection loss, or cancellation. Do not keep retrying an unacknowledged move. The calibration transaction must permit inquiries while the continuous zoom drive is active; it must not block the queue waiting for move completion before it can send the inquiries or stop command. If the selected camera rejects inquiries during movement, fail safely and record that result rather than switching to an unbounded command loop.

For a camera with a digital-zoom toggle, the user sets that mode through the camera's own web/direct interface before calibration. SimpleCam must not toggle digital zoom or other camera setup values. The calibration measures and labels the endpoint for the current mode only. If the profile needs both the optical-only endpoint and the combined endpoint, the user can configure each mode externally and run the measurement again; the tool must not infer one endpoint from the other. A wide-end reading of zero may be used as the expected minimum only after the tool has confirmed it by inquiry.

## Zoom Calibrate acceptance checks

- The Sent/Received trace shows the initial inquiry, one continuous tele-start request, every position inquiry, the stop request, final inquiry, and restore request/reply.
- Calibration stops only after valid position replies have ceased increasing for the stability window; unchanged, malformed, missing, or error replies are not treated as a successful endpoint.
- Timeout, user cancellation, and connection loss all issue a best-effort stop and do not save a new profile value.
- A successful run saves the measured endpoint to the draft with digital-zoom mode, camera firmware, connection variant, and test status; it does not update the active profile until explicitly saved/applied.
- The original zoom position is restored, or the UI reports that restoration failed and the camera remains stopped.
- Digital zoom is never changed by the calibration; the endpoint is tagged with the externally configured mode, and separate mode endpoints require separate operator-configured runs.
- The calibration does not change preset data, pan/tilt position, camera power, or other profile settings.

## Optional startup sequence

A camera profile may define an optional ordered sequence of setup operations for model features that SimpleCamPTZ does not expose as regular Controls Dock actions. Examples include selecting a default speed or setting/clearing a vendor-specific feature. An empty or absent sequence is a no-op.

- Run steps in order after the configured slot's camera connection becomes ready, including after reconnect; provide an explicit **Run startup sequence** action for deliberate reapplication.
- Use only validated, driver-supported operations, typed values, the selected profile's normal transport, and the normal per-camera serialized queue. The sequence is data, never executable code or an unrestricted raw-packet escape hatch.
- Prefer explicit state-setting operations over toggles so repeated startup runs converge on the same state.
- Show each step's sent request and received reply (or `No reply expected`, timeout, or error) in the Camera Selection Tool. Report failures clearly and do not silently claim the profile was applied.
- Startup operations configure the camera; they do not automatically create new buttons or semantic controls in the dock. A custom supported action is still a separate product/API design decision.

The schema field names and exact step representation remain to be designed with the VISCA command sequence schema. This design decision does not imply the startup sequence is implemented in the current 0.1.x plugin.

Implement those editor modes in protocol phases. Verify VISCA first against the AViPAS AV-1280W; then add ONVIF only if that camera advertises usable PTZ services, otherwise test ONVIF against another documented camera. Add NDI PTZ after that as a separate adapter. Every new adapter must pass its own tests and rerun all earlier protocol fixtures and hardware smoke tests. See [`PROTOCOL_EVOLUTION.md`](PROTOCOL_EVOLUTION.md) for phase gates and compatibility requirements.

## Command editing boundary

Profiles remain declarative data. Users can choose from driver operations and edit only driver-supported fields: parameter values, allowed byte/template values, declared sequence steps, and response rules. The profile schema should eventually express sequence steps as structured records rather than unconstrained strings, including:

- operation or driver command reference;
- typed parameters and bounded substitutions;
- order and any required ACK/completion barrier;
- timeout and retry semantics supported by the driver;
- whether the step can move the camera or change persistent camera state.

No profile may execute Lua, Python, shell commands, compiled code, arbitrary loops, or arbitrary URLs/IP destinations. A desired sequence that cannot be represented using the reviewed driver interface is a driver-development task, not an escape hatch for executable JSON.

## Test safety and user feedback

- Keep profile validation, packet simulation, connection check, and command-on-camera as distinct actions. A successful earlier stage is not proof of a later one.
- While a camera test is running, show a compact live transaction trace in the configuration/editor window. Keep one **Sent** row for the latest outbound request and one **Received** row for its response. Include a timestamp, operation, transport, packet/request bytes, and decoded response when available; allow expansion to inspect the full ordered sequence.
- Correlate sent and received rows by transaction id so replies cannot be mistaken for another command's response. For multi-stage protocols, show stages such as `ACK → completion` and whether completion was observed. For protocols with no response, say `No reply expected`; for an expected but missing response, show `Timed out` or the specific transport/protocol error.
- Keep the trace scoped to the selected camera and the editor's explicit test action. It is diagnostic display, not a raw command console. Never expose credentials or allow the user to inject arbitrary packet bytes outside supported driver operations.
- Pan/tilt/zoom/focus tests must be bounded and visibly identify the target camera. Provide a stop action and do not run unattended movement loops.
- Require stronger confirmation for preset store/overwrite, preset clear, power/standby, and reset operations. Display the target camera and preset slot before sending.
- Run commands through one ordered queue per physical camera. Do not block OBS UI, source callbacks, or rendering while waiting for I/O.
- Redact credentials and sensitive settings from packet logs and diagnostic exports. Show protocol bytes only where they contain no secret material.
- Label test results precisely: `validated`, `simulated`, `sent`, `acknowledged`, `completed`, `timed out`, `rejected`, or `transport error`. Only real-camera results record hardware model, firmware, connection variant, OS/architecture, plugin version, and tested operations.
- Keep a test log attached to the draft for the current editing session. Do not silently promote it to the shared or maintained catalog.

## Acceptance checks for a future implementation

- A built-in profile cannot be overwritten by editing or Save As.
- Both the structured editor and direct JSON editor produce the same validated draft; invalid JSON cannot be saved or partially applied.
- Save As generates a new unique profile id, preserves provenance, and leaves the source and existing camera assignments unchanged.
- A valid profile can be saved in the local catalog and exported/imported as JSON-formatted `.camera.cfg` without exporting camera connection data or credentials.
- Invalid syntax/schema or unsupported driver/transport combinations cannot be applied.
- A draft can be simulated without network or serial I/O.
- A camera test uses the configured camera instance and selected connection variant, serializes with other commands, and reports response stages accurately.
- The live trace shows the matching send and reply for the selected camera, correctly distinguishes expected silence from a timeout, and preserves ACK/completion order.
- Destructive tests require explicit confirmation and identify the affected camera/state.
- Profile changes are written atomically and revalidated after saving or reloading.
- Secrets, camera endpoints, and serial device paths do not leak into shared profiles or exported diagnostics.
- A profile remains `draft` unless provenance and review requirements are met; hardware status requires recorded real-device evidence.

## Open implementation questions

- The precise JSON editor library and how diagnostics synchronize with the structured form.
- Exact VISCA sequence-step schema and supported operation bindings for the first implementation.
- Later ONVIF and NDI PTZ structured editor fields and the supported NDI metadata subset.
