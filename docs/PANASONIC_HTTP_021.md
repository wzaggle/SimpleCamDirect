# SimpleCamPTZ v0.2.1 experimental Panasonic testing

This release adds two separate pieces: HTTP transport with a Panasonic AW protocol driver, and a draft AW-HE130 camera profile. Neither represents completed AW-HE130 hardware validation. Existing VISCA TCP, raw UDP and Sony-framed UDP methods remain available. The public v0.2.0 package remains the earlier testing baseline.

## First test

1. Close OBS, back up settings, and install the v0.2.1 package matching OBS architecture. Confirm v0.2.1 in the dock title.
2. Open Tools > SimpleCamPTZ Camera Profiler. Use Open JSON to select `panasonic-aw-he130.aw-http.json` from the installed plugin's `data/cameras` folder (beside the AViPAS and Sony profiles).
3. Confirm the profile name says DRAFT / untested, and Method is HTTP — Panasonic AW (experimental). Enter the camera's numeric IPv4 address. The default camera port is 80, gap 150 ms and timeout 3000 ms. Do not enter a URL or credentials in the IP field.
4. Click Test HTTP. A valid `p1` means power ON; `p0` means standby. `p3` means transitioning and is not treated as ON. No automatic wake command is sent.
5. Render and send Zoom Inquiry, then Position Inquiry. The trace shows the URL-encoded request and raw AW body. Zoom values run from hexadecimal 555 to FFF. Save trace exports a text file; review network addresses before sharing.
6. With the camera away from a live shot, test one short movement followed by Stop Pan Tilt. Test zoom and Zoom Stop, then Manual Focus, focus drive and Focus Stop. Verify physical direction and stopping. Profiler Send is a single command, not a timed jog: send the matching STOP explicitly. Do not use a movement command without being ready to stop it.
7. Only after those tests, test Home and absolute zoom. Home requests AW pan/tilt center. For presets, use a deliberately disposable preset, then test Store and Recall. Store/Power Off retain confirmation prompts. Preset 1 encodes decimal 00 and preset 100 encodes 99.
8. Use Stop & Release Camera, or close the Profiler, before normal camera operation. Testing temporarily pauses matching configured slots and does not replay blocked scene commands.
9. To try the normal dock, select the AW profile in Tools > SimpleCamPTZ Cameras and enter the camera IP. Test connection, manual motion/STOP, named presets and a disposable OBS scene. Save a modified profile as a distinct variant, refresh Cameras and explicitly select it.

## Implemented commands and limits

Power inquiry/on/standby; combined pan/tilt drive and STOP; center/Home; position inquiry; zoom drive/STOP, absolute zoom and readback; focus auto/manual, near/far and STOP; preset store and recall (100 slots). Backlight, exposure compensation, white balance, preset erase and recall-speed control have no bindings in this first draft and remain unavailable.

Pan and tilt use the existing normalized control levels, converted to AW values 01–99 around STOP 50. The HTTP adapter enforces at least 130 ms spacing, defaults to 150 ms, closes each HTTP connection, percent-encodes `#`, validates HTTP status and the expected AW reply, rejects redirects, and does not retry writes. Startup only requests zoom readback; it does not wake, move, store or erase presets.

Test All and automatic zoom discovery are disabled for Panasonic HTTP. PASS means a matching protocol reply, not successful physical movement or a hardware-tested profile. Preset and absolute-position replies acknowledge acceptance; no asynchronous motion-completion subscription is implemented. A subsequent zoom query can therefore capture an intermediate position. The dock labels an HTTP preset result as Recall accepted rather than Moved.

HTTP is limited to numeric IPv4 destinations and a fixed driver-owned CGI path. Profile command templates cannot choose URLs or run code. Plain HTTP only: TLS and credential entry are not implemented. HTTP 401/403 is reported as an access/authentication failure; do not disable camera security to work around it. Report that configuration so authentication support can be assessed separately.

STOP requests take priority over queued manual drives and preserve the normal command gap. Disable, endpoint change, test release and shutdown attempt best-effort STOPs for active drive axes. Network failure or immediate process termination can prevent delivery; real STOP and shutdown behavior must be tested with the camera operator present.

## Evidence

The draft mappings are based on Panasonic's HD/4K Integrated Camera Interface Specifications v1.12 (2020-04-27), especially sections 3.1.1, 3.1.3–3.1.7:

https://eww.pass.panasonic.co.jp/pro-av/support/content/guide/DEF/HE50_120_IP/HDIntegratedCamera_InterfaceSpecifications-E.pdf

Protocol fixtures and a localhost HTTP simulator cover URL encoding, replies/errors/timeouts, pacing, runtime zoom mapping, blank/delayed preset protection, profile Save As and Profiler ownership. These do not establish physical-camera or OBS runtime acceptance. Record native build results separately from hardware results.

## Feedback

Send camera model/firmware, OS and CPU, OBS version, profile revision, function-by-function results, saved JSON if modified, and the exported trace. For a crash include both the affected OBS session log and crash report. Successful tests are useful too. v0.2.1 is offered alongside v0.2.0 as an experimental community-testing pre-release.
