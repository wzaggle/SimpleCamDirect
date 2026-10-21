# SimpleCam Direct for OBS

**Status:** planning baseline; project name selected, implementation baseline not yet established.

SimpleCam Direct is a new, cross-platform OBS PTZ plugin. This blueprint captures its intended architecture and contribution workflow before implementation begins. It is not the current SimpleCamQt application or the already-tested file-bridge OBS plugin.

The planned GitHub repository name is `SimpleCamDirect`. See [`docs/PROJECT_DECISIONS.md`](docs/PROJECT_DECISIONS.md) for the decisions recorded during initial project planning.

## Product principle

Keep the hardware inexpensive, lightweight, and durable. Make the software pretty, easy, and fully functional. Prefer a polished, clear interface and dependable end-to-end behavior over features that add weight without helping the user's camera workflow.

## Design goals

- Keep the proven named, scene-embedded preset source workflow.
- Move camera commands into the OBS plugin so scene actions can command a camera directly.
- Keep a compact, detachable OBS dock for live camera operation and camera cards.
- Put camera communication setup in its own configuration window.
- Load camera model profiles from JSON or JSON-formatted `.camera.cfg` files, validate them, and add only valid compatible profiles to the supported-camera list.
- Make the repository straightforward for people and coding agents to understand, build, test, and extend.
- Establish behavior on Windows first; port that known behavior to Apple Silicon, then Linux.

## Documents

- [`AGENTS.md`](AGENTS.md) — root instructions for coding agents and contributors.
- [`docs/PROJECT_MAP.md`](docs/PROJECT_MAP.md) — logical components and interactions.
- [`docs/PROJECT_DECISIONS.md`](docs/PROJECT_DECISIONS.md) — agreed project identity, first milestones, coexistence behavior, platform direction, and coding-agent expectations.
- [`docs/CAMERA_PROFILE_REVIEW.md`](docs/CAMERA_PROFILE_REVIEW.md) — vendor-document comparison and profile design requirements.
- [`docs/CAMERA_PROFILE_EDITOR.md`](docs/CAMERA_PROFILE_EDITOR.md) — in-plugin profile editing, safe command testing, and Save As behavior.
- [`docs/CAMERA_ACTION_CONTRACT.md`](docs/CAMERA_ACTION_CONTRACT.md) — initial semantic camera functions and protocol-mapping boundary.
- [`docs/PROFILE_VARIABLES.md`](docs/PROFILE_VARIABLES.md) — constrained command-template placeholders, typed variable library, VISCA example, and future ONVIF form.
- [`docs/PROTOCOL_EVOLUTION.md`](docs/PROTOCOL_EVOLUTION.md) — staged VISCA, ONVIF, and NDI implementation plan with compatibility and regression gates.
- [`docs/DEVELOPMENT_ROADMAP.md`](docs/DEVELOPMENT_ROADMAP.md) — phased 0.1.x AViPAS milestone through generic engine, profile editor, ONVIF, additional cameras, and beta testing.
- [`docs/SIMPLECAMQT_PARITY_CHECKLIST.md`](docs/SIMPLECAMQT_PARITY_CHECKLIST.md) — side-by-side behavior and coexistence test for the first native plugin milestone.
- [`docs/UI_THEMING.md`](docs/UI_THEMING.md) — detachable OBS dock appearance choices for SimpleCam Classic and Match OBS modes.
- [`schemas/camera-profile.schema.json`](schemas/camera-profile.schema.json) — draft JSON Schema for model profiles.
- [`examples/profile-template.json`](examples/profile-template.json) — schema-valid teaching example; not a supported camera profile.
- [`docs/PLATFORM_TEST_CHECKLIST.md`](docs/PLATFORM_TEST_CHECKLIST.md) — layered test and fault-isolation procedure.
- [`docs/FORK_AND_AGENT_CONTRIBUTIONS.md`](docs/FORK_AND_AGENT_CONTRIBUTIONS.md) — fork-local agent context and tiered PR evidence standards.
- [`docs/PULL_REQUEST_EVIDENCE_TEMPLATE.md`](docs/PULL_REQUEST_EVIDENCE_TEMPLATE.md) — copyable minimum human PR report and fuller AI-agent report.
- [`examples/fork-human/AGENTS.md`](examples/fork-human/AGENTS.md) and [`examples/fork-human/PR-SAMPLE.md`](examples/fork-human/PR-SAMPLE.md) — small, explicitly fictional human-contributor example.
- [`docs/USER_GUIDE_PLAN.md`](docs/USER_GUIDE_PLAN.md) — scope and structure for the eventual full user manual.

## Evidence and maturity labels

Keep these states distinct in the app and in the documentation:

- **Draft:** profile syntax is valid, but commands or behavior have not been verified.
- **Protocol-reviewed:** command definitions have authoritative source references and have been checked against the model documentation.
- **Hardware-tested:** the profile has been tested with the named camera model and firmware, over the named transport.
- **Maintained:** tested against a stated plugin and OBS version, with an identified maintainer or manufacturer contact.

A profile must not be advertised as hardware-tested merely because it parses or builds.

## Research source policy

Prefer the exact camera model's manufacturer protocol reference, user manual, and firmware notes. Generic VISCA material can explain packet semantics but cannot establish that a particular camera supports a command or IP framing variant. The vendor source links in `docs/CAMERA_PROFILE_REVIEW.md` are an initial reading list, not a claim that every model in those families has been tested.
