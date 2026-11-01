# Profile and installation layout — v0.1.13

Maintainer approved CODE IT for the profile/installation split shown below.
This extends v0.1.12; no GitHub write or cloud build is part of the delivery.

## Camera Profiler

**Camera Profile** is the single authoring area for reusable model settings:
file/Open/Reload/Save As/New, profile name and profile revision, manufacturer,
model, one of the three network methods, camera communication port, VISCA address,
gap, reset setup delay, reply timeout and UDP local receive-port policy. Profile
id, evidence status and JSON format are displayed separately. Profile revision
is not the plugin's version or the JSON schema version. Older profiles without
an explicit revision display 1.0.0; New starts at 1.0.0. Revisions such as 1.2.0 or
1.2.1-draft are accepted and saved; the user edits them explicitly.

**Test Connection** contains only a temporary IP field, Test TCP/UDP, Stop &
Release, status and a short note. There are no editable ports/addresses/names
in this area. Busy tests lock the profile and test IP, but leave emergency release
available. Existing rendering, command editing, Test All, zoom discovery, ownership,
trace and bounded asynchronous transports are preserved.

UDP profiles have a **Same as camera port** checkbox for the local receive port.
When selected, there is one editable port number: camera communication port.
The receive-port number follows it and its separate editor is hidden. Uncheck to
enter a distinct fixed receive port. This is the computer's sending/listening
port, not the camera reply source port. Reply source ports, including the observed
Sony 65000, are still accepted from the selected camera IP by the driver.
Older UDP profiles with equal local/remote numbers initially show Same checked;
older profiles with unequal numbers retain the separate value. Save As records
`udp.localPortMode` as `same-as-camera` or `fixed`; numeric localPort is retained
for backward compatibility. Runtime resolves the alias from the profile's camera
port. TCP hides receive-port controls. Only Sony framing shows After reset; raw
methods have no Sony reset exchange. No serial transport is implemented yet.

## Cameras Tool

The tool contains slot, enabled state, raw video mapping, selected profile,
installation camera name and IP. Editable port and VISCA-address controls have
been removed for every profile. A selectable, read-only **Connection** summary
shows method, effective camera port, effective VISCA address and, for UDP,
computer receive port. Preset capacity remains profile-derived and read-only.
Use Camera Profiler to create a differently tuned profile, then select it here.

## Preserving legacy working connections

Existing raw-TCP slot overrides must not disappear when their controls disappear.
Before startup zoom/source dispatch, each loaded legacy TCP slot is converted to
a user-owned raw TCP profile containing its working port/address and the source
profile's gap/reply-timeout declarations (or legacy defaults when absent). The
slot selects that profile; name, IP, enabled state, video mapping and preset labels
and mappings remain intact. The original profile and old settings are not erased.
The migration itself sends no camera commands.

The new file uses a stable endpoint-specific name in the existing user profile
catalog. Matching slots reuse it; subsequent startups do not create more files.
An existing matching user variant is reused without replacing its tuning. A
conflicting file gets another filename rather than being overwritten. Profiles
with distinct ports/addresses remain distinct. As with Save As, derived profiles
are drafts with parent provenance, not newly certified hardware profiles.

Selecting a legacy profile later in Cameras performs the same conversion. If a
write fails, the original working endpoint and selected source remain intact;
startup logs the error and the read-only summary explicitly says Legacy endpoint
retained. No hidden fallback to a manual's default port is introduced.

The AViPAS built-in profile now displays the maintainer-tested TCP port **1259**
in the Profiler (profile revision 1.0.1). Its notes retain the manual's configurable
factory port 3001 and explain that a differently configured unit needs a variant.
Legacy slots using 3001, 4321 or another port retain those actual numbers during
migration. The Sony profile gains explicit revision 1.0.0; protocol defaults,
commands, timings and hardware evidence are unchanged.

## Evidence / Windows checks

124 headless Qt widget/localhost checks pass. The additional checks verify field
parentage, only-IP Test Connection input, revision validation/persistence, local
port alias/fixed policies, method-dependent field visibility, runtime alias
resolution, invalid policy rejection, legacy migration/restart/reuse, distinct
endpoints, installation/preset preservation and failure without endpoint loss.
The earlier Profiler TCP/raw UDP/Sony network tests remain included in that count.
Existing 18 Sony UDP integration tests, 41 Sony profile checks, TCP state 35 and
concurrency 34, packet/template fixtures, allocator 27, ON-AIR 29 and Properties
title 8 checks pass. Exported profiles, the migration result and both built-ins
validate against the additive schema. Strict GCC/Qt builds and Cameras/menu syntax
checks pass, with the same isolated logging stub/OBS 32 declaration used earlier.
The Profiler was visually inspected offscreen; the Cameras layout was source and
syntax checked. This is not a full native OBS acceptance claim.

Windows: close OBS, build/install RelWithDebInfo, reopen and confirm v0.1.13.
Verify the existing AViPAS still shows port 1259 in its connection summary and
works without re-entering the endpoint. Verify camera names/video mapping/preset
labels remain. Open Camera Profiler: profile fields should all be at the top;
Test Connection should have only IP as connection input. Load the Sony profile,
check 52381/100 ms defaults, Same as camera port, Test UDP, inquiry then intentional
movement/STOP. Save a custom name/revision and select it in Cameras after Refresh.
Verify raw methods hide reset and TCP hides receive-port settings. Full Windows
build/OBS/hardware acceptance and subsequent Mac M1/Ubuntu OBS testing are pending.
