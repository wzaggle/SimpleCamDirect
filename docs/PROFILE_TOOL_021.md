# Sony UDP Profile Test — v0.1.11

Maintainer requested CODE IT for Sony UDP testing and editable profile timings.
This source overlay follows the maintainer's successful v0.1.10 test report.
No GitHub write, cloud build, or native-platform acceptance is implied.

## Use in OBS

1. Rebuild/install the overlay with OBS closed. Confirm dock version v0.1.11.
2. Open Tools → SimpleCamPTZ Profile Test → Open JSON… and select the installed
   `sony-srg-300h.visca-udp.json`. The tool still starts with its existing default
   profile; selecting a camera slot copies its endpoint, not its profile.
3. Select the desired slot to copy its IP, or enter the Sony's numeric IPv4 address.
   The loaded Sony profile supplies remote Port, Local port, Request gap (ms),
   After reset (ms), Reply timeout (ms), and the fixed VISCA address 1.
4. Begin with **Test UDP / Power**. It sends sequence RESET, waits for its ACK and
   configured reset delay, then Power Inquiry. Success means a validated ON or
   STANDBY reply, not merely a successful send. It does not power on or move.
5. Edit command rows and use Render / Send. The tool's Sony token ranges follow
   the profile. Individual Sony tests query power before non-STOP operations;
   standby blocks the operation and asks you to use Power On explicitly. STOP
   bypasses that preflight. Existing TCP auto-wake behavior is unchanged.
6. Edit timings/ports in the draft. Subsequent tests use the draft values. Changes
   reconfigure the test session, stopping active movement and resetting sequencing
   when needed. Do not alter profile timing fields during continuous movement.
7. **Stop & Release Camera**, close, or Escape (when idle) returns normal control.
   During an operation, Stop & Release cancels immediately and sends best-effort
   STOP for any tracked moving axes. Emergency release does not restore zoom.
8. **Save As…** writes a separate validated command/timing profile copy, with a new
   filename-derived id and draft status. It does not replace the built-in file or
   change any slot. In Tools → SimpleCamPTZ Cameras, Refresh, select the saved
   profile and apply it through the existing selection workflow. Apply to Slot in
   Profile Test is disabled for Sony drafts to avoid applying an unsaved variant.

Built-in defaults remain remote/local UDP 52381, gap 100 ms, after reset 100 ms,
reply timeout 3000 ms. Editable bounds: ports 1..65535, gap/reset 0..2000 ms,
timeout 50..30000 ms. These are engineered profile properties, not per-slot
settings. Loading a profile or using Render/Save As sends no camera commands.

## Camera ownership and diagnostics

Profile Test uses the same Sony UDP adapter as normal control, rather than its
own packet implementation. To prevent two clients resetting one camera's
sequence, the tool temporarily borrows all configured slots with the same IP.
Pending slot commands are canceled and active movement receives best-effort STOP
before the listener transfers. New dock/source commands for those slots report
that Profile Test owns the camera; they are not deferred or replayed. Other
camera IPs continue independently. This lease is temporary and does not change
persisted enabled/profile/endpoint settings. Release, close, and destruction
restore transport availability with a fresh reset on its next operation.

The tool retains its UDP session across commands. Any reply source port is
accepted from the selected IP, including the observed Sony port 65000. Sender IP,
framing, sequence and VISCA validation remain in the shared driver. The bounded
trace includes full envelope TX/RX, source port, sequence, reset ACK bytes and
terminal PASS/FAIL. Raw RX lines say validation follows; their presence alone
does not mean acceptance. UDP pacing uses precise Qt timers, never sleeps.

Close the standalone probe before testing. Independent clients such as the
camera web page are outside SimpleCam's lease. Avoid simultaneous control.

Test All retains its explicit confirmation: it can move the camera, overwrite
the selected preset and put the camera into standby. Use individual Power Inquiry
and Zoom Inquiry first, then test movement and a disposable preset deliberately.
Zoom discovery uses the same UDP path and existing bounded poll/STOP/restore
workflow; its camera behavior is not hardware-confirmed by this update.

## Implementation and evidence

Changed components: profile_test_tool.cpp/.hpp (UI, draft editing, test routing,
lease lifecycle), camera_profile.cpp/.hpp (atomic validated timing update),
camera_runtime.cpp/.hpp (non-persistent test lease), visca_udp_transport.cpp/.hpp
(optional diagnostic observer and precise timers), opt-in widget test/CMake,
buildspec/installer version, and this documentation/checklists.

Local checks: 52 headless Qt widget/localhost checks cover draft timing bounds,
atomic invalid-command rejection, Save As round-trip, profile ranges, dock/test
handoff, duplicate tool ownership, reset delay, request spacing, exact inquiry
headers/sequence, alternate reply source port, stale packets, all Sony command
rows via Test All, timeout/error/standby reporting, no implicit Sony wake, STOP
preflight bypass, cancel while awaiting a reply, active-zoom STOP on close, slot
resume, and raw TCP connection/command behavior. Tests use simulated replies.
Existing Sony 41 profile checks and 18 UDP integration tests pass; TCP runtime
state 35 and concurrency 34 pass, along with packet/template fixtures, allocator
27, ON-AIR 29 and Properties title 8 checks. Strict GCC/Qt compilation uses a
logging stub in isolated tests. Offscreen UI visually inspected.

Not run here: complete native Windows plugin build, OBS load, real Sony Profile
Test operations, Mac M1 or Ubuntu OBS acceptance. No new per-command hardware
certification or manufacturer preset-capacity verification is claimed.
