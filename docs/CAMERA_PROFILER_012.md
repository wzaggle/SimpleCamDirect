# Camera Profiler — v0.1.12

The maintainer authorized CODE IT for an independent reusable profile authoring
and testing workflow. This supersedes the slot-oriented Profile Test UI described
in older documents. No GitHub write or cloud build accompanies this source package.

## Workflow

1. Close OBS. Build/install **RelWithDebInfo**, not Debug, and restart OBS.
   Confirm v0.1.12 on the dock. Open **Tools → SimpleCamPTZ Camera Profiler**.
2. **Open JSON…** loads a `.json` or `.camera.cfg` profile. The tool initially loads
   the existing built-in default; it does not obtain a camera slot's profile/IP.
   Load the Sony SRG-300H profile when testing that camera.
3. Enter a **Camera/Profile Name** independently of the filename. Manufacturer
   and Model are editable metadata. Test IP is temporary and is never saved in
   the reusable model profile. There is no Camera Slot selector or Apply to Slot.
4. Choose one method:

   | Method | Wire behavior |
   |---|---|
   | VISCA / TCP — Raw | VISCA bytes on a TCP connection, without a Sony packet envelope |
   | VISCA / UDP — Raw | VISCA bytes in UDP datagrams, without a header, sequence or reset exchange |
   | VISCA / UDP — Sony framing | Eight-byte VISCA-over-IP envelope, command/inquiry types, sequence correlation and reset/setup |

5. Set **Port**, **Local port**, **Request gap**, **After reset**, **Reply timeout**
   and the profile VISCA address. Tuning is separate from the method and belongs
   to the profile. Selecting a method does not infer or overwrite ports/delays.
   Local port is disabled for TCP; reset delay is disabled for both raw methods.
   Sony framing fixes VISCA address 1; raw methods allow addresses 1..7.
6. **Test UDP** sends the profile's Power Inquiry and requires a validated ON or
   STANDBY reply. Sony framing first performs reset/setup; raw UDP does not.
   **Test TCP** checks the connection only; use the Power Inquiry row's Send to
   check the camera protocol. These checks do not wake or move the camera.
7. Edit/render/send command rows using the selected method and timing. Individual
   non-STOP tests check power, and standby asks for explicit Power On rather than
   waking the camera implicitly. STOP bypasses the power preflight. Test All
   retains its explicit confirmation because it can move, change camera state,
   and overwrite the selected preset. Start with inquiries, then low-speed motion
   and a disposable preset when appropriate.
8. **New Profile** creates an unsaved draft using the currently loaded command
   bindings and ranges as a starting template. It clears the prior model identity,
   firmware-test data, hardware-test record and measured zoom endpoint. Enter the
   new model/name and verify the inherited bindings/ranges; the template is not a
   claim that those commands work on the new model. Full arbitrary JSON/range
   authoring and a community submission workflow are not added in this build.
9. **Save As…** writes a new draft profile, preserves the entered display name,
   assigns its filename-derived profile id and records the parent id. Built-in
   profiles/source files are protected from overwrite. Saving does not change any
   camera slot. In **SimpleCamPTZ Cameras**, Refresh and explicitly select the
   saved profile. The saved profile's method and tuning drive normal operation.

Ports are 1..65535; gap/reset delays 0..2000 ms; reply timeout 50..30000 ms.
The SRG-300H built-in profile retains target/local UDP 52381, 100 ms request gap,
100 ms after reset, and 3000 ms timeout. These defaults belong to that profile,
not to the shared Sony-framing method or every camera implementing that framing.

## Ownership, cancellation and compatibility

The Profiler temporarily borrows all configured slots matching the test IP before
network testing. New dock/source requests for that camera report the reservation;
they are not queued for replay. Other camera IPs remain independent. There is no
slot selection requirement. Stop & Release Camera cancels pending test work,
attempts STOP for tracked axes and restores normal control. It remains available
while awaiting UDP/TCP replies and while a TCP send is paced. Idle close/Escape
also releases ownership. Emergency cancellation does not restore zoom position.
STOP delivery is best effort; receipt of a sent packet is not proof of camera stop.
Keep independent controllers/probes from controlling the same camera during tests.

Raw UDP cannot correlate replies by sequence because its wire format has no such
field. It accepts replies from the selected IP on any source port, serializes
requests, validates response shape, and uses profile spacing/timeout. Delayed
same-shaped replies cannot be distinguished reliably in that method. Models that
provide no UDP inquiry replies cannot pass the readback-based test merely because
movement commands work. Sony-framed replies retain IP/sequence/header validation.
The shared Sony adapter retains its current response-size and operation limits;
matching framing alone is not a promise of support for every model or block inquiry.

Newly saved raw TCP/raw UDP/Sony profiles own ports, address and tuning. Existing
legacy TCP profiles retain their established per-slot endpoint behavior until
explicitly saved through the Profiler; production slot port choices are not
silently migrated. Profile name is model metadata; the Cameras Tool still has its
own installation-specific camera name/IP/video mapping.

## Implementation

Profile schema changes are additive: named `raw-visca-tcp` / `raw-visca-udp`
drivers and shared `tuning.minimumRequestGapMs`; existing Sony UDP fields remain
readable. Each new profile selects one method. Raw UDP reuses the asynchronous
UDP queue/listener/parser, bypassing only Sony framing/reset/sequence mechanics.
Normal TCP uses profile pacing and reply timeout for newly saved profiles;
legacy TCP behavior remains available. Normal camera controls/sources still use
the shared runtime service. Timing uses Qt timers, not blocking sleeps.

Model speed/range declarations remain separate from framing. Sony framing no
longer imposes the SRG-300H's zoom-speed limit of 7 on other models; typed VISCA
zoom-speed nibbles allow 0..15, while the SRG-300H profile still declares 0..7.
Custom Save As copies are draft and do not inherit a hardware-test record.
The AViPAS source profile also has two metadata-only schema fixes: explicitly
unrecorded firmware and a shorter description of its unmeasured zoom mapping.

## Evidence and Windows acceptance

97 headless Qt/widget/localhost checks pass, including the real New Profile / Save
As dialog, name-versus-filename persistence, three method selections, independent
ports/timing, custom model metadata, draft status, saved-profile/schema eligibility,
raw UDP framing/address/pacing/timeout/STOP and runtime parity, TCP pacing and
reply timeout/cancel/STOP, plus the earlier Sony suite's reset, source port,
sequence, failure, standby, Test All and ownership coverage.
Existing 18 Sony UDP integration tests, 41 Sony profile checks, TCP state 35,
TCP concurrency 34, profile/packet fixtures, allocator 27, ON-AIR 29 and Properties
title 8 checks pass locally. All three exported method profiles and both built-in
profiles validate against the schema. Strict GCC/Qt builds and dock/menu syntax
checks pass; offscreen UI visually inspected. Isolated tests use an OBS logging
stub; dock/menu syntax uses older OBS headers with an OBS 32 transition declaration.

Not performed here: complete native Windows build, OBS load, real-camera v0.1.12
acceptance, Mac M1 or Ubuntu OBS acceptance. Raw UDP camera hardware is not present.

Windows test order: confirm RelWithDebInfo/version/menu, load the Sony profile,
enter its IP, Test UDP, Power/Zoom Inquiry, then intentional movement/STOP. Save a
custom-named copy, refresh Cameras, select it and repeat dock controls. Exercise
New Profile/Save As and confirm its display name is independent of the filename.
Retest the existing AViPAS TCP slot. Raw UDP needs an owner with matching hardware.
Mac M1 and Ubuntu follow successful Windows testing.
