# Floating-only limits, r8

The Controls widget discovers its enclosing QDockWidget on show and resize.
A connection owned by the Controls widget observes topLevelChanged; a
QPointer guards dock lifetime. Redocking releases maximum size immediately.
The queued responsive pass applies measured caps only while isFloating().

Maximum floating client size includes compact dual content width and full
control height, plus the measured dock/content inset. Native window frame
size is not counted twice. Minimum sizing and preset compression remain as
in r7. The dual threshold has no extra upper margin, allowing two cards at
the compact floating maximum without admitting them below their minimum.

Do not add fixed dock dimensions or host-wide style overrides. Docked mode
retains unbounded outer maximums, with centered 320px internal card caps.

Acceptance: floating maximum/minimum, one/two switching, Settings/Controls,
redock release, both shared dock boundaries, repeated undock/redock and
restart/shutdown. Log applied sizes and release. Windows/OBS testing pending.
