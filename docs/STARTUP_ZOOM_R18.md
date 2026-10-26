# Startup zoom initialization — r18

## Behavior

After OBS FINISHED_LOADING, mark every camera runtime ready for zoom
initialization. The next event-loop turn sends the selected profile's
zoomInquiry for enabled slots with a configured host/port. Each camera uses
its existing asynchronous queue; one camera's delayed reply does not block
another camera's query or commands. This operation sends no movement or power
command and never recalls a preset merely to discover its zoom position.

A valid reply updates the existing per-camera zoom cache and current card's
observer. The UI's existing QSignalBlocker prevents a programmatic slider
update from sending a movement command. Selecting another card uses that
camera's cached value, including cameras not initially displayed.

After initialization has been enabled, an enable, host, port, VISCA address
or valid profile change invalidates the old cache and schedules a new query.
Configuration setters in one event-loop turn coalesce through a zero-interval
single-shot timer. No periodic polling, arbitrary waiting period or auto-wake
is added. Repeating the startup initialization call does not duplicate it.

Generation guards discard replies from the previous profile/endpoint/enable
state, including existing post-move zoom inquiries. Unavailable/disabled
slots do not get a startup query. Failures/malformed inquiry replies log a
ZoomInit warning and leave controls usable; no modal prompt is shown.

Logs identify this revision with `[SimpleCamPTZ ZoomInit] build=r18`.
The camera JSON, scaling, preset mappings, source IDs, Mac docking code and
window geometry remain unchanged.

## Verification

- Local GCC C++17 / Qt 6.4.2 service test: 34 checks, zero failures.
- Simulated TCP cameras verify startup read-only packets, explicit held replies
  across independent camera queues, idempotence, disabled/enabled slots, profile
  and endpoint refresh, old-endpoint reply rejection and recovery after rejection.
- Shared runtime work restores ci/push-platform.txt to all for Windows, Mac and
  Linux validation. Native build/package and real-camera acceptance are pending
  until their runs/operator tests complete.

## Operator test

1. Set the camera to a noticeable midrange zoom using its web controls.
2. Restart OBS. Before pressing a movement/preset button, verify the slider
   moves to the queried position. No unsolicited camera movement should occur
   beyond scene-source requests already configured to run at startup.
3. Select another configured camera card; verify its own cached position.
4. Disable/re-enable a slot; verify the slider refreshes from that camera.
5. Change a connection/profile and verify its new reply supplies the position.
6. With a camera offline, confirm OBS remains responsive and other cameras
   continue operating. Check ZoomInit in the log for that failure.

## Native cloud build result

Run [40402713949](https://github.com/wzaggle/SimpleCamDirect/actions/runs/40402713949)
succeeded for 1f6374e9f887dba8fd3ae5bff8ff5d9a555023cc. Windows x64,
Apple Silicon macOS and Ubuntu 24.04 x64 each compiled and passed all four
automated suites. Windows EXE, Mac user-domain PKG and Linux DEB were created;
Mac installation into the current user's OBS directory was verified again.
The collector checked all packages use the same revision and passed tests.

[All-platform installers](https://github.com/wzaggle/SimpleCamDirect/actions/runs/40402713949/artifacts/12689656780)
[Mac-only package](https://github.com/wzaggle/SimpleCamDirect/actions/runs/40402713949/artifacts/12688424286)

Artifacts expire 2026-11-02. Real-camera startup/refresh acceptance remains
pending. Native builds and simulated replies do not establish hardware behavior.
