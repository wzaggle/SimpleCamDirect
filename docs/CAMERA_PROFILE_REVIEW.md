# Camera profile review and requirements

**Status:** profile contract draft based on manufacturer documentation; not a command certification for every model.

## Why the old VISCA JSON is not yet a camera profile

The working `SimpleCamQt/visca_commands.json` is a useful first-generation command-template file. It has shared command bytes, per-camera overrides, and a default address byte. It currently assumes raw VISCA command bytes over TCP, treats one port as a shared app setting, has no serial transport implementation, and does not declare per-model capabilities, firmware range, response expectations, provenance, or verification state. It should be a migration input/reference, not the new catalog format.

## What manufacturer documents show

| Vendor / family | Documented differences relevant to a profile | Profile implication |
|---|---|---|
| PTZOptics | Its current developer documentation separates serial VISCA, VISCA over TCP, and VISCA over UDP. It documents TCP and UDP setup independently, configurable ports, reply/ACK/completion behavior, and model command references. | `transport` and framing are first-class; port defaults are per protocol/model; represent reply policy and command capabilities. [VISCA reference](https://docs.ptzoptics.com/dev/visca-api/) · [getting started](https://docs.ptzoptics.com/dev/guides/visca-api-getting-started/) |
| AViPAS | AV-1280 family documentation lists RS-232 and VISCA over IP; later AViPAS manuals expose model-specific command tables and ACK/completion semantics. Other models add RS-485/ONVIF or change preset capacity. | Brand alone is insufficient. Capture exact model, hardware/firmware applicability, serial modes, network protocol/port and model limits. [AViPAS manuals](https://www.avipas.com/user-manuals) · [AV-1280 product](https://www.avipas.com/product-page/av-1280-10x-sdi-ptz-camera-w-poe) |
| Panasonic AW series | AW-UE100 publishes separate IP/Serial Control Interface Specifications and a Web Interface Control Supplement. The product may also carry NDI video, which is not the same thing as the camera-control interface. | Support multiple named control interfaces per model; don't infer a protocol from video output support. [AW-UE100 manuals and interface specs](https://connect.na.panasonic.com/av/video/ptz/aw-ue100-4k-ndi-ptz-camera) |
| FoMaKo X300 family | The manual lists RS-232/RS-485/RS-422, several baud rates, VISCA/Pelco-D/Pelco-P, network VISCA, ONVIF, optional NDI HX on some variants, and up to 255 protocol presets while the IR remote exposes 10. | Profile must separate transport-specific preset limits and optional capabilities. Serial settings need data/parity/stop/flow fields as well as baud and port. [X300 user manual](https://www.fomako.net/uploads/20250529/16e88f66add56841760cc3254a818e39.pdf) |
| Tenveo | Model pages and manuals vary across baud rates, supported serial protocols, network VISCA, and preset capacity; an example family lists VISCA/Pelco-D/Pelco-P with 9600/38400 baud. | Keep profile granularity at exact model or validated model family, and include firmware/hardware variants. [Tenveo camera catalog](https://www.tenveo.com/collections/ptz-cameras) · [Tenveo manual example](https://www.tenveo.com/uploads/pdf/VHD-PRO-H.pdf) |
| Sony | Sony publishes model-specific VISCA command lists. Some models expose RS-422 and configurable 9600/38400 baud; some newer network cameras do not support legacy VISCA RS-232/422. VISCA-over-IP address and response behavior are documented separately from serial use. | Do not mark an operation or transport supported across all Sony cameras. Bind commands and transport to a model/firmware variant. [BRC-AM7 support/manuals](https://www.sony.com/electronics/support/ptz-and-remote-cameras-ptz-cameras/brc-am7/manuals) · [Sony VISCA command list](https://pro.sony/s3/2022/09/14131603/VISCA-Command-List-Version-2.00.pdf) |
| BirdDog | Its current X1/X5 command references require an 8-byte VISCA-over-IP header, distinguish setting and inquiry headers, use UDP port 52381, and document model-specific speed ranges. | A VISCA payload template alone is not enough. Model the IP envelope, sequence field, and response parser as driver/framing metadata. [BirdDog X1 reference](https://birddog.tv/x1-series-visca-over-ip-commands/) · [X5 reference](https://birddog.tv/x5-visca-over-ip-commands/) |
| AVer | AVer's controller guide documents VISCA-over-IP UDP with port 52381, but its camera setup also requires model-specific protocol mode and serial address. | Network default port and address behavior vary by implementation; include exact connection mode and commissioning steps. [AVer control guide](https://www.averusa.com/pro-av/downloads/control-codes/AVer%20Pro-AV%20PTZ%20Visca%20over%20IP-UDP%20and%20RS-232%20Guide.pdf) |
| Canon | CR-N700 specifications list NDI HX2, Canon XC, standard communication protocol, and an RS-422 terminal. Those are separate integration paths, not evidence that all Canon models use the same VISCA commands. | A Canon profile may need a Canon XC driver in addition to any model-specific standard/VISCA option; treat each as an explicit connection variant. [CR-N700 specifications](https://ph.canon/en/consumer/cr-n700/main/product) · [Canon support/manuals](https://www.usa.canon.com/support/p/cr-n700) |
| Datavideo | PTC cameras may support VISCA over serial/IP and the separate proprietary DVIP protocol. Their protocol guides document command responses and command families. | Keep proprietary protocol drivers separate from VISCA; expose them only when supported and validated. [Datavideo PTC-305](https://www.datavideo.com/us/product/PTC-305) · [PTC VISCA protocol](https://www.datavideo.com/global/protocol/163/ptc-285-visca-control-protocol) |
| NDI PTZ | NDI is a transport/control API and media ecosystem, not a camera manufacturer or a VISCA byte-string variant. Its SDK exposes standardized receiver PTZ support detection and pan/tilt/zoom/focus operations; the camera may also expose a web-control URL. | Model NDI as a separate optional driver/SDK adapter with discovery/capability checks and its own dependency/license review. Do not put NDI commands into a VISCA JSON template. [NDI receiver PTZ docs](https://docs.ndi.video/all/developing-with-ndi/sdk/ndi-recv) · [SDK licensing](https://docs.ndi.video/all/developing-with-ndi/sdk/licensing) |

This list prioritizes the user's named vendors plus Sony, BirdDog, AVer and Datavideo because their official documentation exposes meaningful protocol or framing differences. It is a candidate support list, not a market-share ranking. Canon, Lumens, Marshall, Bolin, Minrray/OEM, and other brands can be added as evidence and community hardware arrive.

### Why port and framing must be model/transport-specific

The sources provide concrete counterexamples to one global VISCA default: PTZOptics describes TCP commonly on 5678 and UDP commonly on 1259; the user's tested AViPAS setup uses TCP 1259; BirdDog specifies UDP 52381 and an 8-byte envelope; AVer documents UDP 52381. A profile must identify transport plus framing, not merely say “VISCA over IP” or supply a global port.

## Profile design requirements

### 1. Identity and compatibility

- Stable profile id; schema version; manufacturer; exact commercial model; aliases; optional product family.
- Hardware revision and firmware applicability, including known-good tested versions and known exclusions.
- Human-readable display name, profile version, maintainer, and optional profile update URL.
- Evidence URLs to manuals, command references, firmware notes, and license terms if an SDK is involved.
- Verification status: draft, protocol-reviewed, hardware-tested, or maintained; test date and tester attribution.

### 2. Protocol and connection variants

One model can expose multiple connection choices. Each named variant should declare:

- Protocol family: VISCA, VISCA dialect, Pelco-D/P, ONVIF, NDI PTZ, DVIP, or future explicitly implemented driver.
- Physical/network transport: RS-232, RS-422, RS-485, TCP, UDP, ONVIF SOAP/HTTP(S), or NDI SDK connection.
- Required endpoint fields and defaults, with ports declared per variant (not global).
- Serial parameters: allowed baud rates; data bits; parity; stop bits; flow control; camera address; wiring/daisy-chain notes.
- Network parameters: host/discovery identity, port, TCP/UDP, IPv4/IPv6 support, packet framing, authentication, TLS, and camera-side mode/enable steps.
- VISCA-over-IP envelope details when needed: header type, payload length encoding, sequence counter, address handling and reply header.
- Response strategy: no reply, ACK then completion, inquiry reply, timeout, retry safety, serialization and cancellation rules.

### 3. Declared capabilities and limits

Declare each operation independently and its numeric limits: pan/tilt movement and speed model, home, optical/digital zoom including absolute zoom and its native position range, focus near/far/auto/manual, preset recall/store/clear, power/standby, backlight, exposure compensation, iris/shutter/gain, white balance, position inquiry, and device discovery. Include:

- Supported preset index range and any reserved slots, plus 0-based/1-based mapping.
- Pan and tilt speed minimum/maximum, curve (linear vs vendor curve), direction/stop encoding and whether values are shared or independent.
- Zoom/focus range and encoding; optical vs digital range only if query semantics support it.
- Per-operation required parameters, command variants, state prerequisites, and known unsupported states.
- Optional commands as absent/unsupported rather than copied from a generic VISCA table.

### 4. Safe command data model

- Use typed operation bindings and a known driver, not executable scripts.
- Command templates may contain fixed protocol literals plus only the registered typed variables and allowlisted formatters in `PROFILE_VARIABLES.md`. Validate variable type/range and exact packet shape; do not implement C `printf` or expression evaluation.
- Multi-packet actions declare order and response barriers (for example wait for completion before sending the next packet).
- Separate press/release or start/stop operations for continuous motion and focus/zoom.
- Mark destructive or disruptive operations (preset overwrite, clear-all, camera power/standby, reset) so UI confirmation and tests can be required.
- Keep IPs, COM/tty paths, credentials, camera names, and user presets outside distributed model profiles.

### 5. Catalog lifecycle and validation

- Built-in profiles ship under `data/cameras/`; per-user profiles live under an OS-specific user data directory; installation overrides must have documented precedence.
- Scan validated JSON profile files with `.json` or `.camera.cfg` extensions; enforce size, schema version, id uniqueness, and implemented driver/transport combinations. `.camera.cfg` is JSON content, not executable configuration.
- A syntactically valid profile is not automatically selectable: require supported driver, valid required fields, and an explicit eligibility state.
- Show validation errors by file/JSON path; isolate one bad file so it does not hide all valid profiles.
- Provide a “Validate Profiles” action and command-line validation mode for manufacturers and contributors.
- Never download or execute profile content automatically. User-initiated installation/import should show source and validation results.
- Keep logs secret-free and use atomic writes for profile/catalog updates.
- Support import, in-app creation/editing, local catalog save, and external export of the same profile document. A `.camera.cfg` profile is a reusable camera-model definition; camera endpoint and credentials remain per camera instance.
- For VISCA bindings, permit a bounded template string plus typed variables such as camera address, preset number, speed, and zoom position; preserve fixed prefix/postfix bytes and let the plugin map runtime values into declared output formats.

### 6. In-plugin editing and command debugging

The editor should make profile development practical without turning JSON into an executable language. The detailed interaction and test contract is in [`CAMERA_PROFILE_EDITOR.md`](CAMERA_PROFILE_EDITOR.md). Preserve these boundaries:

- Treat manufacturer/built-in profiles as read-only. Open a copy in a draft workspace and use **Save As** to create a user-owned profile with a new stable id.
- Keep physical camera connection settings (IP, port, serial device, credentials) separate from reusable profile data. A user may select a customized profile for a particular configured camera without embedding its endpoint in the profile.
- Offer separate **Validate**, **Simulate/Preview**, and **Test on Camera** actions. Valid JSON is not proof that a command is valid or safe for a device.
- Let users edit supported driver operations, parameters, and ordered command steps using protocol-aware fields; show the resulting encoded packet and response policy before transmission.
- Test only against an explicitly selected camera and profile connection variant. Use confirmation for movement and stronger confirmation for preset overwrite, clear, power, or reset operations.
- Do not expose arbitrary Lua, Python, shell, raw socket destinations, or unbounded loops/retries in a profile. If the driver cannot express the desired sequence declaratively, implement a reviewed driver capability instead.
- Report actual outcomes: encoded, sent, acknowledged, completed, timed out, rejected, or failed. Do not label a profile hardware-tested based on validation or simulation.

## Draft field map

The schema in `schemas/camera-profile.schema.json` is an initial contract, not a final implementation promise. It groups fields as:

| Section | Contents |
|---|---|
| `identity` | stable id, manufacturer, model, aliases, hardware/firmware ranges |
| `support` | maturity status, provenance, verification details |
| `capabilities` | operations and constraints supported on this exact profile |
| `connections[]` | serial/IP/SDK variants, framing, endpoint, serial settings, response behavior |
| `commands` | typed operation bindings and validated driver-specific data |
| `notes` | safe user setup notes and known limitations |

Before freezing v1, test the schema against actual profiles from at least: the user's AViPAS AV-1280W, a PTZOptics model with serial and network VISCA variants, a BirdDog model with its IP header, one Sony or Panasonic model, and an NDI PTZ adapter profile. Keep those as separate profiles where framing or capability differences exist.

The initial implementation scope is VISCA and the common action set in [`CAMERA_ACTION_CONTRACT.md`](CAMERA_ACTION_CONTRACT.md). ONVIF and NDI PTZ are later, PTZ-only adapters; do not treat their presence in this draft schema as implemented support. Their eventual editors should use protocol-native structured fields rather than VISCA raw-hex input.

The first action bindings are the requested Home, four-direction pan/tilt, zoom in/out and absolute zoom-to-position, preset store/recall by `N`, focus auto/manual and near/far, white balance auto/manual, backlight mode/behavior, and camera on/off or the model's accurately named standby operation. Manual movement speed, absolute zoom position/speed, and per-source preset transition speed are separate optional capabilities; see `CAMERA_ACTION_CONTRACT.md` for how profile ranges map to each.

Implement the protocol drivers incrementally: first VISCA on the user's AViPAS AV-1280W; then ONVIF PTZ only after checking whether that model/firmware advertises the required services; then NDI PTZ through its own adapter. Each phase retains the common action contract, adds only protocol-specific profile fields or registered typed values as needed, and passes the full regression suite for every previously supported driver. See [`PROTOCOL_EVOLUTION.md`](PROTOCOL_EVOLUTION.md).

## Implementation notes for the initial JSON loader

1. Parse and validate all discovered files at startup.
2. Build a catalog of eligible profiles plus diagnostics for rejected ones.
3. Let camera configuration select a profile and connection variant; save only the profile id/version and user-specific endpoint/settings.
4. At execution, resolve typed action → declared capability → driver operation → transport frame.
5. Log correlated state transitions without credentials.
6. Revalidate profiles on explicit rescan/import; avoid hot-swapping a profile while an action is in flight.

## Candidate support order

1. Sony-style VISCA fundamentals, but only per validated model profile.
2. AViPAS AV-1280W real hardware, as the user's known test camera.
3. PTZOptics and FoMaKo/Tenveo profiles, which exercise serial/network variants and mixed capability limits.
4. BirdDog and AVer, which exercise nontrivial VISCA-over-IP framing/ports.
5. Panasonic, Canon and Datavideo model profiles after exact model documents are reviewed.
6. After the VISCA action/profile workflow is established, add ONVIF and NDI as separate PTZ-only adapters. NDI is an optional SDK integration with structured metadata, not a camera JSON raw-hex dialect.
