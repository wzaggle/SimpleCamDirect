# ON-AIR sampling rule — v0.1.2

User-approved rule: in Studio Mode, hold the ON-AIR camera until a Program
transition completes. Preview visibility, ordering and selection changes must
not update the held indication, even when OBS shares editable scene contents.
Outside Studio Mode, continue following live visibility and stacking changes.

Implementation uses a separate OnAirPolicy latch. The existing actual-channel-0
scanner supplies candidates, but only startup, Program activation/completion,
scene-collection initialization or a Studio Mode switch permits a Studio sample.
Preview events never request one. Non-Studio mode samples on the existing 250ms
poll. An active libobs transition blocks either mode from replacing outgoing.

OBS's TRANSITION_STOPPED frontend event can originate at video completion before
transition audio settles. Queue the request, then wait until the channel-0
transition is no longer active before capturing settled SOURCE_A. Do not use an
arbitrary delay or select incoming B early. Existing camera command dispatch is
unchanged; this policy controls the visual ON-AIR indicator only.

The v0.1.1 passive diagnostic is retained. Logs now include held_camera, sampled
and transitioning. program_camera is the current scanner candidate; it may change
while held_camera remains correct. Zero means no matched camera. Preview scanning
is diagnostic only and cannot choose the indication.

Local Visual Studio source-overlay iteration only; no cloud builds are requested.
The dock uses compiled PLUGIN_VERSION from buildspec.json: v0.1.2. Existing v0.1.1
adaptive Properties changes are included. Real Windows/OBS acceptance is pending.

Checks for operator:

1. Confirm SimpleCamPTZ v0.1.2 in the dock title after rebuilding/installing.
2. In Studio Mode transition camera 1 to Program. Change Preview to camera 2,
   including reordering within the same working scene. Indicator must stay on 1.
3. Transition: retain 1 during the transition, then indicate 2 after completion.
4. Repeat with a cut and a scene containing no mapped camera (clear indication).
5. Disable Studio Mode. Live visibility/order edits should update the indicator.
6. Re-enable Studio Mode and restart OBS; initialize then hold appropriately.
7. If incorrect, send the current log's OnAirDiag v0.1.2 lines for the sequence.

Validation: 20 local policy checks pass; GCC/Qt 6.4.2 syntax checks pass for
plugin-main.cpp, camera_docks.cpp and preset_source.cpp. Native VS build and
Windows/OBS behavior remain for the maintainer to verify. No cloud jobs ran.
