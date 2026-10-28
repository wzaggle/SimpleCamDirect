# ONVIF PTZ first implementation research

Research date: 2026-10-27. Proposal only; no ONVIF implementation or hardware
compatibility claim. Preserve the working VISCA driver and existing OBS source ids.

## What the references establish

ONVIF standardizes services, capabilities and coordinate spaces. Discover
Device/Media/PTZ endpoints, choose a media profile with PTZ configuration, then
query the node/options instead of assuming every camera exposes the same ranges.
The PTZ service defines continuous/relative/absolute motion, Stop, Home and preset
operations. Optional capabilities must be checked. Presets are identified by
device-issued tokens rather than a guaranteed integer range. GotoPreset may
accept Speed when advertised; its response is non-blocking, not a move-complete
notification. GetStatus position/move state can be optional. [1,2]

AViPAS lists ONVIF for AV-1280W/G. Its controller guide lists ONVIF port 2000 for
the AV-1280/1250 series; verify the actual configured port and returned service
URLs. Its ONVIF Device Manager guide demonstrates pan/tilt/zoom but explicitly
says preset settings do not work on that page. That observation does not prove
all firmware lacks preset support: query and test it on the maintainer's camera.
Do not assume ONVIF and VISCA expose identical preset memory. [3,4,5]

## Proposed implementation boundaries

Use the existing Qt Network dependency for asynchronous HTTP requests, and
Qt XML stream serialization/parsing in Core. QNetworkAccessManager supplies
HTTP requests/replies; ONVIF SOAP serialization, authentication, capability
interpretation and completion tracking remain our responsibility. [6]

Add an ONVIF adapter behind the shared semantic camera actions. Preserve one
ordered queue per camera, independent progress between cameras, and UI-thread
responsiveness. Keep endpoint/profile/preset tokens as installation state.
Reusable camera JSON declares protocol/capability expectations and documented
quirks. Credentials belong to per-install secure storage or a per-session
prompt, never shared profiles, source names, exported JSON or reply traces.

Start by entering IP, ONVIF port and credentials manually. Service discovery is
still required; multicast device discovery can follow later. Add a read-only
capability probe to the test harness before enabling movement. Report firmware,
service versions, advertised ranges, preset capacity, status support and faults.

Then implement bounded continuous pan/tilt/zoom with Stop on release, optional
absolute zoom/status synchronization, followed by one explicitly chosen test
preset store/recall. Gate Home and speed options by discovered support. Map our
logical preset/page entries to opaque ONVIF tokens and persist that mapping
without changing visual swaps or overwriting recovered VISCA catalogs.

Do not treat a successful HTTP/SOAP response as completed camera motion. Design
bounded status monitoring around advertised feedback; report unknown completion
when the device cannot establish it. Define that fallback before connecting the
OBS scene-source queue. Do not emulate an acknowledgement with arbitrary delay.

Initial scope remains PTZ/presets/status. Focus, white balance, backlight,
exposure compensation and power need separate service/capability decisions;
do not advertise full existing-control parity from generic ONVIF support.

## Authentication and security

Core capabilities distinguish HTTP Digest and WS-Security UsernameToken. Query
camera UTC for diagnosing timestamp-sensitive legacy authentication. Prefer
Digest/HTTPS when supported; any legacy UsernameToken compatibility must be
explicit. ONVIF is retiring new Profile S conformance submissions after March
31, 2027 because of its username-token requirement; existing devices do not stop
working on that date. Profile T is the recommended successor. [7,8]

## First exit gate

Proposed sequence: read-only AV-1280 probe → manual PTZ/Stop → zoom feedback →
one reserved preset → Camera Control → OBS-source recalls. Keep raw responses
redacted for diagnosis and report each unsupported operation separately. Repeat
VISCA fixtures and hardware smoke tests after shared-layer changes. Test
Windows first, then M1 and Ubuntu, with the known Linux xcb launcher if needed.
No automatic preset sweep, camera configuration rewrite or credentials import.

## Primary references

[1] ONVIF PTZ Service Specification 26.06:
https://www.onvif.org/specs/srv/ptz/ONVIF-PTZ-Service-Spec.pdf
[2] ONVIF Device Feature Discovery:
https://www.onvif.org/wp-content/uploads/2022/07/ONVIF_Device_Feature_Discovery_Specification_21.12.pdf
[3] AViPAS AV-1280 manufacturer specifications:
https://www.avipas.com/product-page/av-1280-10x-sdi-ptz-camera-w-poe
[4] AViPAS controller connection guide:
https://www.avipas.com/_files/ugd/6b6a18_d275e8a8593143d9966b48a4405b3644.pdf
[5] AViPAS ONVIF Device Manager guide:
https://www.avipas.com/_files/ugd/6b6a18_e0eacdd454b24a5e862641e864960200.pdf
[6] Qt QNetworkAccessManager:
https://doc.qt.io/qt-6/qnetworkaccessmanager.html
[7] ONVIF Core Specification 26.06:
https://www.onvif.org/specs/core/ONVIF-Core-Specification.pdf
[8] ONVIF Profile S deprecation FAQ:
https://www.onvif.org/profiles/profile-s/profile-s-deprecation-qna/
