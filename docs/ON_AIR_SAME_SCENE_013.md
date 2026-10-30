# Same-scene Program commits — v0.1.3

Windows local VS overlay; no cloud builds requested. Preset Properties sizing
passed maintainer testing. Move Make independent copy above the scene names so
it remains accessible before a long list.

v0.1.2 passed holding ON-AIR during Preview edits and switching to a different
Program scene, but did not update after committing edits to the same scene.

This revision adds:

- Actual channel-0 transition start/video-stop/stop signal observation. The
  render/audio-thread callback only increments an atomic counter; it never
  touches Qt, scans scenes, or performs camera I/O. Filter other transitions out.
- Observe settled output scene UUID/pointer, not scene name, plus transition
  active-to-settled edges. Request a fresh Studio snapshot for those commits even
  without a frontend scene-changed event. Preview-only content edits do not
  change the held indication.
- Capture generation and active state before scanning. Completion during the
  scan remains observable on the next poll instead of consuming a stale sample.
- OBS 32's same-source no-op may emit neither transition nor scene events. Watch
  the localized main Transition button and quick-transition buttons in the
  identified Studio control panel (direct T-bar plus transition-options icon).
  These queued click observers request sampling after OBS handles the click;
  active transitions still retain outgoing until completion. Do not intercept
  clicks, alter OBS controls, or bind arbitrary buttons by text alone.
- Disconnect global signals on unload. Button connections belong to the dock;
  QPointer guards allow rebuilt Studio panels/quick buttons without stale links.

The no-op button fallback covers the GUI buttons. A programmatic/hotkey request
that emits no observable event and changes no output identity cannot be inferred
by the output observer alone. Do not override OBS's global hotkey router to work
around this; investigate separately if an operator reports that exact path.

Diagnostics retain OnAirDiag v0.1.3 and add transition_generation. The explicit
GUI fallback logs "explicit Program transition button". program_camera is the
scanner candidate; held_camera is the actual indicator. Zero means no match.

Validation: local policy test passes 29 checks, including same-identity completion,
same-named new output copies, instantaneous transitions between polls and a
missed-signal falling-edge fallback. Qt widget smoke passes 13 checks, including
copy-button placement above the names. GCC/Qt syntax checks pass for changed
plugin units. Native VS/OBS runtime testing is pending; no native build claimed.

Operator: confirm v0.1.3; put camera 1 on Program; edit that same scene in Preview
to put camera 2 on top. Indicator stays on 1. Press Transition (and test a quick
cut): retain outgoing during a transition, then indicate 2, even with the same
scene name. Test a different scene and non-Studio live edits as before. If it
fails, send the current log showing the full sequence and which button was used.
