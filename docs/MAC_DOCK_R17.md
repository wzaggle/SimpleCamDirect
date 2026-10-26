# Mac docking investigation — r17

## Observed baseline

The maintainer installed the first arm64 package. OBS could not discover its
system-Library payload. Copying simplecamptz.plugin to the user's
~/Library/Application Support/obs-studio/plugins/ fixed discovery. The M1 then
loaded the plugin and displayed the controls, but dragging did not produce a
docking highlight even with sufficient room. Camera testing remains incomplete.

OBS 32.2.1's add_dock_by_id creates a normal OBSDock parented to OBSBasic and
explicitly starts it floating. The plugin did not replace window flags, set
NoDockWidgetAreas, or call winId(). It does impose floating maximum sizes on
the wrapper. Qt documents native-title-bar limitations and recommends content
size constraints, so these remain hypotheses to test rather than proven causes.

## This diagnostic build

- macOS-only source adapter, attached to the existing OBS-owned wrapper; no
  duplicate registration, custom docking engine or global styling.
- At the first FINISHED_LOADING of r17, after saved layout restoration, try
  docking into OBS and show the dock. On success persist ui/macDockR17Placed;
  subsequent launches leave the saved layout alone. Respect Lock Docks.
- Release the wrapper's floating maximum sizes by default on Mac only. The
  existing Windows floating limits and responsive card sizing are unchanged.
- Tools → SimpleCamPTZ Mac Dock Diagnostics:
  - **Dock into OBS**: request docking using public Qt calls.
  - **Release floating size limits**: checked initially; uncheck to compare
    with the previous floating maximum dimensions.
  - **Use Qt title bar**: unchecked initially; check to bypass the native Mac
    title bar. Qt handles dragging; close/float controls follow dock features.
  - **Log dock state**: snapshot without creating a native handle.
- Logs use `[SimpleCamPTZ MacDockDiag] build=r17`. Include parent/main-window
  relationship, flags, allowed areas, feature/lock bits, floating state, bounds,
  title-bar mode and mouse/modifier state. Drag logs are limited to 10/second;
  the passive filter does not consume events. No camera addresses/labels logged.
- All queued callbacks have QObject lifetime contexts and QPointer guards. The
  adapter is removed before the dock during module unload.

## M1 operator test

1. Close OBS, run the new PKG, reopen OBS with Docks → Lock Docks unchecked.
2. Check whether the dock starts inside OBS. Undock it and try redocking.
3. If dragging fails, choose **Dock into OBS**. Does this succeed?
4. Leave **Release floating size limits** checked. Enable **Use Qt title bar**,
   then try dragging out/in. Do not hold Command during the drag.
5. If dragging works, compare once with floating limits enabled by unchecking
   **Release floating size limits**; record which setting changes the result.
6. Return to the working combination. Restart and verify saved layout, then
   close/reopen OBS to check shutdown. Send the current OBS log with MacDockDiag.

The diagnostic toggles intentionally reset on restart. Only the first successful
r17 dock placement is persisted. Do not claim a permanent docking fix until
native M1 testing establishes which change is required.

## Mac installer and cloud workflow

The PKG is now a product package restricted to CurrentUserHomeDirectory. Its
payload maps to ~/Library/Application Support/obs-studio/plugins, not /Library.
Bundle relocation is disabled so an older system copy cannot redirect an upgrade.
CI runs the actual user-domain installer, compares the installed/staged binary
hashes and checks the installed signature. No production INIs are packaged.

Actions → Build All Platforms → Run workflow now has **platform** choices:
all, macos (Apple Silicon), windows, linux. A single-platform run uploads only
packages-<platform>; the combined collector runs only for all. The current
ci/push-platform.txt value macos scopes automatic infrastructure validation to
this Mac investigation. Set it to all for shared infrastructure validation.
The optional PowerShell helper accepts -Platform macos (default remains all).

Intel Mac support is not added by this change. The maintainer ordered an i7
Mac mini. The matching x86_64 OBS/Qt dependencies and an Intel Actions runner
are available; add that build/test target separately after the M1 docking test.

## Evidence before cloud build

- Python build/selector syntax passes; eight selection/invalid-input checks pass.
- New adapter and existing dock/entry code syntax-check against Qt 6.4.2 and
  official OBS 32.2.1 headers with GCC C++17 and warnings treated as errors.
- Actual macOS compile, installer execution and docking tests remain pending
  until their respective CI/operator checks complete.

References:
- https://doc.qt.io/qt-6/qdockwidget.html
- https://developer.apple.com/library/archive/documentation/DeveloperTools/Reference/DistributionDefinitionRef/Chapters/Distribution_XML_Ref.html
- https://www.swift.org/install/macos/package_installer/

## Cloud and local results

Run [40393283631](https://github.com/wzaggle/SimpleCamDirect/actions/runs/40393283631)
passed for aa7055ee9c19251596dac83ea25ec88d4b42c5fe. Exactly one native
job ran (macos); no Windows/Linux compile jobs were scheduled. The combined
collector correctly skipped this single-platform run.

- Native Apple Silicon compile and all four automated suites passed.
- Product package creation, user-domain installer execution, staged/installed
  binary hash comparison, and installed bundle signature verification passed.
- Installer log explicitly reports base path /Users/runner; the verified bundle
  is /Users/runner/Library/Application Support/obs-studio/plugins/simplecamptz.plugin.
- Separate local Qt offscreen title-bar smoke: seven checks, zero failures
  (float/redock/close/lock). This does not test native Cocoa dragging.
- Common/non-Mac entry and dock source paths were compared and are unchanged.
- M1 drag and saved-layout acceptance remain pending. This is a diagnostic build,
  not a claimed fix for the drag issue.

[Mac package](https://github.com/wzaggle/SimpleCamDirect/actions/runs/40393283631/artifacts/12685942914)
expires 2026-11-02; download for local retention.
