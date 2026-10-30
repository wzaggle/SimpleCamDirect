# Lowest unused preset suffix — v0.1.8

New preset sources needing a number and Make independent copy choose the lowest
unused suffix for the same generated preset-request name, starting at (2).
For existing (2), (4), (7), the next numbered identity is (3). A source can use the
unsuffixed name when that name is free. Independent-copy actions always number.
No existing sequence is compacted. Multiple references to one source reserve one
identity. Plain and generated orange names both reserve numbers, including legacy
OBS suffixes after </span> and escaped characters inside the wrapper.

Implementation changes:
- src/preset_name_allocator.hpp: bounded reservations and markup normalization.
- src/preset_source.cpp: both creation paths use reservations; saved settings
  retain allocated numbers; loaded sources preserve stored/legacy numbers.
  Global sharing refreshes never allocate. Copy naming uses captured edited
  camera/preset settings rather than the restored original's settings.
- tests/preset_name_allocator_test.cpp and CMakeLists.txt: allocator checks/CTest.
- buildspec.json and installer.iss: version 0.1.8.
- AGENTS.md, PLATFORM_TEST_CHECKLIST.md and USER_GUIDE_PLAN.md: context/status.

The overlay is cumulative over the supplied v0.1.7 ZIP, not a complete checkout.
GitHub main was read at a21b536d363aff4fd99b1a055642e1ad11734d4b; its older files
were not substituted for the attached local revisions. No GitHub writes/cloud jobs.
Entries use current America/Denver local timestamps for the MSBuild workflow.

## Evidence and limits

GCC C++17 with -Wall -Wextra -Werror -pedantic: allocator target compiles;
27 checks pass. Existing standalone ON-AIR policy target: 29 checks pass.
The environment lacks Qt/OBS development dependencies and CMake; full plugin
compile, real-libobs integration suites, native Windows build, OBS load and
operator acceptance were not run. No camera protocol/action code changed.

Windows is the first operator test gate. After successful Windows validation,
the maintainer tests the Mac M1 and Ubuntu builds from the full multiplatform
project. This source overlay is for the existing local Windows checkout.

## Local VS / OBS acceptance

1. Close OBS. Extract this overlay into the existing project root, replacing
   files. Build simplecamptz in RelWithDebInfo and install using the normal local
   VS workflow. Confirm preset_source.cpp recompiles and the dock shows v0.1.8.
2. For one request name, leave identities (2), (4), (7); create a new preset
   source that needs a number. Verify (3), then (5) on the next creation.
3. Repeat with Make independent copy. Verify the original retains its number
   and other references/settings; only the captured scene item is replaced.
4. Make the occupied (3) source shared/orange. A new identity must skip (3).
   Repeat with a legacy suffix outside the orange span if one exists.
5. Delete a numbered source's last reference and create another: its gap is
   reusable. Removing only one shared reference must not free its number.
6. Save/restart OBS: existing numbers remain. Create another identity and verify
   it fills the next gap. Test a second camera/preset family independently.
7. Check ordinary Properties Cancel, plain Properties title, orange source cue,
   the shared-section width and accepted ON-AIR indication as a brief regression.
