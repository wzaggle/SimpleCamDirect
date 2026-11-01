# Preset editing — v0.1.14

The Profiler uses one checkbox caption: **Local receive port same as camera
port**. Unchecking it exposes the separate UDP receive port. Transport behavior
and saved connection settings are unchanged.

In Camera Control Rename mode, selecting another preset saves the current edit
and opens the next editor, keeping Rename active. Enter saves and exits; Escape
discards only the current edit. Camera/page changes keep their existing editor
cancellation behavior.

A blank or whitespace-only preset name cannot recall from Camera Control or an
OBS preset source. Set can still store that preset and Rename can name it.
Allocating a physical preset for an unnamed button does not automatically name
it. Clearing a name keeps its mapping and camera memory, but disables recall
until named again, including after restart.

The shared runtime checks names before accepting recalls. TCP, raw UDP and
Sony-framed UDP also check a queued recall immediately before transmission;
clearing its name or changing its mapping cancels it. A command already sent to
the camera cannot be withdrawn.

## Validation and local acceptance

Automated checks use actual Qt widgets and localhost transport simulators.
The current runs pass 141 Profiler/runtime checks, 13 widget checks, 35 TCP
state checks, 34 concurrency checks, 27 suffix-allocation checks, 29 ON-AIR
checks and eight properties-title checks, plus profile/packet fixtures. All 41
Sony profile checks and 18 localhost UDP integration tests also pass.
The isolated widget test substitutes only the OBS source-name refresh function;
it does not initialize OBS or operate a camera. Full native Windows/OBS/hardware
acceptance and Mac M1/Ubuntu acceptance remain pending.

Build/install windows-x64 **RelWithDebInfo** with OBS closed. Confirm v0.1.14.

1. Confirm the single Profiler checkbox and separate receive-port editor.
2. Rename several buttons without Enter between selections. Confirm all edits
   persist; Enter exits and Escape discards only the current unsaved edit.
3. Clear a named preset. Confirm it cannot recall from the dock or its existing
   OBS source, including after restart. Whitespace should behave the same.
4. Set an unnamed button. Confirm Store works but recall stays disabled until
   a name is entered. Repeat on an initially unmapped button.
5. Confirm named recalls, preset-source identity/suffix allocation and ON-AIR
   handling still work normally.

No GitHub commit or cloud build accompanies this local-testing package.
