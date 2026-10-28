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
