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
