# Mode-specific ON-AIR inspection — v0.1.4

User-approved split:

- Studio Mode: actual output transition -> held Program scene -> scene items.
  Preserve v0.1.3's snapshot hold, transition completion/output identity and GUI
  commit detection. Preview edits alone cannot release the held indication.
- Non-Studio Mode: obs_frontend_get_current_scene() -> scene items. Continue
  tracking live visibility and stacking changes on the existing 250ms timer.

Select the mode once per scan and pass it explicitly to the scene acquisition
helper. All returned OBS references are released. Remove the Preview scene
acquisition and recursive scan from ON-AIR diagnostics as well as operation.
Preview preset-source dispatch and source Properties context remain unchanged:
those unrelated features still legitimately use the Preview scene API.

Logs identify tree=output-transition or tree=frontend-current-scene, inspected
scene identity, matched camera paths, held camera, sampling and transition state.
No longer log Preview camera candidates. Zero camera means no mapped feed.

Local VS overlay only; no cloud jobs. Dock version and build metadata: v0.1.4.
Preserve accepted adaptive Properties sizing and copy button above scene names.
Native Visual Studio/OBS operator validation remains pending.

Test: Studio Preview edits leave ON-AIR held; same-scene and different-scene
Program commits update it; disable Studio and live item changes update directly.
Re-enable Studio, verify initialization and renewed hold. If incorrect, capture
OnAirDiag v0.1.4 lines showing the active tree and transition sequence.

Validation: 29 local policy checks pass. GCC/Qt 6.4.2 syntax checks pass for
plugin-main.cpp, camera_docks.cpp and preset_source.cpp. Native VS build and
actual OBS mode-switch behavior still require maintainer testing.
