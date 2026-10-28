# Future upgrade: A–D custom camera buttons

Status: design note only; not implemented or scheduled for v1.0.

## Controls

- Optional small row at the bottom of each camera card: A/red, B/yellow, C/green, D/blue, in that order.
- Letters and colors provide two ways to identify each function. A tooltip describes the profile-defined assignment.
- Show or hide the entire row as one unit. When enabled, retain all four positions; undefined buttons are muted and disabled. One assigned button uses the same vertical space as four.
- Actions target the camera selected on that card and use the existing per-camera serialized command queue and reply handling.

## Profile definitions

- Each button can reference an ordered sequence of static VISCA commands for camera features outside SimpleCam's standard controls.
- Possible assignments include brightness, gamma, backlight, or a complete image look, where the camera supports those commands. These examples do not imply support by the current AViPAS profile.
- Keep the optional Startup sequence separate from manually invoked Other/A–D actions. Reusable command definitions may be shared between them.
- Startup examples include horizontal/vertical image flip, color modes, and default settings. Prefer explicit state-setting commands over toggles so reapplying setup does not reverse it.
- Validate commands and expose send/reply results through the existing profile tooling. Exact schema and editor UI remain to be designed.

Implementation requires a future CODE IT instruction. This note does not alter current camera profiles, startup behavior, or the control layout.
