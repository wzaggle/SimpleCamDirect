# Fork and coding-agent contribution standard

## Purpose

The project welcomes human-led and AI-assisted work from forks. Each fork should carry enough local context for a new contributor or coding agent to understand why its changes exist, what differs from upstream, and what evidence is required before proposing a pull request.

## Fork-local `AGENTS.md` requirement

Each fork maintains its own root `AGENTS.md`:

1. Keep the shared upstream rules intact unless the fork intentionally documents an alternative and its reason.
2. Add a `## Fork-local context` section. Record the fork purpose, upstream base commit, intended behavior differences, affected areas, target platforms, and validation expectations.
3. Update that section whenever the fork's scope, architecture, supported hardware, or tests change.
4. Before editing, every coding agent reads the shared rules and fork-local section, then summarizes the task against both.
5. Before opening a PR, the contributor includes the relevant fork-local section or a link to the exact fork commit containing it, plus the evidence report below.
6. Fork-only preferences stay in the fork. Propose changes to upstream's `AGENTS.md` only when they apply to all contributors.

The fork-local section must name the evidence expected for the fork's changes. At minimum, retain the PR evidence fields below and add any fork-specific build targets, devices, operations, or acceptance thresholds. A pull request without the required entries or metrics is incomplete; write `N/A` with a reason rather than leaving fields blank.

The parent repository's `AGENTS.md` contains the shared instruction to maintain this fork-local context. A fork may preserve it as a local-only change; the PR should not replace upstream's shared file with fork-specific instructions.

## Minimum evidence for a human-led pull request

Every PR must state:

- What changed and why; link the issue or include the user's goal.
- Whether the change is intended for upstream generally or only for the fork.
- The fork-local `AGENTS.md` section or exact fork commit link.
- Files/modules affected and user-visible behavior changes.
- Build/configurations run: `PASS`, `FAIL`, or `NOT RUN` with a reason.
- Automated checks: numeric pass/fail/skip counts, or `0 / none applicable` with a reason.
- Profile/schema validation: files checked and accepted/rejected counts, or `N/A` with a reason.
- Protocol fixture results for command/transport work: passed/failed case counts, or `N/A`.
- Hardware/platform checks: exact model/firmware/transport/OS used and cases passed/failed/not run; never claim hardware verification based on simulated tests.
- Known gaps, risks, and a manual test the maintainer should perform.

This minimum applies to all human-led changes, including small changes. Keep the report proportionate: a docs-only PR can mark runtime checks `N/A — documentation-only`, but must still say what changed, which files were affected, and what the maintainer should verify.

Docs-only or UI-only contributions can honestly report protocol, profile, or hardware metrics as `N/A` when they do not apply. Do not invent test coverage, test devices, command timings, or platform results.

## Enhanced standard for AI coding agents

AI agents must satisfy the human minimum and add a reproducible engineering record:

### Before editing

- Read upstream `AGENTS.md`, fork-local context, the project map, relevant behavior contract, and affected profile/test docs.
- Identify the starting commit and inspect the relevant implementation, tests, and build files.
- Summarize the requested behavior, affected paths, compatibility assumptions, and failure/safety risks.
- For camera work, identify exact manufacturer source references before changing commands. If no authoritative source or hardware is available, label the change draft and state that limit.

### During implementation

- Keep changes focused and preserve unrelated fork work.
- Add/update automated tests for profile parsing, command encoding/decoding, source migration, queue sequencing, and failure handling when applicable.
- Do not run unbounded camera moves or destructive preset/power commands without an explicit safe test target.
- Do not block OBS callbacks/UI on network or serial operations.
- Keep secrets out of profiles, test fixtures, logs, screenshots, and PR text.

### Required numerical metrics in the PR

Report values, using `0` or `N/A — reason` where a metric does not apply:

| Metric | Required report |
|---|---|
| Build matrix | `passed / attempted / required` per OS and CPU architecture; compiler/toolchain and OBS version |
| Automated tests | `passed`, `failed`, `skipped`, and total; list exact test command(s) |
| Profile validation | JSON files scanned, valid, invalid, plus schema version and validator command |
| Protocol fixtures | cases passed/failed/added for each changed driver or profile; include packet-level expected/actual comparison for protocol changes |
| OBS behavior checks | relevant source/dock paths passed/failed/not run (for example Program, Preview, Both, delay, restart/persistence) |
| Hardware test matrix | model, hardware/firmware revision, transport, OS/architecture, operations attempted/pass/fail/not run |
| Performance/resource impact | for affected paths, sample count plus median/p95 command or queue latency and baseline comparison; include CPU/memory or startup/shutdown measurements when those are the affected resource; otherwise `N/A — no performance-sensitive path changed` |

The agent must include exact commands run and concise outcome excerpts. Record the revision tested and the required OBS/plugin versions. A platform that was not available must be `NOT RUN`, not silently omitted. Compile-only evidence is not a runtime/platform pass. Profile validation is not camera testing. A successful send is not proof of command completion unless the protocol response was observed and parsed. Never use aggregate pass rates to hide an individual failed or skipped required check.

### Before opening the PR

- Update the fork-local `AGENTS.md` if new constraints, metrics, architecture, or platform requirements were discovered.
- Update relevant project docs and user-facing text.
- Provide a concise diff summary, test evidence, untested items, and required maintainer actions.
- Distinguish the AI's inferences from facts established by source, test output, or hardware.
- Recommend which changes are generalizable to upstream and which should remain fork-only.

## Review and merge policy

- A maintainer reviews all changes; AI output is not self-approval.
- Protocol command changes need source provenance and encoder/decoder fixtures; physical validation is required before a profile is promoted to hardware-tested.
- The upstream maintainer may request a smaller PR, different architecture, or additional evidence.
- Fork-specific instructions and defaults are not merged upstream unless they improve the shared contributor process.
- No contributor is required to use an AI agent. Human-led changes use the minimum evidence level and can still receive full review.
