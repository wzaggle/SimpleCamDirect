# Fork-local coding context — fictional human contributor sample

This fork keeps the shared upstream instructions above this section. This sample addendum demonstrates the minimum expected context for an ordinary human-led contribution; replace it with the actual fork's details.

## Fork-local context

- **Fork purpose:** Maintain a small local documentation correction for our volunteer camera operators.
- **Upstream base:** `<upstream-commit-sha>`
- **Fork-only behavior:** None. This change corrects wording in the local setup notes and must not change camera commands or scene-source behavior.
- **Affected areas:** `docs/USER_GUIDE.md` only.
- **Target platforms:** Documentation-only; no platform behavior changes.
- **Required PR evidence:** State the goal, summary, upstream-or-fork scope, affected files, and compatibility impact. Confirm the referenced UI labels against a named build. Report build status and platform, automated check pass/fail/skip/total counts, profile valid/invalid/scanned counts, protocol fixture pass/fail/added counts, OBS behavior paths, exact hardware/platform checks, known gaps, and a maintainer check. Mark unrelated runtime metrics `N/A — documentation-only`; do not leave fields blank.
- **Safety limits:** Do not edit camera profiles, VISCA command bytes, preset behavior, or credentials as part of this documentation change.

## Update rule

Update this addendum if this fork adds implementation changes, supported hardware, platform requirements, or new tests. Keep fork-specific preferences out of upstream unless they apply to the shared project.
