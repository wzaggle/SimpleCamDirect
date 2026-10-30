# Adaptive shared-source Properties (r22)

Scope: Windows-only installer validation first. Maintainer passed r21's independent
copy functionality; this revision changes only its display and initial sizing.
Do not produce Mac/Linux packages for this revision until Windows acceptance.

- Hide the entire Used in scenes section when the source has fewer than two
  reference paths; no unshared status message and no blank reserved section.
- Shared scenes appear as plain non-interactive text, one name per line, in a
  borderless scroll area. Keep the independent-copy button and existing guards.
- Initial dialog height is the compact form baseline plus the visible section's
  measured text/header/button height. Cap it to the current screen's available
  height, including floating frame allowance. Long names can scroll horizontally;
  long lists scroll vertically only when necessary.
- Measure after OBS constructs the fields. Refit when the scene-name list or
  sharing state changes, not every poll; unchanged polling preserves manual size.
- Do not change dialog width, shared source settings, naming or replacement code.

Local GCC/Qt 6.4.2 syntax check passes. The offscreen widget smoke test in
tests/sharing_section_smoke.cpp passes 12 checks: collapsed unshared section,
plain non-selectable names, per-line growth, budget limit, overflow scrolling,
collapse after sharing ends, and unchanged-list detection. This is widget-level
verification, not a live OBS/Windows acceptance claim.

Windows operator checks:

1. Unshared source opens compact with no sharing heading, text or copy button.
2. Shared source opens tall enough for names and Make independent copy. Compare
   two versus several references; width should stay the same.
3. Large lists stop growing at the available screen height and scroll. Nothing
   should be clipped below the screen. Check the normal Windows scaling setting.
4. Copy still preserves other references and opens an unshared compact dialog.
5. Manually resize, then wait several seconds: no repeated resizing on a stable
   scene list. Other source types' Properties must remain unaffected.

After Windows acceptance, restore ci/push-platform.txt to all and validate the
same source on macOS arm64 and Ubuntu. No cross-platform package claim yet.

## v0.1.1 local Visual Studio overlay

Maintainer requests source overlays and local Windows builds for the next
iterations. No additional cloud build is requested for this overlay. The adaptive
Properties code at a21b536d363aff4fd99b1a055642e1ad11734d4b passed all six native
Windows suites in run 40644194470; the subsequent version/ON-AIR instrumentation
passes local GCC syntax checks only and still needs the maintainer's VS build.

The dock title now reads the compiled PLUGIN_VERSION, starting at v0.1.1.
CMake tracks buildspec.json so later patch increments trigger reconfiguration.
Keep the installer script version consistent even when sending source only.

ON-AIR behavior is NOT declared fixed. Passive OnAirDiag v0.1.1 logs record
actual output/Program, frontend Program and Preview source identity, camera
matches and their parent scene UUIDs. Only actual Program selects the indicator;
the extra Preview scan is diagnostic. Logs emit only when that state changes.

To capture: start OBS; transition the working scene to Program; reorder/change
the top camera in Preview without transitioning; then transition. Send the
current OBS log containing [SimpleCamPTZ OnAirDiag v0.1.1] lines. This distinguishes
root-scene identity from shared nested-scene changes without guessing a fix.
