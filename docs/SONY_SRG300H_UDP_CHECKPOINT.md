# Sony SRG-300H UDP diagnostic checkpoint — 2026-10-30

Standalone probe v0.0.14 is preserved in tools/sony-udp-probe. It is NOT integrated into the OBS plugin or a claim of a complete Sony profile. MARK IT authorized this checkpoint separately from SimpleCamPTZ v0.1.8.

## Real Windows/camera evidence

Maintainer built and ran v0.0.14 on the SRG-300H; model firmware revision is unverified. Log SonyUdpProbe-20261030-022002.log records power ON at sequence 1; initial zoom 0x0031 (49) at sequence 2; lowest-speed TELE for 250 ms at sequence 3; STOP ACK and completion at sequence 4; final zoom 0x0062 (98) at sequence 5. Standard inquiry header 0110 returned changing position feedback. Command header 0100 was used for TELE and STOP. Queued sequence-3 ACK/completion were logged and excluded from STOP matching.

Destination and local listening UDP ports were 52381. Replies came from camera source port 65000. Validate configured sender IP but do not require sender port to match destination port. Use big-endian eight-byte Sony envelope, camera payload address 81, reset-counter ACK before subsequent requests, and advancing transaction sequence numbers.

v0.0.11: power sequence 1 answered, immediately following power sequence 2 timed out. v0.0.12 added 100 ms after first power reply; both answered. v0.0.13 spaced power1/zoom2/power3/zoom4 by 100 ms, independently testing 0110 and 0100 zoom headers; all 8 answered. These observations establish a working conservative gap on this unit, not an experimentally measured minimum or proof of the internal cause. Initial failures do not establish unsupported commands or serial-only block queries. Model/ROM and block inquiries have not been retested with corrected spacing.

## Validation and boundaries

GCC C++17 strict warnings build passes. 41 localhost simulator tests pass; updated movement-gap assertions also pass in the eight movement tests. Native Windows compilation and actual power/zoom results are maintainer-reported. No macOS or Ubuntu hardware validation of this probe. No Sony pan/tilt movement, preset writes/recalls, identification, or full-profile support is claimed.

Default probe mode MOVES the lens: brief low-speed TELE then STOP and readback, leaving zoom at its new position. No preset restore, power change, or automatic return is attempted. Source and launchers are preserved for repeatability; no EXE or private installation data is committed.

## Integration direction, not yet implemented

Add a distinct Sony VISCA/IP UDP transport behind the existing asynchronous per-camera action service. Preserve AViPAS raw TCP behavior. Keep local listening port, reply timeout, and minimum request gap distinct. The tested Sony default gap is 100 ms after a completed reply; reset ACK also gets 100 ms. STOP takes priority over polling and must not be held behind unrelated queued requests. Handle ACK, completion, inquiry, errors, shutdown and late replies without blocking OBS threads. Put model defaults and evidence in a validated profile/driver. Identification must not gate basic connectivity until verified.

Manufacturer reference: https://pro.sony/s3/2019/01/16020225/AES6100121.pdf (A-ES6-100-12(1)). This is the SRG-300H manual; newer FR7/SRG-A manuals are not substituted as unit-specific evidence. Git implementations compared during diagnosis: bitfocus/companion-module-sony-visca and misterhay/VISCA-IP-Controller. The preserved probe is our standalone diagnostic implementation.
