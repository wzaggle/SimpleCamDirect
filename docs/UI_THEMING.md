# Camera Control dock appearance and theming

## Goal

The Camera Control surface should work as an OBS dock whether docked in the main frame or detached as a floating window. When docked, the full control surface follows the active OBS theme exactly; do not apply SimpleCam-specific color overrides. When detached, the user may optionally give the dock content more of the familiar SimpleCam identity. Redocking immediately restores the exact OBS-themed appearance. The separate Cameras dock follows OBS styling and contains profile and communication configuration.

## Appearance choices

Offer a user preference with two modes:

| Mode | Dock content behavior |
|---|---|
| **Follow OBS** | Use the active OBS theme for all dock content in both docked and floating states. This is mandatory while docked and is the default while floating. |
| **SimpleCam Classic (floating only)** | When the dock is detached, restore the familiar SimpleCam camera-control and preset-group colors in the dock content. Keep the floating dock's OBS/Qt-managed frame and title bar. Redocking disables the Classic content styling immediately. |

Both modes have the same controls, layout, behavior, and accessibility requirements. The mode changes presentation only. **Follow OBS is the default** for docked and floating states. Offer a separate floating-appearance preference for users who want **SimpleCam Classic** while detached; it never changes the docked presentation. Store the preference as plugin UI state, separate from camera profiles, camera connections, and scene source settings.

## Docking and implementation boundary

- Build the control surface as an OBS-managed dock. When detached, it remains a floating OBS dock with OBS's own frame/title treatment; it does not become an unrelated application window.
- While docked, let OBS/Qt styling provide the full appearance without SimpleCam-specific color overrides. When floating and set to SimpleCam Classic, apply those colors only within the camera dock's own widgets or custom-painted controls.
- Do not set an application-wide stylesheet or palette from the plugin; that would alter OBS and other plugins.
- Keep the floating Classic camera/preset color mapping in named theme tokens (for example, selected camera, preset group, set confirmation, error, disabled). Do not scatter literal color values through control logic.
- Recompute theme-aware styling when the OBS theme changes. Preserve contrast and meaning in both dark and light themes; never rely on color alone to communicate camera selection, preset group, error, or confirmation.
- Avoid baking theme colors into icons or image resources when a style-aware vector/custom widget can render them. Keep any fixed Classic colors explicit and localized.

## Verification

Test the dock in both docked and floating states, using every supported OBS theme family available on the target platforms. Verify that docked content always matches OBS and never shows Classic overrides; floating Follow OBS updates with theme changes; floating SimpleCam Classic retains its accents; and redocking immediately restores OBS styling. Neither choice may change layout geometry or control behavior. Test selected/unselected, hover/focus, disabled, error, group, and flash-confirmation states for readable contrast.

The OBS frontend exposes a theme-changed event (available since OBS 29); subscribe through the supported frontend API rather than polling. Keep any version-specific fallback explicit if the minimum OBS version changes.
