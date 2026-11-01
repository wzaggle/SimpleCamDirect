# Profile command templates and variable library

## Goal

Camera profiles specify fixed protocol bytes or fields while the plugin supplies runtime values such as camera address, speed, preset number, and target position. The profile format remains declarative data, not a programming language.

Use a constrained token syntax with a plugin-owned variable registry and allowlisted output encodings. Do not implement C `printf`, Lua, scripting, expression evaluation, or user-defined functions.

## Template syntax

The editor inserts short, fixed tokens such as `{A}` and `{ZP}` so users do not have to type long variable names. These are keys in a plugin-owned variable registry, not user-defined names. Each operation has an allowlist of tokens, types, ranges, and encodings. The first VISCA profile test implements the eight tokens below. The semantic names in the registry and this document remain the stable explanation of what each short token means.

`templateLanguageVersion` belongs to a command binding that contains a template. If omitted, readers interpret it as version 1. A reader must reject an unknown version with a clear validation path; it must never silently parse an unfamiliar syntax as version 1. Adding a variable or formatter that leaves the grammar and all existing meanings unchanged is an additive registry change. Changing the grammar or an existing formatter's behavior requires an explicit version/migration and regression review.

| Token | Registry meaning | Encoding in the first VISCA tester |
|---|---|---|
| `{A}` | `camera.viscaAddress` | One address nibble, 1–7; the header literal supplies the leading `8` |
| `{PS}` | `pan.speed` | One byte, 1–24 |
| `{TS}` | `tilt.speed` | One byte, 1–20 |
| `{PD}` | `pan.direction` | One byte, 1–3 |
| `{TD}` | `tilt.direction` | One byte, 1–3 |
| `{ZS}` | `zoom.speed` | One nibble, 0–15 |
| `{ZP}` | `zoom.position` | Four high-to-low VISCA nibble bytes (`0p 0q 0r 0s`) |
| `{PI}` | `preset.index` | User-facing preset 1–255 is converted to VISCA index 0–254 |

`templateLanguageVersion` belongs to a command binding. If omitted, readers interpret it as version 1. A reader must reject an unknown version; it must never silently parse unfamiliar syntax as version 1. Adding a token without changing existing meanings is additive. Changing token meaning, grammar, or encoding requires an explicit version/migration and regression review.

Example absolute zoom packet (position only):

```text
8{A} 01 04 47 {ZP} FF
```

For camera address 1 and native zoom position `0x1234`, the encoded packet is:

```text
81 01 04 47 01 02 03 04 FF
```

The user-provided example combines a speed value with an absolute position. Sony's VISCA command reference lists `8x 01 04 47 0z 0z 0z 0z FF` as direct/absolute zoom and does not include a speed byte. Variable zoom speed is a separate continuous command (`8x 01 04 07 2p FF` for tele, with a corresponding wide command) that uses a speed nibble. Model documentation must decide which operations are supported; the profile must not add a byte to a standard packet just to carry speed. See [Sony BRC-AM7 VISCA command list](https://www.sony.com/electronics/support/res/manuals/5066/0e7e6b1bbac527853e7ea0cca5346923/50660651M.pdf).

Example variable-speed zoom-in binding:

```text
8{A} 01 04 07 2{ZS} FF
```

`{ZP}` emits the four high-to-low position nibbles in VISCA's four `0z` byte cells. The profile states the exact native zoom minimum/maximum and command applicability. The app maps its normalized zoom control to that model-specific range before formatting; if the range is unknown, validation must block this binding instead of guessing.

The profile can bind the templates to semantic operations like this:

```json
{
  "limits": {
    "zoomAbsolute": {
      "minimum": 0,
      "maximum": 16384,
      "units": "native-zoom-position"
    }
  },
  "commands": {
    "zoomAbsolute": {
      "driverOperation": "visca.zoomAbsolute",
      "template": "8{A} 01 04 47 {ZP} FF"
    },
    "zoomIn": {
      "driverOperation": "visca.zoomIn",
      "template": "8{A} 01 04 07 2{ZS} FF"
    }
  }
}
```

The numeric `maximum` above is only an illustration; each camera profile must use the range documented for that exact model and zoom mode. Continuous zoom speed and absolute zoom target are distinct input values.

The editor inserts tokens through its eight-button token picker. VISCA byte cells may be separated by commas or whitespace. Advanced users can edit JSON directly, but tokens are fixed and case-sensitive; arbitrary placeholder names, `printf` syntax, and expressions are rejected. The plugin owns each token's encoding, including the user-visible preset to protocol-index conversion.

## Initial variable registry

Variables are supplied by the action service from typed action arguments, validated profile ranges, selected camera settings, or protocol discovery. Profiles may reference only variables allowed for that action and driver operation.

| Variable | Type and source | Initial use |
|---|---|---|
| `camera.viscaAddress` | Bounded VISCA camera address; current app default is address 1 | VISCA address header |
| `preset.index` | Protocol-native preset index within the profile's declared range; the action/driver layer derives it from SimpleCam's 1-based user-visible physical preset number | Store/recall preset |
| `pan.direction`, `tilt.direction` | Enumerated direction values, not free-form text | Manual movement |
| `pan.speed`, `tilt.speed`, `zoom.speed`, `focus.speed` | Normalized user speed mapped through the profile's supported range/curve | Manual movement commands |
| `zoom.position` | Native zoom position mapped from the user-facing normalized control and profile min/max | Absolute zoom command |
| `zoom.position.nibble1` … `zoom.position.nibble4` | Derived high-to-low nibbles of the validated native zoom position | Commands whose protocol assigns one nibble per byte |
| `preset.transitionSpeed` | Optional normalized per-source speed, mapped only when the profile declares support | Preset recall command/sequence |
| `ptz.pan.target`, `ptz.tilt.target`, `ptz.zoom.target` | Position values in a discovered and selected protocol coordinate space | Future ONVIF/NDI PTZ actions |
| `ptz.pan.speed`, `ptz.tilt.speed`, `ptz.zoom.speed` | Speed values in a discovered protocol speed space | Future ONVIF/NDI PTZ actions |
| `onvif.mediaProfileToken` | Token selected/discovered from the camera's ONVIF media profiles | Future ONVIF PTZ requests |

The table is the start of a versioned registry, not a set of variables that every command can use. A VISCA operation cannot reference ONVIF tokens, and no command may access unregistered values such as arbitrary source text, file paths, or credentials.

## Allowlisted formatters

The first formatter set should be intentionally small and protocol-aware:

- `hexNibble` — one hexadecimal digit, range 0–15.
- `hexByte` — one hexadecimal byte, range 0–255.
- `viscaNibbles4` — one validated 16-bit numeric value rendered as four VISCA nibble bytes (`0p 0q 0r 0s`).
- `decimalNumber` — locale-independent decimal formatting for explicitly numeric protocol fields.
- Protocol-native typed serializers — for example, ONVIF numeric fields and NDI metadata fields; these return typed values to a protocol library, not arbitrary text to concatenate into a packet.

Each formatter declares input type, range, byte/field count, endianness or nibble order, and allowed drivers. A profile cannot request a formatter outside the selected driver's allowlist. Values outside limits fail validation or command construction; do not silently wrap, truncate, or clamp protocol values.

## ONVIF form

The ONVIF editor should reuse semantic values but not ask users to write a raw SOAP body. It should expose an `AbsoluteMove` form with a selected media profile token, target `Position`, and optional `Speed`; the ONVIF adapter serializes the typed request and handles namespaces, authentication, HTTP, and SOAP faults.

Conceptually, a zoom-only move is:

```text
AbsoluteMove(
  ProfileToken = onvif.mediaProfileToken,
  Position.Zoom = ptz.zoom.target in a supported absolute zoom space,
  Speed.Zoom = ptz.zoom.speed in a supported zoom speed space
)
```

`AbsoluteMove` position fields are target positions, not current camera positions. Pan/tilt and zoom are optional separately; a zoom-only request should omit PanTilt rather than reading and resending the current pan/tilt. If both are supplied, use `ptz.pan.target` and `ptz.tilt.target`, not speed variables. Speed is a distinct optional vector.

ONVIF's generic absolute pan/tilt position space uses -1 to 1; generic absolute zoom uses 0 to 1; generic speed is a separate 0 to 1 space. These are valid only when the selected PTZ node advertises those spaces. The actual request uses the advertised space URI and range. `http://onvif.org` by itself is not the required coordinate-space URI. The profile token also comes from the selected camera media profile; it must not be hard-coded as `MainStreamProfileToken`.

The ONVIF PTZ specification states that AbsoluteMove's position is an absolute target, PanTilt and Zoom may be omitted independently, and Speed is optional. It also defines generic pan/tilt positions as -1..1, generic zoom as 0..1, and generic speed as 0..1. See the [ONVIF PTZ Service Specification](https://www.onvif.org/specs/srv/ptz/ONVIF-PTZ-Service-Spec.pdf), especially Sections 4, 5.3.1, and 5.7.

## Validation and execution rules

- Parse and compile every command template when importing, editing, saving, or activating a profile.
- Reject unknown placeholders, formatter/type mismatches, variables unavailable for the action, incomplete byte cells, extra output bytes, out-of-range values, and unsupported operation/driver combinations.
- Treat fixed literals as immutable data in the template. The plugin supplies only typed runtime values and performs the registered conversion.
- Resolve all variables before opening/sending a command. Display the fully encoded VISCA packet or structured ONVIF/NDI request in the Sent trace.
- Never concatenate user-entered names, labels, IPs, credentials, or arbitrary text into camera commands.
- A profile does not select or invent network destinations, execute actions conditionally, loop, call other commands dynamically, or change the meaning of a variable.
- Keep profile test outcomes and hardware maturity separate from successful template compilation.

## Tests

- Verify the sample VISCA template compiles to the exact example packet for known inputs.
- Test every formatter at minimum, maximum, one below/above range, and invalid type.
- Test missing/unknown placeholders, unsupported variables for an operation, malformed bytes, and output-length mismatch.
- Test 16-bit zoom nibble order across boundary values and each model's declared native maximum.
- Test that per-source transition speed is passed only to profiles declaring it.
- For future ONVIF, test advertised space discovery, zoom-only requests omitting PanTilt, position/speed separation, XML serialization, selected profile token, and SOAP faults using protocol fixtures.

