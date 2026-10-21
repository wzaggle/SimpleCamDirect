# Pull request evidence template

Copy this into each pull request. Human-led submissions may use the compact section. AI-agent changes fill out both sections. Replace every `TBD`; use `N/A` with a reason rather than leaving a metric ambiguous.

## Compact human-led report

### Change

- Goal/issue:
- Summary:
- General upstream improvement or fork-specific change:
- Affected files/modules:
- Fork-local `AGENTS.md`: section below or exact fork commit link:
- Compatibility or migration impact:

### Evidence

| Check | Result |
|---|---|
| Build(s) | `PASS/FAIL/NOT RUN`; platform, architecture, OBS version, command/reason |
| Automated checks | `passed / failed / skipped / total`; command or N/A reason |
| Profile validation | `valid / invalid / scanned`; validator or N/A reason |
| Protocol fixture cases | `passed / failed / added`; or N/A reason |
| OBS behavior paths | `passed / failed / not run`; relevant source/dock paths |
| Hardware | exact model/firmware/transport/platform and pass/fail/not-run matrix; or not run |
| Performance | metric/sample/baseline if relevant; otherwise N/A reason |

Known limitations and requested maintainer test:

## Enhanced AI-agent report

### Agent context

- Agent/tool and model/version, if known:
- Upstream base commit:
- Fork-local `AGENTS.md` read and updated: yes/no; summary/link:
- Assumptions and inferred behavior:

### Implementation and risk

- Behavior changed:
- Design areas touched:
- Failure/safety risks considered:
- Secrets/privacy/logging review:
- Generalizable upstream changes vs fork-only changes:

### Required metrics

| Metric | Result |
|---|---|
| Build matrix | `passed / attempted / required` by OS + architecture; compiler and OBS version |
| Automated tests | `passed / failed / skipped / total`; exact commands |
| Profile validation | `scanned / valid / invalid`; schema and validator command |
| Protocol fixtures | `passed / failed / added` per driver/profile; packet comparisons where relevant |
| OBS behavior | `passed / failed / not run` by event/action path |
| Hardware tests | model, revision, firmware, transport, OS/architecture, tested operations and outcomes |
| Performance/resource | sample count, median/p95, baseline comparison, or N/A with rationale |

### Reproduction

1. Exact configure/build command(s):
2. Exact test command(s):
3. Minimal steps for the maintainer to reproduce:
4. Untested platform/device combinations:
