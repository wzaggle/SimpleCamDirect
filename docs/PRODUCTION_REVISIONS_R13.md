# SimpleCamPTZ v0.1.0 production revisions — r13, 2026-10-26

## Scope and behavior

1. Source Properties uses Camera, Preset, Trigger, Delay (seconds), and Program/Preview/Both. Missing locale entries fall back to these readable English labels instead of showing SimpleCamPTZ.* keys. The source picker name is SimpleCam Preset; the source type ID stays simplecamdirect_preset.
2. Generated names read `Move Camera 1 to Podium`, without italic/bold tags. The orange span and asterisk for shared/referenced sources remain. Names inside that span are escaped once; ordinary names remain plain text.
3. A source-specific Properties adapter requests a compact initial height based on its four-row form. It identifies only OBSBasicProperties dialogs containing this source's camera field tooltip, removes that instance's 400px splitter minimum, and hides its unused preview frame. It does not access private OBS headers, alter global QSS, impose a maximum, or resize unrelated source dialogs. If OBS changes the known widget structure, it leaves the ordinary host dialog alone. OBS still owns the dialog and its normal geometry persistence.
4. On-air title/selector accents use light red #FF9999. Selected on-air shows a red number on the black circle; unselected on-air shows a black number on the red circle. Feed matching is unchanged.
5. The selected camera square uses muted blue #3C96C9, matching the lower function buttons. Unselected squares remain green. Camera selector dimensions and circle centering are unchanged.
6. Rename edits directly over the selected preset button using a child QLineEdit, never a layout insertion. Enter saves and flashes yellow; Escape cancels. Moving focus elsewhere leaves the uncommitted draft in place; it does not turn a subsequent preset click into a camera recall. Changing camera, page or management mode cancels an unfinished edit. The 32-character name limit and independent per-camera catalogs remain. Refresh and responsive button resizing keep the overlay attached to that button.
7. Every recall waits for VISCA completion before its appended zoom inquiry; the reply sets the slider and a per-camera position cache. Absolute slider moves now also inquire after completion, keeping that cache current. Zoom +/- already ends with stop plus inquiry. Card reassignment restores the last reported position (or the bottom before any report). Programmatic slider changes block signals, so a reply never sends an absolute move back to the camera. Digital positions still clamp to the optical slider endpoint; no zoom percentage is reintroduced.
8. Each camera retains its own nonblocking QTcpSocket, queue and timers; no worker/thread or global queue is added. Same-camera recalls, their delays, and their final inquiries remain ordered. Different cameras can proceed while another awaits completion. Preset recall and absolute zoom completion have a 60-second failure bound, replacing the former 2.5-second move deadline; other commands retain 2.5 seconds and TCP connection attempts retain 3 seconds. The deadline is a failure limit, not a sleep. Missing completion/error/timeout does not produce a fabricated zoom update.
9. Settings help says `Clear all camera presets before deleting group`. Normal Clear blanks one label. Ctrl+Clear confirms `Clear ALL preset labels in ‘<actual group name>’?` with Yes/Cancel and Cancel default. It blanks fourteen labels for only the currently selected camera/group, with one save. It sends no VISCA command and retains physical mappings and camera memory. Global page deletion still requires blanks on every camera (including disabled cameras), with five pages remaining.

## Preservation

- This overlay contains no production INIs, registry changes, automatic import, or new migration. Recovered names/maps remain in the existing r12 catalogs. Existing camera/group migration methods are unchanged.
- Camera enabled flags, network connections, source UUID mapping, profiles, settings file names, source ID and serialized source fields remain compatible.
- The r11 production geometry fix, responsive card sizing, floating limits, rightmost black gear, aligned OFF button and Ctrl+Off safety remain.
- No change to AViPAS JSON commands/capabilities, protocol preset indexing, preset store, manual movement speeds or preset recall speed support.
- The old fixed VISCA fixture helper is not the profile-driven runtime; its existing fixtures remain unchanged.

## Validation evidence

- PASS here: existing non-Qt C++ packet fixture executable compiled with g++ C++17 and ran with all 24 assertions enabled.
- PASS here: source preservation comparisons (profiles, command rendering and r12 migration code), no popup rename, no italic/bold generated-name tags, and overlay ZIP integrity/timestamps.
- NOT RUN here: full Qt6/OBS/MSVC compilation, profile test executable, runtime state executable, new localhost concurrency executable, OBS UI/lifecycle tests and physical camera tests. This workspace lacks Qt6 development libraries and the Windows/OBS toolchain. The packet fixture result does not certify the changed Qt transport.
- Added optional `simplecamptz_camera_concurrency_tests` target (EXCLUDE_FROM_ALL). Uses two localhost VISCA simulators with explicit ACK/completion gates. Checks independent progress, same-camera ordering through the zoom inquiry, exactly one inquiry per recall, shared group offsets, retained query cache, and local-only batch clearing with mappings unchanged. A watchdog only bounds test failure. The existing r12 runtime-state tests remain available. Neither optional executable is built by a normal Install.
- OBS dialog sizing was designed against the locally available official OBS 32.2.1 source. Installed OBS 33 behavior needs a runtime check; the adapter is guarded to skip unknown structure.

## Build and test on Windows

Close OBS. Overlay this package in the existing source project; use the usual RelWithDebInfo Install action. Confirm `camera_docks.cpp`, `camera_runtime.cpp`, `preset_source.cpp` and `plugin-main.cpp` compile. Startup marker: `build=r13-production-revisions`. Module title/version remains SimpleCamPTZ v0.1.0.

If copying manually to production, close OBS there and copy both the new DLL and `data/locale/en-US.ini` to their corresponding installed plugin directories. Do not overwrite plugin_config INIs with source/project files. The normal Install or installer includes the locale.

Optional tests (matching OBS/Qt release DLLs must be on PATH):

```powershell
cmake --build out/build/simplecamptz-windows-x64 --config RelWithDebInfo --target simplecamptz_runtime_state_tests simplecamptz_camera_concurrency_tests
ctest --test-dir out/build/simplecamptz-windows-x64 -C RelWithDebInfo -R "simplecamptz_(runtime_state|camera_concurrency)_tests" --output-on-failure
```

### Operator acceptance checks

- Open existing/new preset source Properties: simple labels, trigger choices, plain name, compact height with all fields/footer visible. Resize normally. Open a different OBS source Properties and confirm it remains normal. Check referenced sources still orange and marked *.
- Compare light-red on-air camera titles/number circles and blue selected squares in both cards, floating/docked. Missing visible feed still yields no on-air mark. Preset use/Set/rename still flashes yellow.
- Rename in Controls and Settings. Type and wait past the one-second refresh; text/caret must remain. Enter commits; Escape cancels. Resize while editing and change cameras/pages without committing; no camera move or wrong-camera rename should result from the edit itself.
- Recall from dock and OBS source; verify final zoom slider against actual camera. Adjust zoom from that value, switch cards/cameras, and confirm each camera retains its own last reported setting. A zero/optical-end/digital-end inquiry must not send an absolute move.
- Trigger two physical cameras together, with one doing a longer preset move: the second must start while the first is moving. Trigger two sources for one camera: second recall waits until first recall plus inquiry is complete. Check camera disconnected/error behavior leaves OBS responsive.
- On a disposable/test group, cancel Ctrl+Clear first. Confirm labels unchanged. Then accept: only that camera/group blanks; other cameras/groups and physical presets remain. Normal Clear still targets one button. Deleting a nonblank global page fails; deleting an extra globally blank page works; five-page minimum remains.
- Close and reopen OBS; check saved labels/mappings and normal shutdown.
