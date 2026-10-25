# AViPAS AV-1280W profile capture

**Status:** Confirmed for the maintainer-tested AViPAS AV-1280W unit over raw VISCA/TCP. The exact firmware and hardware revision were not recorded, so this is not a blanket certification of the AV-1280/1281 family.

**Built-in profile:** [`data/cameras/avipas-av-1280w.visca.json`](../data/cameras/avipas-av-1280w.visca.json)

## What the manufacturer documents

| Profile data | Captured value | Notes |
|---|---|---|
| Model identity | AViPAS AV-1280W; product SKU AV-1280W/G; manual covers AV-1280/1281 series | Hardware-tested on the maintainer's AV-1280W unit; firmware and revision not recorded. |
| VISCA address | 1–7; default 1 | The first packet byte is `0x80 + address`; address 1 gives `0x81`. |
| Preset capacity | 255 positions | The manual's VISCA memory parameter is **0–254**. SimpleCam keeps user-visible physical labels **1–255** and the VISCA adapter encodes physical preset N as protocol memory N−1. |
| Optical/digital zoom | 10x optical; up to 5x digital | Digital zoom is documented as off by default. This does not give the native VISCA zoom-position endpoints. |
| Native zoom command | VISCA direct zoom uses four nibble bytes: `8x 01 04 47 0p 0q 0r 0s FF` | Need actual wide and tele inquiry values before mapping the UI range. Test optical-only and digital-enabled behavior separately. |
| Physical PTZ travel | Pan ±135°; tilt ±30° | Manufacturer product specifications. |
| Physical PTZ speeds | Pan 0.1–60°/s; tilt 0.1–30°/s | UI protocol speed bytes are documented as pan `0x01–0x18`, tilt `0x01–0x14`; these are not a linear degrees-per-second conversion. |
| Variable zoom speed | Tele `07 2p`, wide `07 3p`; `p=0` low through `F` high | Do not reuse the UI's 0–10 value directly as a byte without a defined mapping. |
| Focus | Auto `04 38 02`; manual `04 38 03`; near `04 08 03`; far `04 08 02`; stop `04 08 00` | The command bytes conflict with some legacy app mappings; see the test list below. |
| White balance | Auto `04 35 00`; manual `04 35 05`; also 3000K, 4000K, one-push, 5000K, and 6500K | A one-push action can be added when the semantic action contract exposes it. |
| Backlight | On `04 33 02`; off `04 33 03` | This is distinct from exposure compensation. |
| Exposure compensation | On `04 3E 02`; off `04 3E 03`; adjustment range in camera menu −7 through +7 | Requires auto exposure and EV enabled according to the manual. The current action API only models the on/off toggle. |
| Power | On `04 00 02`; off/standby `04 00 03` | The UI should continue its deliberate safety confirmation for off. |
| VISCA responses | ACK `z0 41 FF`; completion `z0 51 FF`; syntax error `z0 60 02 FF`; not executable `z0 61 41 FF` | `z = camera address + 8`. Treat ACK and completion as different states. |
| Power state inquiry | Query `8x 09 04 00 FF`; reply `y0 50 02 FF` means On and `y0 50 03 FF` means Off/Standby | Camera address 1 query: `81 09 04 00 FF`; response begins `90 50`. The profile tester uses this to verify wake state before sending an individual command. |
| RS-232 | 2400, 4800, 9600, or 115200 baud; 8 data bits, no parity, 1 stop bit | Manual identifies an 8-pin mini-DIN interface. Confirm installation cable pinout and whether using input or cascade port. |
| Network | Default IP `192.168.5.163`; configurable VISCA port default `3001`; data port default `3000`; web port `80`; ONVIF port `2000` | The manual's network port table does not specify TCP vs UDP. |

Manufacturer references: [AViPAS AV-1280/1281 manual](https://www.avipas.com/_files/ugd/6b6a18_83e960b465f748c1ab4d79ce159b531d.pdf) and [AViPAS AV-1280 product specifications](https://www.avipas.com/product-page/av-1280-10x-sdi-ptz-camera-w-poe). The manual version is dated 2018-11-07; verify its applicability to the connected unit.

## Existing installation evidence

Earlier testing in this project used the camera at `192.168.1.4` on port `1259`. The existing application sent raw VISCA over TCP and movement worked. Treat those as the current installation's connection values, not as profile constants. The AViPAS manual's port default is 3001, so record the actual web-configured VISCA port and selected TCP/UDP mode on the reconnected camera before setting a profile default.

The user's working VISCA setup has camera address 1. The legacy app's `SendCommand` prepends the address byte (`0x80 + camera address`), sends command bytes to the configured TCP endpoint, then reads a reply and may wait for a second response for commands expected to complete. The new profile/driver should keep that response-aware queue behavior rather than replacing it with a fixed sleep.

## Confirmed behavior and bounded exceptions

The built-in profile was checked against the manual and exercised through the SimpleCamPTZ test harness and OBS controls. These are the model-specific results to preserve:

| Operation | AViPAS manual | Existing legacy mapping | Required check |
|---|---|---|---|
| Focus near/far and stop | Near `08 03`; far `08 02`; stop `08 00` | Manual focus must be enabled before near/far commands. | Tested with manual focus; use the profile bindings. |
| Backlight and exposure compensation | Backlight `33 02` / `33 03`; exposure compensation `3E 02` / `3E 03` | These are separate camera functions. | Both toggles were tested in the Qt/Direct camera workflows. |
| Absolute zoom layout | Four VISCA nibbles `47 0p 0q 0r 0s` | Older code used a different encoding. | Optical slider/inquiry path is usable. Digital zoom behavior on the tested unit remains inconclusive and is not claimed by this confirmation. |
| Preset index base | Manufacturer says 255 positions, VISCA memory number 0–254 | SimpleCam user-visible/physical labels are 1–255 | This is the expected boundary conversion: physical preset 1 encodes as VISCA memory 0; physical preset N encodes as N−1. Add packet fixtures for first and last slots and retain 1-based labels in pages, sources, and settings. |
| Preset store and speed | AV-1280/1281 manual lists clear `3F 00 pq`, set `3F 01 pq`, recall `3F 02 pq`; it does not list preset speed assignment | Legacy code sends clear + set, then `7E 01 0B pq 10` | AV-1280 test of `81 01 7E 01 0B 00 10 FF` returned `90 60 02` (Syntax Error). A separate real-camera test confirmed that changing the `VV WW` values in Pan/Tilt Stop does not change the speed of the next preset recall. Keep preset transition speed unsupported for this profile; the dock slider controls manual movement only. Other camera models may define a documented variant. Set does not clear the slot; Recall does not include the separate Zoom Inquiry. Test All orders Store → Recall → Zoom Inquiry → Clear so Recall is checked before Clear. |
| Network transport/port | Manual lists configurable VISCA port 3001 but does not say TCP or UDP in that section | Tested installation uses raw VISCA over TCP at port 1259 | Keep address and port in OBS slot settings; do not bake them into this reusable profile. |

## Zoom data still needed

“Maximum zoom” needs to be represented as separate optical and digital capability facts plus a native VISCA position mapping. A 10x optical / 5x digital specification is not enough to infer the VISCA position maximum. The manual defines the four-nibble packet and inquiry format but does not publish the physical endpoint values.

With the camera stationary and digital zoom disabled, record:

1. Inquiry response at full wide.
2. Inquiry response at full optical tele.
3. Several intermediate inquiry responses and whether values progress monotonically.
4. The same endpoints with digital zoom enabled, if the camera's web/configuration UI permits it.
5. Whether absolute zoom accepts values beyond the optical tele endpoint when digital zoom is enabled.
6. What response is returned for malformed or out-of-range positions; do not probe with an unbounded sweep.

The user-facing optical scale can then map its optical range to measured native positions. Digital magnification should remain a distinct range only if the camera supports it predictably. The current profile JSON deliberately leaves `zoomAbsolute` physical limits unset instead of inventing a number. SimpleCam will not enable or disable DZoom; users manage that and the camera's color, image, and vendor-specific move-curve setup in the camera's web/direct interface.

### Planned in-tool calibration

The first Camera Definition and Configuration Tool pass should include **Zoom Calibrate**. On a selected test camera it reads the initial position, starts continuous Zoom Tele once, and repeatedly queries the reported position. When valid readings stop increasing for a bounded stability window, it sends Zoom Stop, queries once more for the endpoint, and restores the initial position. The profile editor records the result in the unsaved profile draft with camera firmware, digital-zoom mode, connection variant, and the transaction trace. Timeout, cancel, malformed replies, or connection loss send a best-effort stop and do not save a maximum.

For this camera, the user configures DZoom in the camera's web/direct interface, then runs calibration to measure the maximum for that current state. If both optical-only and optical-plus-digital endpoints are needed, the user changes DZoom externally between two runs. SimpleCam records which mode each run measured; it does not change DZoom or other camera setup values. The current inquiry behavior and stop timing still need real-camera verification before the tool can treat the measurement as reliable.

The first standalone **Camera Profile Test** implementation is deliberately narrower than the planned editor: it records the measured native endpoint, a user-selected DZoom label (`off`, `on`, or `unknown`), and a UTC measurement timestamp in the in-memory JSON draft. It shows the connection and packet/reply trace in the tool, but does not copy local IP/port data or the trace into the profile. A successful measurement remains unsaved until the user chooses **Save As**. A cancellation or polling timeout stops movement and restores the starting position when absolute zoom is available, but does not save the partial endpoint.

## Safe camera test capture

For each check, record camera firmware/version inquiry, exact connection variant and port, sent packet, all replies (ACK/completion/error), observed result, and whether it is safe to repeat. Use an unoccupied preset number for store tests; do not clear or overwrite a production preset. Confirm power-off means standby on this camera.

For future profiles, capture at least the following before setting status to `hardware-tested`:

- VISCA address and connection test over the exact configured TCP/serial mode.
- Home; each pan/tilt direction and stop; minimum and maximum documented speeds.
- Zoom in/out/stop, absolute zoom endpoint/inquiry mapping, and digital-zoom state if in scope.
- Focus auto/manual, near/far, and stop.
- Preset recall, store to a spare index, recall, and clear that same test index.
- White balance auto/manual; backlight on/off; exposure compensation on/off.
- Power on and standby only with the camera operator physically present.
- Correct ACK/completion/error parsing and per-camera command serialization.

## Profile status and remaining limits

The profile records documented VISCA operations, serial parameters, manufacturer network defaults, capabilities, and sources. It intentionally excludes the installation IP, the custom port as a default, and guessed native zoom limits. Preset Clear, Store, Recall, and Zoom Inquiry are separate commands. Preset speed assignment is disabled for this model after the camera returned a syntax error; another model can add a separately documented command. The profile's `hardware-tested` status applies to the tested AV-1280W unit and recorded workflow only; retest when firmware, hardware revision, transport, or model changes.
