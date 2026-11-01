# Sony UDP first integration — v0.1.9

Source overlay over main 7b5f60291076b35e8fe6f11fa60d4d7011e632d1.
The maintainer authorized CODE IT; no GitHub update or cloud build is requested.

## First test scope

Sony SRG-300H is a separate protocol-reviewed profile. Firmware is unknown.
The standalone probe proved power inquiry, variable TELE/STOP and changing zoom
readback; this OBS integration still requires a Windows VS build and hardware test.
Only zoom +/minus, STOP, zoom feedback and read-only connection testing are enabled.
Pan/tilt, Home, presets, focus, absolute slider and power changes have no Sony
bindings yet and remain unavailable. No automatic power-on or preset recall.
The full AViPAS TCP profile and source naming/ON-AIR behavior are preserved.

## Connection and scheduling

Tools -> SimpleCamPTZ Cameras -> choose a slot and the Sony SRG-300H UDP profile.
Set its numeric IPv4 camera address, camera port 52381, UDP listening port 52381,
minimum request gap 100 ms and reply timeout 3000 ms. Sony VISCA address is fixed 1.
Connection test sends a read-only power inquiry and requires a valid ON/STANDBY reply.
Close SonyUdpProbe.exe before OBS uses its port. The earlier Norton rule must apply
to the OBS executable as well as the probe; an EXE-specific probe rule may not.
Use existing installation settings; no production endpoints are included in the profile.

The UDP adapter is distinct from raw TCP and selected by explicit profile framing
and named driver, not a guessed port. It binds one unconnected IPv4 listener per
local port, shares it among distinct camera IPs, filters replies by camera IP and
active sequence, and accepts any camera reply source port (the tested unit uses 65000).
Assign a physical camera IP to only one active UDP slot. Another application using
the local port gives a readable bind error. Camera slots maintain independent queues,
sequence counters and pacing; a silent camera does not block another camera.

A new session resets the protocol sequence counter (not camera settings), waits for
reset ACK, then always allows 100 ms setup time. Ordinary commands/inquiries wait the
configured gap after the previous terminal reply/ACK. Requests continue with advancing
32-bit sequences on the persistent listener. ACK, completion, inquiry and errors are
handled separately. Timers are asynchronous: no sleeps or blocking socket operations
are added to OBS threads. On timeout, the current request fails; a later request
starts a new reset session. There is no unbounded retry loop.

STOP is prioritized over queued polling, interrupts an active request wait, bypasses
normal gap (but not reset setup), and removes unsent continuous starts for the same
axis. It retains other scene requests in relative order. Zoom STOP completion precedes
its appended zoom readback. Best-effort zoom STOP is sent on an active zoom's transport
failure, disable, endpoint change or destruction. UDP delivery is not guaranteed.
Stopping an interrupted request reports cancellation; a late reply cannot satisfy a
different sequence. Model/firmware identification is not required for connection.

UDP port/gap/timeout settings are stored per slot; switching back to TCP restores its
previous camera port. Existing TCP scheduling/packet parser is unchanged. The standalone
Profile Test tool remains raw TCP only; use Cameras -> Test connection and the dock
for this integration.

## Verification

- Strict GCC/Qt compilation of runtime, profile and UDP adapter.
- Dock source syntax with Qt/OBS headers; the OBS 32 transition declaration is supplied
  for the local older libobs headers, and logging is stubbed for isolated tests.
- Real Qt adapter/runtime localhost UDP simulator: 15 tests, including reset/spacing,
  advancing sequences, differing reply port, wrong IP/stale/invalid packets, errors,
  timeouts, startup query-only behavior, zoom/STOP/readback, STOP priority, disable,
  duplicate IP rejection, shared listener, and repeated sessions.
- Sony profile/settings contract: 14 checks.
- Earlier TCP runtime-state: 35 checks; concurrency: 34 checks. Existing profile
  template and packet fixture executables pass. Allocator 27, ON-AIR 29 and title 8 pass.
- Windows full plugin build, OBS load, actual integrated camera behavior, Mac M1 and
  Ubuntu OBS runtime checks are NOT run here. Prior standalone hardware evidence is
  not relabeled as an integrated-plugin hardware pass.

## Local Windows acceptance

1. Close OBS and the probe. Extract overlay into the project root.
2. Build RelWithDebInfo and install normally. Confirm camera_runtime.cpp,
   camera_profile.cpp, visca_udp_transport.cpp and camera_docks.cpp compile and the
   loaded dock shows v0.1.9. The new profile must install under data/cameras.
3. Use a spare slot for Sony so the production AViPAS slot stays intact. Select Sony,
   enter the unit IP and defaults above. Test connection should receive power reply.
4. Briefly hold zoom +, release, and observe STOP. Repeat zoom minus. Confirm readback
   changes; unavailable controls stay disabled. Startup initialization only reads zoom.
5. Check repeated connection tests, disable/re-enable, restart persistence, and clear
   errors if the probe still occupies the port. Stop testing if release does not stop
   the lens; retain OBS log and Wireshark trace for diagnosis.
6. Run the existing AViPAS TCP controls and scene preset/source naming smoke checks.
   Pan/tilt and presets for Sony are the next separate test steps, not this overlay.
7. Mac M1 and Ubuntu testing follow Windows acceptance.

Opt-in test build: simplecamptz_sony_udp_driver and simplecamptz_sony_profile_tests.
With Python 3 available, CTest simplecamptz_sony_udp_integration runs the localhost
simulator only. No camera is contacted by automated tests.

Reference: Sony SRG-300H A-ES6-100-12(1),
https://pro.sony/s3/2019/01/16020225/AES6100121.pdf.
See SONY_SRG300H_UDP_CHECKPOINT.md for standalone packet evidence.
