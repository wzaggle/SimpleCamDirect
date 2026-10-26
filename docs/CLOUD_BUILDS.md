# One source revision, three native builds

## Current setup

GitHub Actions workflow **Build All Platforms** compiles the same exact commit
on Windows 2022 x64, Apple Silicon macOS 15, and Ubuntu 24.04 x64. It runs the
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

Local source/JSON/YAML checks and Python syntax checks are recorded during setup.
Cloud build results must be read from the actual Actions run, not inferred from
valid workflow syntax. Initial Windows/Mac/Linux build and test results are
pending until that run completes. OBS runtime/hardware checks remain manual.
