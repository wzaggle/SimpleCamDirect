# Camera profile editor and command test workflow

## Goal

The OBS plugin should include a profile editor so a user can tune an existing camera definition, verify custom command sequences against real hardware, and keep a successful variation as a new profile. This shortens the debug loop for a new camera model while protecting the shipped definition and the user's saved camera connection.

This is a product and safety contract for the later profile-editor milestone in [`DEVELOPMENT_ROADMAP.md`](DEVELOPMENT_ROADMAP.md). It does not imply the editor exists in the current SimpleCamQt or file-bridge plugin, or that it is part of the first SimpleCam Direct 0.1.x vertical slice.

## Keep three kinds of data separate

| Data | Example | Storage and edit behavior |
|---|---|---|
| Shipped model profile | Manufacturer/model, supported protocol variants, command bindings, capabilities (including default usable preset capacity) and evidence | Installed profile catalog; read-only in the editor |
| User profile or custom variant | A tested adjustment to command steps or model quirks | User-writable camera profile directory; user may edit it or Save As another profile |
| Camera instance settings | Camera name, IP/port, serial device, credentials, selected profile and connection variant, plus optional effective preset-capacity override | OBS user settings for that physical camera; never copied into a shared profile |

There is one profile format for shipped and user-created profiles. A custom profile may be created for one physical camera, but it remains a normal reusable profile; the camera's address and other instance-specific settings are kept separately. The editor can be opened from a configured camera's profile selection or from the profile catalog. When editing a shipped profile, it creates a draft copy. **Save As** creates a distinct stable profile id and records the source profile id/version as its parent. It does not change the profile already selected by any camera. A user-owned profile may be saved in place only after validation; write it atomically and require explicit confirmation before changing a profile currently in use. Camera instances do not change profiles until the user explicitly applies one.

## Editor workflow

1. **Choose a profile and connection variant.** Show exact manufacturer/model, firmware applicability, protocol, transport, framing, and profile maturity. Select the configured camera to use for hardware tests.
2. **Create a draft.** Preserve the original profile. Show modified fields and retain provenance links and compatibility notes.
3. **Edit supported operations.** Provide both a guided structured editor and a direct JSON editor. Both edit the same draft, show changes in the other view, and run identical syntax, schema, semantic, and safety validation. The structured editor can create a complete profile from scratch; advanced users and camera manufacturers can paste, import, or edit JSON directly.
4. **Inspect encoding.** Show the effective parameter values, packet bytes or protocol request, transport framing, and expected reply behavior before sending. Make substitutions explicit and range checked.
5. **Validate.** Run JSON syntax, schema, semantic, duplicate-id, driver/transport compatibility, value-range, sequence-reference, and safety checks. Errors identify the JSON path or operation and block saving/activation where required.
6. **Simulate.** Render the sequence and expected response flow without sending to the camera. Simulation must be clearly labeled and must not raise the profile's hardware-test status.
7. **Test on camera.** Require a selected camera, connection variant, and explicit per-test action. Send through the same serialized camera action service as normal control. Display each step's encoded request, send result, ACK/completion/reply, timeout, or failure. Do not bypass transport framing or response parsing.
8. **Save or Save As.** Save a validated user profile atomically. Save As creates a new id, keeps source profile provenance, and starts at `draft` or `protocol-reviewed` according to its evidence; it cannot inherit `hardware-tested` or `maintained` status merely by copying metadata.
9. **Apply explicitly.** Let the user choose which configured camera instance uses the saved profile. Do not switch a running camera instance or scene source silently.
10. **Export/share.** Keep the profile in the local user catalog and allow export as a JSON-formatted `*.camera.cfg` file for sharing or manufacturer distribution. Also allow importing a selected `.camera.cfg` or `.json` file through the same validation and duplicate-id workflow. Export strips camera-instance addresses, serial device paths, credentials, and local test logs.

The in-plugin authoring form starts with VISCA. Its first operation set is defined in [`CAMERA_ACTION_CONTRACT.md`](CAMERA_ACTION_CONTRACT.md); users map those functions to supported VISCA commands and sequences. VISCA command definitions may use fixed byte literals plus registered typed placeholders and output formats; see [`PROFILE_VARIABLES.md`](PROFILE_VARIABLES.md). ONVIF and NDI PTZ are later editor modes and remain limited to PTZ functions. Their forms use protocol-specific structured request fields (including NDI metadata structures) rather than asking users to enter VISCA-style raw hex or raw SOAP.

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
