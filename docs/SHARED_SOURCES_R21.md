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
- If the containing scene is itself reused by another scene, the action is
  disabled: replacing its child would also change those parents. Make that
  containing scene independent first. The guard is rechecked before replacement.
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

## Native build evidence

Compiled revision: `1d1c70414dbe375239cbbe52561b4b5a4dc108f7`.
Run: https://github.com/wzaggle/SimpleCamDirect/actions/runs/40637641274

Windows x64, macOS arm64 and Ubuntu 24.04 x64 all compiled the plugin and passed
all six CTest suites (18 successful suite executions). This includes the new
real-libobs source-reference/replacement suite and all five existing suites.
Platform installers and the same-revision combined collector succeeded. This
does not establish live OBS Properties UI or real-camera acceptance.

An earlier fixture failed its transform check because a headless scene had zero
canvas height. It now uses the public scene settings/load API to establish a
1920x1080 canvas before adding items and verifies valid starting coordinates.
The production replacement implementation did not need a transform change.

Combined artifact: `12793374658`, 5,776,852 bytes.
SHA-256: `c60d1f3b612652fe0c00d6b8486af770d39f63b0f52983fa1edc953b53fdd321`.
Windows artifact: `12794895030`; macOS: `12793937704`; Linux: `12794391580`.
The source overlay is cumulative over r18, includes r19/r20 support changes and
uses Denver local ZIP entry timestamps for the existing MSBuild overlay workflow.
