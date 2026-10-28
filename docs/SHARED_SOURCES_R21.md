# Shared preset sources and independent copies (r21)

Status: implementation for operator testing; not an accepted production checkpoint.

## Operator behavior

SimpleCam Preset Properties includes a compact read-only **Used in scenes** list.
It shows direct references and parent scene/group paths, including hidden items.
A repeated entry can mean two occurrences in one scene. References match the same
OBS source UUID, not unrelated sources recalling the same camera/preset. The list
refreshes while Properties is open.

For a shared source, **Make independent copy** replaces only the direct item in
the scene captured when Properties opened. It closes the shared Properties,
creates a separate source with the current settings and a unique numeric suffix,
then opens the copy's Properties. The original source's pre-dialog settings are
restored through OBS's Cancel button before replacement. Subsequent edits to the
copy do not affect other references. Ordinary OK still edits the shared source;
ordinary Cancel does not create a copy.

The replacement preserves stack order, visibility, selection, lock, transform,
crop, blending, scale filtering and private item settings. It duplicates source
filters through OBS's source-duplicate API. It does not create, erase or duplicate
camera memory presets, or request a camera move. The suffix is persisted in the
copy's settings so automatic name refreshes preserve its identity. Existing
sources need no migration; the type id remains `simplecamdirect_preset`.

## Boundaries

- Only the captured scene's direct item is eligible. With multiple occurrences,
  select exactly one. Ambiguous context disables the action.
- Nested scene/group paths are listed, but contents are not silently edited from
  a parent. Open the actual containing scene. Group-only contexts without an
  unambiguous direct item are not supported in this first version.
- Switching scenes does not retarget the operation. Deleted/replaced items and
  scene-collection changes are revalidated. Failure leaves the old reference.
- OBS applies ordinary Properties edits live. The copy action restores the
  original before duplication, but edits made before pressing it may temporarily
  affect the shared source. Press Make independent copy before making changes.
- This first version does not register a custom OBS Undo action for replacement.
- UI is scoped to our source's Properties marker/UUID, without private OBS ABI.
  Queued work and added UI timers are cancelled during plugin UI shutdown.

## Validation

Local GCC/Qt 6.4.2 syntax checks pass for the Properties code and new libobs test
fixture. Native builds/tests and live OBS acceptance must be recorded separately.
`simplecamptz_source_reference_tests` covers real scenes/sources: paths, separate
UUID/settings, ambiguous/stale targets and preserved item state.

Operator checklist:

1. Share one source across two scenes. Verify scene names, hidden references and
   the orange cue. Press Make independent copy in the copied scene.
2. Verify new Properties opens with the same settings/new suffix. Change preset,
   trigger and delay; the other scene must remain unchanged.
3. Detach one of three references: remaining two stay orange; copy is normal.
   Detach one of two: both become normal.
4. Verify stack position, visibility, lock and save/restart persistence. Copy an
   independent source again and verify another unique number.
5. Ordinary Cancel creates no copy. Scene changes after opening Properties do not
   retarget it. A removed target cannot remove another reference.
6. Inspect nested paths and ambiguous occurrences. Verify disabled-button help.
7. Recheck compact sizing, camera controls and source triggers on all platforms.
