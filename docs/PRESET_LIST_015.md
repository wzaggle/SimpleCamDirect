# Compact shared-source Properties — v0.1.5

Maintainer passed v0.1.4 ON-AIR in all tested modes. Its C++ implementation and
policy headers are byte-identical in this revision; only build metadata changes
to v0.1.5 for the delivered iteration. No ON-AIR behavior change is intended.

Properties now matches the supplied mockup:

- Remove the separate sharing heading. The button itself reads: "Click here to
  make this preset independent from these other scenes:".
- Show other scene names below it, one per line as plain text. Exclude all paths
  rooted in the captured editing scene by scene UUID, not a string/name match.
- Collapse duplicate display paths. Preserve indirect parent/scene paths.
- Keep actual sharing detection separate from this filtered display. Two total
  references with one other scene still show the button and that one scene.
- If multiple occurrences exist only within the current scene, keep the action
  available under its existing selection guards, use a same-scene caption and
  show no empty list. An unshared source still hides the entire section.
- Recalculate height without the removed heading. Preserve adaptive screen
  limits, scrolling, independent-copy lifecycle, source numbering and guards.

Validation: 17 local Qt widget checks pass. GCC/Qt syntax checks pass for
preset_source.cpp and the updated source-reference test fixture. The fixture
adds a root UUID assertion for future native test runs; that new libobs assertion
was not executed locally. Native Visual Studio/OBS UI testing is pending.
No cloud builds or installer packaging were requested or performed.

Operator: confirm v0.1.5, check one other shared scene, several other scenes,
and an independent source. The current editing scene should not be listed.
Make an independent copy and verify the other source/settings remain unchanged.
