# Rightmost settings gear and on-air title

The dock retains one settings button and transfers it between explicit
bottom-function-row layouts on changes to visible card count. Reparenting
is followed by show(); signal connections and checked state remain intact.
The observed width threshold and floating sizing rules are unchanged.

Each card stores the latest on-air camera index from the existing visible
feed scan. Its selected camera title is yellow (#FFFF00) only when the
indexes match, otherwise the existing green. The title style changes only
when its on-air state changes; font metrics remain unchanged. Runtime/card
selection refresh reapplies the latest state. No match is represented by -1
and never produces a yellow title. Existing selector indicators remain.

Acceptance: gear follows rightmost visible card in Controls/Settings, stays
functional across transfers, source feed changes update both titles, selection
changes update title state, and no-camera scenes clear caution. Recheck r8
floating limits, shared borders, height compression and OFF row. Source checks
passed; Windows/OBS runtime confirmation remains pending.
