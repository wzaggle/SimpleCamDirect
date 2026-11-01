## Windows widget platform

Windows native compilation and nine CTest suites pass, including all 18 UDP
cases. Both QApplication suites time out before output. Use the shipped Windows
Qt platform plugin for those fixtures instead of forcing offscreen; retain
offscreen on Mac/Linux. Enable console plugin diagnostics and suppress modal
Windows loader error dialogs in the CI parent process. Preserve test timeouts.

The new test-only callbacks explicitly include obs.h and cstdint; the local
compatibility preinclude had masked the missing OBS type declaration. Native
rebuild required.

Windows native linking also requires test-only stubs for the full docks Program
scene/generation callbacks; the isolated preset card fixture never uses them.
Application symbols and behavior are unchanged. Linux native build/tests and
DEB packaging passed on the previous commit.

## Native test harness corrections

Mac compilation passes after the JSON-loop fix. Native tests require an explicit
127.0.0.2 loopback alias and the SDK-built frontend dylib in test Frameworks.
Use a 1500 ms response deadline only in the multi-action simulated-camera test
to tolerate scheduling on shared runners; timeout/priority fixtures retain their
short deadlines. Runtime profile defaults are unchanged.

# v0.2.0 native validation

The first four-platform run exposed Apple Clang range-loop reference warnings
in five QJsonArray iterations. Materialize QJsonValue copies explicitly; keep
strict warnings enabled. Native rebuild results remain pending.

# Native build validation notes

## 2026-10-26 initial setup

- Use the published CMake 3.31.10 wheels on all runners.
- Match QT_NO_KEYWORDS in the service tests with the plugin target.
- Select macOS 26 / Xcode 26.5 for the pinned OBS source SDK.
- Add the SDK Frameworks prefix for macOS libobs package discovery.
- Remove an unused profile-tool local variable for the Linux warning gate.
- Keep assert fixtures enabled in RelWithDebInfo.
- Include release dependency DLL directories for standalone Windows service tests.
- Windows and Ubuntu compiled and passed all four suites in run 40356815569.
- Clang requires value iteration over QJsonArray entries (rather than references
  to converted proxy temporaries). Apply this source-compatible parser correction
  before producing the final same-revision combined package. JSON data and
  serialized command bytes are unchanged. Mac OBS/hardware testing remains pending.

## 2026-10-26 — accepted r16 and macOS package validation

- Save maintainer-accepted group clear/delete behavior and its regression checks.
- Run 40358713833: all platforms compiled and passed 4/4 suites; Windows and
  Linux packages succeeded. Mac code signing passed; lipo architecture checking
  then failed because the input filename followed the variadic -verify_arch list.
- Put the input filename first, preserving the arm64 validation itself.
- Rebuild all platforms for the same new source revision. M1 OBS-load and
  real-camera validation remain separate from build/test/package success.

## 2026-10-26 — r17 Mac docking/installer investigation

- Add isolated macOS dock diagnostics and optional Qt title bar, preserving
  Windows code paths and the accepted camera/preset behavior.
- Correct the Mac installer to a nonrelocatable current-user product package.
  Verify real user-domain installation and signed binary identity in CI.
- Add native platform choice; scope this infrastructure validation push to Mac.
- No Intel build or permanent Mac docking success is claimed at this checkpoint.

## 2026-10-26 — r18 startup zoom initialization

- Validate the shared runtime on all three native platforms again.
- Extend the existing concurrency suite with startup inquiries and guarded
  refresh after camera enable/connection/profile changes. Local 34 checks pass.
- Preserve r17 Mac user-domain installation checks and the platform selector.
