# One source revision, three native builds

## Current setup

GitHub Actions workflow **Build All Platforms** compiles the same exact commit
on Windows 2022 x64, Apple Silicon macOS 26 (Xcode 26.5), and Ubuntu 24.04 x64. It runs the
packet, profile, runtime-state, and localhost camera-concurrency tests before
packaging each platform. No physical cameras or production settings are used.

The final downloadable artifact is named
`SimpleCamPTZ-0.1.0-<revision>-AllPlatforms`. It contains three directories:

- `Windows`: Setup EXE, portable plugin tree, instructions, build metadata/checksums.
- `macOS`: Apple Silicon PKG and bundle tar.gz, instructions, metadata/checksums.
- `Linux`: native Ubuntu 24.04 amd64 DEB, instructions, metadata/checksums.

All three packages must report the same source commit and passed automated tests
before the combined artifact is produced. If a platform fails, successful
platforms keep their individual artifacts, but there is no misleading combined
complete set. OBS load, UI, and camera tests are separate acceptance gates.

## Running from GitHub

1. Open the private repository `wzaggle/SimpleCamDirect`.
2. Choose **Actions → Build All Platforms → Run workflow**.
3. Leave source revision blank to build the selected branch's current source.
   Alternatively supply a committed source SHA from this repository.
4. Open the run and wait for its jobs to finish.
5. Under Artifacts, download the artifact ending **AllPlatforms** and extract it.

Normal source pushes do not automatically run all three builds. Changes to the
workflow or `ci/` build scripts trigger a validation build. This initial setup
therefore attempts one validation build when committed. There is no schedule,
public release, automatic installer launch, or production deployment.

## Running from Visual Studio on Windows

Your existing **RelWithDebInfo Install** remains the fast local Windows action.
The added **build-all-platforms** target starts the cloud workflow and downloads
the combined artifact to a unique `installers/` subdirectory in the source tree.
It needs a Git working copy, GitHub CLI, and your one-time CLI authentication:

```powershell
winget install --id GitHub.cli -e
gh auth login
```

Use a Git clone of the repository for this target. A folder extracted from a ZIP
can still build/install locally and can use the GitHub website workflow button,
but it cannot identify/push local edits to the remote repository. Commit and push
changes before a cloud build; the script rejects uncommitted edits and revisions
that are absent from GitHub. It does not silently commit or push your work.

To download and then open the Windows installer wizard after all builds pass:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-all-platforms.ps1 -InstallWindows
```

Close OBS first. Without that switch, the script only downloads the packages.
The source revision, not just the unchanged v0.1.0 version number, distinguishes
iterations. Packages from different commits cannot be mixed by the collector.

## Platform requirements and limitations

- Windows builds retain the existing pinned OBS/Qt dependency bootstrap and
  Inno Setup installer. Debug Qt DLLs are never packaged. The OBS installation
  supplies its matching Qt libraries, including Qt6Network.
- macOS builds the OBS 32.2.1 SDK against checksum-verified OBS arm64 dependencies,
  using Xcode and an arm64-only plugin. Minimum target macOS 13. The plugin is
  ad-hoc signed for testing, not Developer ID signed or notarized. The first M1
  OBS 32 install/load and real-camera checks remain required. Intel Mac support
  is not claimed. Do not disable Gatekeeper system-wide to test a package.
- Linux builds only the pinned OBS 32.2.1 SDK/API targets, with native Ubuntu
  libraries; it does not rebuild or replace the installed OBS application.
  The DEB targets native Ubuntu 24.04 OBS 32+, not Flatpak or Snap. No generic
  compatibility claim is made for every Linux distribution or OBS package.
- Model profiles, source IDs/settings fields, TCP port/address handling and
  per-camera preset persistence remain shared across platforms. Runtime tests
  on one OS do not certify another OS.
- Native SDK build tools, dependency versions and checksums live in the same
  repository. Update `ci/dependencies.json` deliberately with the OBS target;
  preserve Windows `buildspec.json` compatibility. macOS local presets expect
  the SDK prepared by `python3 ci/native_build.py --platform macos`.

## Cost controls

- Manual day-to-day builds; only infrastructure edits trigger a validation run.
- Seven-day artifact retention; dependency-download caches are separate.
- Read-only workflow token permissions, native jobs bounded to 45 minutes.
- No account payment method, billing budget, repository visibility or signing
  secret is changed by this setup. Configure your GitHub Actions budget in the
  account's Billing settings if you want a financial stop limit.

GitHub reference: https://docs.github.com/en/billing/concepts/product-billing/github-actions
OBS build-template reference: https://github.com/obsproject/obs-plugintemplate

## Properties cleanup included with this setup

The r14 deferred field identification and accepted 280px baseline are retained.
Temporary `PropertiesDiag` snapshots, resize-event tracing and settle timers are
removed. The adapter still retries identification briefly, resizes only a matched
SimpleCam preset dialog once, and leaves normal manual resizing available.
No camera setting INIs, recovered labels or Windows registry contents are shipped.

## Evidence

Native cloud validation completed successfully on 2026-10-26 for source
`e0963690927afaea393f9aa985bbd9a6482e5a34`:

- Windows x64: plugin compile, four automated suites and EXE installer passed.
- macOS arm64: plugin compile, four suites, ad-hoc signature verification,
  arm64 architecture check, tar archive and PKG creation passed.
- Ubuntu 24.04 x64: plugin compile, four suites and DEB creation passed.
- Collector verified all three BUILD-INFO files use that exact revision and
  report automated tests passed, then created the combined artifact.

[Successful run](https://github.com/wzaggle/SimpleCamDirect/actions/runs/40365409463)

[Combined Windows/macOS/Linux ZIP](https://github.com/wzaggle/SimpleCamDirect/actions/runs/40365409463/artifacts/12670800797)

Artifacts expire after seven days (this run: 2026-11-02); download them for
local retention. The macOS folder contains
`SimpleCamPTZ-Setup-0.1.0-macOS-arm64.pkg` and a portable tar.gz alternative.
The M1 OBS-load, modifier behavior and real-camera checks remain manual and
unverified. The package is ad-hoc signed for testing, not notarized. These build
results do not establish Mac/Linux runtime or camera compatibility.

## r17 platform selection and Mac installation

Run workflow now accepts platform all/macos/windows/linux; macos means Apple
Silicon. Single-platform jobs upload packages-<platform> and skip the combined
collector. The PowerShell helper accepts -Platform macos. Automatic CI pushes
currently read macos from ci/push-platform.txt for the scoped Mac investigation.
The Mac PKG is now restricted to the user's home domain and CI verifies an
actual installation at the path OBS scans. The earlier all-users description
above is superseded. See MAC_DOCK_R17.md for test procedure and current status.

## r18 shared-runtime validation

ci/push-platform.txt is restored to all for the shared startup-zoom change.
Run 40402713949 passed all three platforms, their tests, installers and combined
revision check. The earlier r17 Mac-only automatic selection is superseded for
this shared change; manual platform choices still work. See STARTUP_ZOOM_R18.md.

## r19 Linux-only diagnostic build

ci/push-platform.txt selects linux for the floating-dock investigation. Download
packages-linux from this run; no new Windows/Mac binaries are requested. Shared
changes must restore all. See LINUX_DOCK_R19.md for acceptance procedure.

## r20 shared Program-output correction

Automatic selection is all again. A fifth native suite,
simplecamptz_program_output_tests, exercises OBS scene copies and transition
source selection. It does not access cameras or initialize video rendering.
Preserve the existing four suites and platform installers.

r20 final run 40622959759 passed all five native suites per platform and all
packaging/collection steps. Combined artifact 12788730201 (source b6a4acbed)
contains Windows, macOS arm64 and Linux; expires 2026-11-04. A passing functional
suite does not certify antivirus safety or physical-camera/OBS UI acceptance.
