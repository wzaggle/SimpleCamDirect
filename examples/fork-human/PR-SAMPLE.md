# Example PR — human-led documentation-only correction

> Fictional example showing the minimum evidence format. It is not a real test report or claim about a released UI.

## Change

- **Goal:** Correct a sentence in the setup guide so it names the current “Camera Configuration” window.
- **Summary:** Documentation wording only; no executable code or profile changed.
- **Scope:** Intended as a general upstream documentation correction.
- **Fork-local `AGENTS.md`:** `examples/fork-human/AGENTS.md`, section “Fork-local context”.
- **Compatibility impact:** None expected; no serialized settings, commands, or source behavior changed.

## Evidence

| Check | Result |
|---|---|
| Build | `NOT RUN` — documentation-only correction |
| Automated tests | `0 / 0 / 0 / 0` — no code or test fixture changed |
| Profile validation | `0 / 0 / 0` — no camera profile changed |
| Protocol fixture cases | `0 / 0 / 0` — no protocol implementation/profile changed |
| OBS behavior paths | `NOT RUN` — no OBS behavior changed |
| Hardware | `NOT RUN` — no camera interaction changed |
| Performance | `N/A` — no runtime code path changed |

**Maintainer check requested:** Confirm the UI label is current before merging.
