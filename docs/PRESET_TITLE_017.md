# Plain Properties window title — v0.1.7

After the existing deferred probe identifies our exact source dialog, enable
window-title cleanup and apply it immediately. Reapply on WindowTitleChange so
OBS source-renaming updates do not restore visible raw markup. A reentrancy guard
prevents recursive cleanup during setWindowTitle. Unrelated dialogs are not opted in.

Remove the generated <span style="color:#FF6050;"> wrapper and its trailing shared
asterisk. Decode QString::toHtmlEscaped entities exactly once, preserving literal
entity text, source suffix numbers and the host's localized Properties prefix.
No actual source name changes: OBS's orange reference cue remains intact throughout
editing and needs no restoration after close.

Eight local QtCore title checks pass, covering plain titles, generated spans,
escaped characters, literal entity text, translated prefixes, external OBS suffixes,
malformed input and literal asterisks. GCC/Qt 6.4.2 syntax check passes for
preset_source.cpp. Native VS/OBS testing is pending. No cloud build was started.

Confirm v0.1.7; open an orange shared source's Properties and verify the plain title
with its number. Change its preset, verify the refreshed title stays plain; close
and confirm its source-list orange cue still works. Check an independent source
opens normally. Accepted ON-AIR, compact layout and 610px shared width are unchanged.
