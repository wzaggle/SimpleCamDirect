# Shared Properties default width — v0.1.6

Set shared-source Properties initial client width to the maintainer-selected
610 Qt logical pixels, capped to available screen width minus frame/margins.
Apply once after the source fields/sharing state are identified; repeated list
refreshes preserve manual resizing. Unshared dialogs retain OBS-supplied width.
Keep adaptive height and keep the dialog within the screen if enlargement would
otherwise leave an edge off-screen. No fixed width or maximum-width constraint.

ON-AIR implementation files are byte-identical to the accepted v0.1.4 lineage.
GCC/Qt 6.4.2 syntax check passes for preset_source.cpp. Native Windows/OBS visual
verification is pending. No cloud build or installer generation was performed.

After overlay/rebuild/install, confirm v0.1.6. Open a shared source: verify the
610px opening width and full independent-copy caption. Resize manually and wait
for list polling; it should retain the chosen width. Unshared behavior stays as
before.
