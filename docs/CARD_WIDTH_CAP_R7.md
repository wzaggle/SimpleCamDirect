# Centered card width cap

Individual camera cards cap at 320 logical pixels. The outer OBS dock remains
unbounded horizontally. Existing equal-stretch cards consume available width
until capped; symmetric expanding zero-stretch side spacers consume excess.
Settings header columns use matching caps and side spacers, with the single
shared group editor capped to 320/642 for one/two cards. Existing preset
compression and bottom-row sizing remain unchanged.

Floating-only hard outer window limits are a separate proposed change and
are not implemented here. Runtime checks: centered cards at excess width,
Settings columns/group editor, narrow/tall states, both shared splitters.
