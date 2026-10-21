# Semantic camera action contract

## Purpose

The plugin should first define the exact camera functions the UI and OBS scene sources need. A camera profile then maps each supported semantic function to model-specific protocol commands and ordered response behavior. The UI, preset source, and camera dock must not contain VISCA bytes or protocol-specific assumptions.

The first implementation targets VISCA only. ONVIF and NDI PTZ may be added later as separate PTZ-only adapters, using their own structured request fields. The common action contract is the boundary; each driver and profile may declare a supported subset.

## Initial user-facing functions

| Semantic function | Parameters/state | Source use |
|---|---|---|
| `home` | None | Dock control |
| `panTilt` | Direction: left, right, up, down; manual movement speed; start/stop phase | Dock control |
| `zoom` | Direction: in/out; manual zoom speed where supported; start/stop phase | Dock control |
| `zoomTo` | Absolute target position in the profile's native range; a separate optional speed parameter only when the model's absolute-move operation supports it | Dock control and future scene-source actions |
| `presetStore` | Preset number `N` | Dock control; confirm before overwriting |
| `presetRecall` | Physical preset number resolved from the source's logical preview number (1–14) and the currently selected Preview Group; optional per-source transition speed | Dock and scene preset source |
| `focusMode` | Auto/manual | Dock control |
| `focusMove` | Near/far; start/stop phase | Dock control |
| `whiteBalanceMode` | Auto/manual | Dock control |
| `backlightMode` | Auto/manual, or the exact supported on/off behavior declared by the profile | Dock control |
| `cameraPower` | On/standby/off as supported by the model; UI label reflects actual operation | Dock control; confirmation for shutdown/standby |

The `start/stop` phases are part of the action contract so a held control can stop safely on release. If a device implements a function as a one-shot step instead, the profile/driver declares that behavior. Unsupported functions are disabled or marked unsupported; do not synthesize a command from a similar-looking VISCA function.

OBS scene sources and Camera Control dock buttons call the same in-process camera action service. A scene source stores its camera, logical preview number (1–14), trigger mode, and delay; it does not store a preset group or physical mapping. While editing a source, its UI requests the names for the currently selected Preview Group. At runtime the source passes its saved preview number to the shared preview action, which reads the active Preview Group and resolves the physical camera preset through that camera's page map. The initial sequential map is equivalent to `Preview + ((Preview Group - 1) * 14)`, but user layouts may distribute physical presets across any page slots. Start with five pages of 14 slots. The existing top `PG` button cycles pages. In Settings, `+` and trash beside the active page-description field add/remove pages; removal requires all 14 labels blank and at least five remaining pages. Clear beside Swap blanks a button label without erasing the camera's stored preset. Control mode provides Set/Rename; Settings provides Swap/Clear/Rename. Respect configured camera preset capacity; disable and label unassigned/out-of-capacity buttons `---`. Scene-source requests must preserve their OBS dispatch order and must not coalesce repeated preset recalls: a later source may intentionally move the camera again, including back to an earlier preset. A source's delay is part of that ordered request, so later sources wait behind an earlier delayed recall. Serialize this plugin's own protocol transactions as required by the driver. Camera access is not reserved exclusively for this plugin; SimpleCamQt and the camera web interface remain independent clients, with no global ordering guarantee across clients.

The table is the initial scope requested for the product, not a claim that every camera supports every function. Inquiry/status actions, exposure controls beyond backlight, and other vendor features remain outside the initial contract unless added deliberately.

## Speed is two separate controls

Expose a manual movement speed for dock pan/tilt/zoom/focus actions where the driver supports it. Absolute `zoomTo` takes a target position; it accepts a separate speed only when that camera's absolute-move operation supports speed. For example, Sony's documented VISCA absolute zoom packet has a position but no speed byte, while variable-rate continuous zoom has its own speed nibble. Separately, allow each scene preset source to carry its own optional `presetTransitionSpeed`, so two sources recalling different presets can request different transition speeds.

These values are user-facing normalized levels, not raw protocol bytes. A profile declares each range and mapping; the driver converts the normalized value to its supported model-specific encoding or sequence. A profile may implement preset transition speed as a recall parameter, as a per-preset speed-setting command followed by recall, as a temporary common-speed change followed by recall and restore, as a duration, or may not support variable speed. The editor shows only modes supported by that profile. If variable transition speed is unsupported, use the camera's normal recall behavior and indicate that the speed setting is unavailable; never silently promise or emulate it. The transaction trace must show any speed-setting command before recall, and the profile must declare if the camera-side speed setting persists.

Manual movement speed and preset transition speed must remain distinct in source settings and profile capabilities. A scene source's speed parameter must persist with that source and be passed through the action service on every recall. Existing sources that have no speed value use the profile's documented default.

VISCA speed behavior is model-specific. For example, Sony's BRC-AM7 VISCA command list documents per-preset speed/duration and common speed settings, while Sony's BRC-X400 family guide documents separate/common recall modes. This supports a capability-driven profile design; it is not evidence that all VISCA cameras provide the same speed feature. See [Sony BRC-AM7 VISCA command list](https://www.sony.com/electronics/support/res/manuals/5066/0e7e6b1bbac527853e7ea0cca5346923/50660651M.pdf) and [Sony BRC-X400 family guide](https://pro.sony/support/res/manuals/5011/c63d0ae378bdbcf267ab0dc410789a99/50112501M.pdf).

## Profile binding model

Profiles bind semantic actions to protocol-specific operations and data:

1. The UI or OBS source requests a typed action, such as `presetRecall(preset=4, transitionSpeed=0.35)`.
2. The action service checks the selected profile capability and parameter ranges.
3. The profile selects its declared VISCA connection variant and binding, including any ordered command sequence and response barriers.
4. The VISCA driver encodes the packet(s), parses ACK/completion/error/reply, and returns a structured result.
5. The transport adapter delivers bytes over the configured serial, TCP, or UDP path.
6. The action service reports the result to the source/dock and the configuration window's Sent/Received trace.

Profiles describe supported operations; the driver owns protocol mechanics. The profile editor may expose model-specific command bindings only through supported VISCA templates and typed parameters. Raw bytes are visible for inspection, but arbitrary byte injection and executable sequences are not part of the camera profile interface.

When ONVIF or NDI PTZ is implemented, its profile/editor mode will use typed PTZ fields or NDI metadata structures rather than a VISCA raw-hex editor. Those adapters remain limited to the common PTZ action contract and explicitly declared capabilities.

## Profile and camera-instance identity

There is one profile format for shipped models and user-created/customized profiles; there is no special one-camera override file type. A user may make a profile specifically for one physical unit, and it remains a normal reusable profile that can be selected by other configured cameras. The physical camera's name, address, port, serial device, credentials, and selected profile id remain in per-camera OBS settings.

## Compatibility and tests

- Add optional `presetTransitionSpeed` to scene-source settings using a versioned migration; old sources use the documented default.
- Test source serialization/deserialization, migration, UI enable/disable based on profile capability, normalized-to-model speed mapping, and exact packet/sequence output.
- Test recall with speed and without speed, plus models whose profile declares no speed support. For common-speed devices, verify queue serialization and restore behavior; for per-preset speed devices, verify the target slot and camera-side persistence.
- Use manufacturer documentation and packet fixtures for protocol tests; use real hardware evidence before labeling the speed capability hardware-tested.
- Confirm that a speed setting on one scene source does not alter another source's saved value. If the camera supports only a global/common speed, serialize the temporary change with recall and restore the configured common default afterward when the protocol allows it; otherwise clearly expose the shared behavior and its limitation.
