# Program-only ON-AIR indicator — r20

## Confirmed operator workflow

Four camera scenes are stacked inside the working scene. Studio Mode shows
that scene in both Preview and Program. The operator brings another camera to
the top in Preview, prepares its position/zoom, then transitions to Program.
Previously the warning changed during preparation, before that camera aired.

OBS 32.2.1 frontend/OBSStudioAPI.cpp returns the editable programScene reference
from obs_frontend_get_current_scene(). OBSBasic_Transitions.cpp may duplicate
that scene privately for the actual transition output. Our previous scan read
the editable copy and therefore saw the Preview reorder/visibility changes.

## Change

currentProgramSceneSource() now calls acquireProgramOutputScene(), which reads
OBS output channel 0 and follows transition SOURCE_A references to the actual
scene, including private Studio Mode copies. A remains the outgoing scene
while transitioning; OBS promotes B to A on completion. Do not substitute
obs_transition_get_active_source(): OBS chooses B as soon as a transition starts.
Nested transitions have a 24-level bound. Missing/black/non-scene output gives
no camera match, with no fallback to editable Preview. All acquired references
are released or returned with explicit caller ownership.

The existing 250ms timer, raw-source UUID mapping, bottom-to-top visible scene/
group traversal, single highest-layer camera warning, colors and camera queues
remain unchanged. This is an operator cue, not pixel coverage analysis. During
a dissolve both cameras may contribute; this intentionally keeps the outgoing
warning until OBS settles the transition. A camera used live and in Preview is
still the same physical camera: moving it can change the live picture. The fix
isolates scene-layer edits, not physical camera hardware.

## Regression evidence

New native libobs test uses two camera sources, an editable working scene, a
private Program copy, and a registered transition. It checks output identity,
Preview reorder and visibility independence, outgoing selection at transition
start/progress, selection after settling, black/missing/non-scene output, and
source cleanup. It performs no camera IO or OBS UI interaction. The completion
fixture uses obs_transition_set to model settled A; it does not render a GPU
transition. Native build/test results are pending; local C++ syntax passed.
Existing packet/profile/runtime/concurrency suites remain in the CI gate.

## Operator acceptance

1. Program working scene shows camera 1 on top; ON-AIR must mark camera 1.
2. In Preview of the same working scene, bring camera 2 to top or change its
   visibility. Program picture and warning must stay on camera 1.
3. Prepare camera 2's preset/zoom. Confirm its warning does not replace camera 1.
4. Transition. Warning stays on outgoing camera during transition, then changes
   to camera 2 within the existing 250ms polling interval after completion.
5. Repeat with Cut and Fade, nested camera scenes, hidden sources, no mapped
   camera, and Studio Mode off. Check fade to black clears the warning.
6. Check original preset-source triggers and camera actions remain unchanged.

Product/source ids/version and user preset/settings files are unchanged.
Shared native validation selects all platforms. Local Windows overlay uses
Denver ZIP timestamps. Linux r19 diagnostics remain present; the independent
X11 launcher is not added to the plugin installer by this revision.
