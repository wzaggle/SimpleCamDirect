# Sony UDP full SimpleCam bindings — v0.1.10

Maintainer authorized CODE IT after reporting v0.1.9 zoom working in OBS.
No GitHub write or cloud build is authorized by this iteration.

## Profile versus installation

Sony SRG-300H — VISCA/UDP is a separate profile from any future TCP variant.
Its fixed camera/listening ports, VISCA address rule, framing, reply parsing policy,
reset setup delay, request gap, reply timeout, capacities and command/range definitions
are engineered profile/driver properties. Camera Selection shows installation choices
(name, IP, exact profile and video mapping); it no longer offers editable Sony UDP
ports, address, request gap or timeout. TCP profiles retain their existing editable
port/address because that is the established configurable connection path.
No raw TCP packet behavior or scene-source naming policy is changed.

Runtime reads port, listener port, minimumRequestGapMs, resetSetupMs and timeoutMs
from the profile on load/selection. Old v0.1.9 per-slot timing/listener overrides are
ignored and retired. A profile-forced port cannot be changed through setPort.
Switching back restores the prior TCP port. Timing uses Qt timers, not sleeps.
The standalone Profile Test tool is still raw-TCP-only; test Sony in the dock.

## Binding scope and evidence

Zoom in/out, STOP and position readback: maintainer-confirmed in v0.1.9 OBS.
New bindings: manual pan/tilt and STOP, Home, absolute zoom, focus auto/manual,
near/far/STOP, white balance auto/manual, preset store/recall, power on/standby,
backlight on/off, exposure compensation on/off, and pan/tilt position inquiry.
These bindings are available for incremental real-camera tests, not marked as
hardware-confirmed. White-balance and position inquiry have bindings but no new dock
controls are introduced. Preset erase and per-preset transition speed stay disabled.

SimpleCam's Clear remains label-only and never erases camera memory. Set keeps the
existing immediate overwrite behavior: use a disposable preset for testing.
Startup still sends only zoom inquiry; it does not move or power the camera.
Power test now renders the profile's powerInquiry, not hardcoded request bytes.
Pan/tilt/zoom values honor the profile's declared ranges. Absolute zoom range is
declared in the profile; it is not relabeled as a measured/calibrated endpoint.

## IP preset capacity

The profile now declares 255 user-visible presets, mapping physical 1..255 to
VISCA memory indexes 00..FE. This capacity is MAINTAINER-REPORTED for IP operation.
The previous 16-memory profile limit was removed. High-index store/recall is not
hardware-tested, and must not be described as manufacturer-reverified here.
The original Sony SRG-300H manual used in probe diagnosis is
https://pro.sony/s3/2019/01/16020225/AES6100121.pdf (A-ES6-100-12(1)).
Its fresh download failed (403), and web lookup was unavailable during this change.
The established command-table reference and operator correction are kept distinct;
a future firmware/manual check may require a separate compatibility profile.

## Continuous movement and ordering

Priority STOP still interrupts polling, removes unsent same-axis starts, and waits
for STOP completion before queued readback. The UDP adapter tracks active zoom,
focus and pan/tilt separately; errors, endpoint change, disable and unload attempt
the appropriate stops. This is best effort over UDP, not a delivery guarantee.
Normal scene requests retain order and repeated recalls are not deduplicated.
Presets and absolute zoom await completion before the next zoom inquiry.
Shared listeners route distinct camera IPs and independent sequences; duplicate
active slots for one camera IP are rejected.

## Local test plan

Close OBS and the standalone probe. Extract this cumulative overlay into the full
project, rebuild/install RelWithDebInfo, and confirm v0.1.10.
Select the existing Sony profile; ports/timing rows should disappear without
re-entering camera IP. Test connection and zoom first as a regression.

Then test in small steps:
1. Low-speed pan/tilt, release-to-stop, then Home with its existing double-click.
2. Absolute zoom slider, followed by manual zoom and readback.
3. Manual focus near/far/STOP, then autofocus.
4. Backlight and exposure compensation separately.
5. Store/recall one disposable preset and verify recall-completion/readback.
   Do not begin by overwriting production presets. High-number tests are separate.
6. Test an OBS preset source for that disposable preset, repeated source requests,
   profile persistence/restart, and an existing AViPAS TCP slot.
7. Only when appropriate, test standby and power-on. Initialization never auto-powers.
Mac M1 and Ubuntu runtime testing follow successful Windows acceptance.

Automated checks passed: 18 real Qt/localhost UDP integration tests and 41 Sony
profile/settings/packet checks; schema validation; existing TCP runtime state 35
and concurrency 34 checks; earlier packet/template fixtures; allocator 27, ON-AIR
29, Properties title 8. Runtime/profile/transport compile with strict GCC warnings.
Dock source syntax checked with Qt and older libobs headers plus an OBS 32 transition
declaration; logging stub used in isolated tests. Full Windows plugin compilation,
OBS load and new camera-function testing were not run in this environment.
Simulator/packet fixtures do not certify the added camera functions or IP capacity.
