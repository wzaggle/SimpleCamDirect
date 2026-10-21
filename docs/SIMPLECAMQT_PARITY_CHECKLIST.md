# SimpleCamQt to SimpleCam Direct 0.1.x parity checklist

Use this checklist to compare the established SimpleCamQt + OBS Lua workflow with the new native SimpleCam Direct source and dock. Keep the same Windows system, OBS version, AViPAS AV-1280W, firmware, camera connection, and preset state. SimpleCam Direct's source and dock share the same in-process action service. SimpleCamQt and the camera web interface remain independent clients and may also control the camera; their requests are not globally ordered with Direct's.

Record for every row: SimpleCamQt result, SimpleCam Direct result, pass/fail, notes, and any intentional difference. Do not call a difference a regression until the expected behavior is agreed.

## Setup and shared action path

- [ ] Both products can be installed and loaded in the same OBS instance, and both can actively command the same camera.
- [ ] The native source type has a distinct OBS id; existing Lua/file-bridge sources still load and retain their settings.
- [ ] A Direct scene source and a Direct dock action invoke the same typed action service and per-camera queue; no file or IPC bridge is involved inside Direct.
- [ ] SimpleCam Direct connects to the configured AViPAS over the verified VISCA transport and reports failures clearly.
- [ ] Each product retains its own camera/preset settings; both may point to the same camera endpoint without overwriting the other's configuration.
- [ ] The camera web interface, SimpleCamQt, and SimpleCam Direct each move the camera successfully without requiring the other clients to exit or release ownership.
- [ ] Repeated and consecutive Direct scene-source requests are all delivered to the Direct action service in OBS dispatch order; the final position matches the last Direct request.
- [ ] SimpleCamQt and the web interface can also control the same camera, with no claim of global ordering across those separate clients.

## Preset groups and source resolution

- [ ] A scene preset source stores its camera, logical preview number (1–14), trigger mode, and delay; it does not store a preset group.
- [ ] While editing a source, the preset list displays names from the currently selected Preview Group.
- [ ] At runtime, the shared preview action uses `Preview + ((Preview Group - 1) * 14)`; group 1/Preview 1 recalls physical preset 1 and group 2/Preview 1 recalls physical preset 15.
- [ ] Changing Preview Group changes the physical slot recalled by the same source number; the user selects the group they intend.
- [ ] Camera preset capacity limits available groups and buttons. For capacity 25, group 1 has 14 active buttons; group 2 has 11 active buttons and three dimmed, disabled `---` buttons.
- [ ] Visual preset swapping and renaming retain the agreed behavior without physically rewriting camera presets.
- [ ] Scene source labels remain clear after changing group or visual order; renaming and duplicate-source behavior still work.
- [ ] Repeated requests are not merged or deduplicated, including when two consecutive entries select the same preset.

## Direct camera controls

- [ ] Pan and tilt in all four directions, diagonal movement if exposed, stop-on-release, and home.
- [ ] Zoom in/out and zoom speed control.
- [ ] Focus auto/manual toggle and near/far controls only in manual mode.
- [ ] White balance auto/manual behavior.
- [ ] Backlight and exposure compensation on/off behavior.
- [ ] Camera off uses the required deliberate confirmation; on/standby behavior matches the documented camera function.
- [ ] Speed values produce comparable motion and stopping behavior.
- [ ] Repeated/rapid actions remain ordered; no stuck movement remains after release, focus loss, disconnect, or OBS shutdown.

## Preset controls and names

- [ ] Recall a known preset from every configured preset group.
- [ ] Set a test preset and confirm it recalls the saved physical camera position.
- [ ] Rename an active preset and confirm every corresponding OBS source displays the intended camera/preset name.
- [ ] Swap two visual preset positions; confirm button positions and labels move while physical preset numbers and camera locations do not change.
- [ ] Group selection, group name, visual color cue, and restart default match the agreed behavior.
- [ ] Confirmation flashes, selected/pressed state, and function feedback are clear without shifting the layout.

## OBS scene source behavior

- [ ] Create a named preset source for the configured camera and logical preset.
- [ ] Program, Preview, and Both trigger modes each match their expected scene behavior.
- [ ] Configured delay is honored; zero-delay behavior is checked separately.
- [ ] Scene switching triggers the intended move once, without stale queued moves.
- [ ] Duplicate copies of a preset source are marked and renaming behavior is consistent across scenes.
- [ ] Save/reopen OBS and the scene collection; source settings and camera/preset associations persist.
- [ ] Rename a preset while the source exists; the source display updates without requiring source recreation.
- [ ] Use the camera web interface between OBS scene actions; confirm its move succeeds and later OBS actions still execute normally.

## Stability and comparison record

- [ ] Run a representative service with repeated scene transitions and manual controls; record hangs, missed commands, errors, and camera response differences.
- [ ] Restart OBS and repeat core tests.
- [ ] Record exact OBS version, SimpleCamQt version, SimpleCam Direct build id, Windows version, camera firmware, connection, and test date.
- [ ] Attach redacted logs or traces for any failure; do not include credentials or private endpoint details in public reports.

## Acceptance

SimpleCam Direct 0.1.x passes when every in-scope function has a recorded result, critical camera actions and scene behavior match the agreed SimpleCamQt behavior, no unintended camera movement occurs, and known differences are documented. This gate establishes only Windows + VISCA + AViPAS AV-1280W behavior; it does not establish support for another camera, OS, transport, or protocol.
