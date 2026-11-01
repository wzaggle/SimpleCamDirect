# SimpleCamPTZ 0.2.0 — four-platform test release

This promotes the v0.1.14 source baseline to 0.2.0. Runtime behavior remains
unchanged: profile-owned transports/tuning, independent Camera Profiler, suffix
gap filling, Rename save-on-next and blank-name recall protection are included.
Lyrics2.lua is a separate script and is not included in the plugin installers.

Native packaging targets:

| Machine | Installer | Artifact folder |
|---|---|---|
| Windows x64 | Setup EXE | Windows |
| Intel Mac | x86_64 PKG | macOS-Intel |
| M1 Mac | arm64 PKG | macOS-AppleSilicon |
| Ubuntu 24.04 x64, native OBS | amd64 DEB | Linux |

Both Mac builds target macOS 13+, install in the current user's OBS plugins
directory, disable bundle relocation, and verify architecture and installed
binary identity. Test bundles are ad-hoc signed, not notarized. Use the package
matching OBS's architecture; an Intel OBS process under Rosetta needs x86_64.
Linux packages do not target Flatpak or Snap OBS. OBS SDK remains pinned at 32.2.1.

The Intel job uses macos-26-intel with the x86_64 OBS dependency archives, whose
SHA256 values were checked against the official obs-deps 2026-07-15 release API.
Apple Silicon retains macos-26. Both select Xcode 26.5 as required by the existing
SDK pipeline. CI now explicitly builds all registered native test executables,
including the Sony UDP, Profiler and preset-editing suites added since v0.1.8.

The combined installer artifact requires all four jobs to pass and all four
BUILD-INFO files to identify the same source commit. Native compile/test and
installer results must be read from that run; this document does not claim
they have completed. Local hardware acceptance remains separate.

Close OBS before installing. Back up existing OBS settings first. On each
machine verify plugin load/version, saved camera profiles/catalogs, dock behavior,
TCP/Sony UDP camera control, named/blank preset behavior, scene-triggered recalls,
ON-AIR indication and shutdown. Record Intel and M1 results separately.

This request authorizes synchronizing the current source and running all four
native installer jobs. It does not authorize a public release or deployment to
production machines. Production currently remains on v0.1.8.
