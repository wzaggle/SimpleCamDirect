# Ubuntu floating-dock investigation — r19

## Evidence and scope

The maintainer reports Ubuntu camera/source functions work, but the floating
control dock cannot be dragged. Dragging its top resize edge changes its bottom
edge; other OBS docks drag normally. Enlarging OBS did not help. Root cause is
not established. Windows and M1 startup zoom initialization passed separately.

OBS 32.2.1's frontend/OBSStudioAPI.cpp creates an OBS-owned dock and explicitly
starts it floating. This plugin previously recalculated floating maximum size
from current wrapper/content geometry during resize handling. That is a suspect,
not proof of a window-manager or Qt defect.

## Scoped experiment

Linux alone starts with wrapper maximum size released. Card centering, minimum
content size and 320px card caps remain. No forced initial docking, geometry
persistence, native handle creation, global styles, window flags, new dock or
manual drag/resize implementation is added. Windows and Mac paths are unchanged.

Tools → SimpleCamPTZ Linux Dock Diagnostics provides:

- Release floating size limits (on initially): turn off for an A/B comparison.
- Use Qt title bar (off initially): tests Qt's own dock drag handler instead of
  native decorations. Float/close controls follow OBS's lock/features flags;
  blank title-bar mouse events propagate to QDockWidget.
- Dock into OBS: explicit recovery using the existing wrapper and public Qt API;
  respects Lock Docks and allowed dock areas.
- Log dock state: capture bounds, limits, flags, features, area and platform.

Toggles reset on restart. OBS continues to own saved placement. Passive logs
use [SimpleCamPTZ LinuxDockDiag] build=r19 and report Qt platform (xcb/wayland),
Qt version, XDG session type, native/client pointer events, resize and placement.
Move/resize samples are limited to ten per second; no pointer events consumed.
No camera settings, profiles, names, commands or source ids change.

## Operator procedure

1. Close OBS and install this revision's native Ubuntu DEB. Reopen OBS.
2. Leave diagnostic defaults unchanged. Try moving by the title bar, resizing
   both top and bottom edges, docking and undocking.
3. If stuck, enable Use Qt title bar and repeat; report which setting works.
4. Dock into OBS is also available independently as a recovery/test action.
5. Choose Log dock state and provide the current OBS log if either path fails.
6. Check camera selection, preset flash, startup zoom and clean OBS shutdown.

Do not change desktop/session or disable host locks to make the test pass.
Do not infer Ubuntu window-manager drag success from offscreen Qt tests.

## Validation

Local Qt 6.4.2/OBS 32.2.1 syntax check passed. Optional title-bar smoke test:
7 checks, 0 failures (float/redock, lock features, close). This uses offscreen
Qt, not a live Ubuntu desktop. Native Linux build/tests/package are pending.
Windows/Mac preprocessor-excluded changes are checked for preservation; their
native jobs are deliberately not scheduled for this Linux-only experiment.
