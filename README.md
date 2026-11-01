# SimpleCamPTZ v0.2.1 experimental HTTP testing release

Adds HTTP transport with Panasonic AW handling and a separate draft AW-HE130 profile. No real AW-HE130 hardware tests yet. See [Panasonic testing guide](docs/PANASONIC_HTTP_021.md) for first tests, limitations and feedback. Existing VISCA methods remain available; v0.2.0 remains published as the earlier baseline. Build and hardware validation are separate evidence.

The earlier release and build background follows; current behavior is established by source and the v0.2.1 guide.

---

# SimpleCamPTZ v0.2.0 — complete source project

Open this folder in Visual Studio and build/install **windows-x64 / RelWithDebInfo**
with OBS closed. Confirm dock version **v0.2.0**. A fresh CMake configure downloads
the pinned build dependencies; Debug is unsuitable for normal OBS installations.

Preset Rename now saves edits when you select the next button. Blank or
whitespace-only names block recalls from both the dock and OBS sources; Set and
Rename remain available. Delayed recalls also recheck the name and mapping
before transmission. The Profiler has one receive-port checkbox caption.

See [docs/PRESET_EDITING_014.md](docs/PRESET_EDITING_014.md) for behavior and local
acceptance checks. Automated widget and localhost checks pass; native Windows,
OBS/hardware and Mac M1/Ubuntu acceptance remain pending.
Four-platform installer builds are authorized for this test release.
See docs/RELEASE_020.md for architecture-specific packages and acceptance.

The earlier README follows for build background; older slot/port/Profile Test
statements are superseded by the current layout document.

---

# SimpleCamPTZ for OBS

**Release candidate:** Windows x64, OBS Studio 32.2.1, AViPAS AV-1280W over raw VISCA/TCP. This is intended for production-equipment testing; it is not yet a general multi-camera or cross-platform release.

SimpleCamPTZ is an OBS PTZ-control plugin. It is independent of the SimpleCamQt desktop application and the legacy Lua/file-bridge plugin. The first release targets Windows x64 and OBS Studio 32.2.1, using the AViPAS AV-1280W over VISCA TCP.

The first implementation contains:

- An OBS **Camera Control** dock with two cards; each card can select any of the five configured camera slots, and the two visible cards always control different slots.
- **Tools → SimpleCamPTZ Cameras**, where each slot selects a discovered profile and stores its own camera name, IP address, VISCA TCP port, and VISCA address. The selected profile's command templates drive Camera Control and scene preset sources.
- A non-rendering scene preset source with Program, Preview, and Both trigger choices and an optional delay.
- A shared asynchronous VISCA-over-TCP command service for dock controls and scene preset actions.
- An optional per-camera mapping to its raw OBS video input. Camera Control marks the highest visible mapped feed in the live Program scene with a dark-red number circle; missing/unconfigured feeds remain unmarked.
- A five-second Camera Control status message after a successful OBS preset recall, such as “Camera 1 moved to Stage Left by OBS.” A later status or preset move replaces it.
- Five initial pages of 14 preset buttons per camera, page cycling in Settings, virtual button swapping, and persisted names/mappings.
- A separate **Tools → SimpleCamPTZ Profile Test** window for editing/rendering command templates, sending individual commands or **Test All** over raw TCP, viewing replies, and discovering the camera's current zoom endpoint. Save custom profiles in the user profile folder, then use **Refresh** in Cameras to load them.
- Packet-construction and profile-template tests.

The scene source fires when the selected Program or Preview scene changes, preserving the order in which its visible source entries appear in that scene. Delayed recalls wait in the same camera queue, so a later source cannot overtake an earlier delayed request. In Studio Mode off, the single current scene acts as Program, Preview, and Both. On scene collection load, the active scene's preset entries are also processed once.

The built-in **AViPAS AV-1280W — Confirmed (VISCA/TCP)** profile reflects testing on the maintainer's AV-1280W unit. Firmware and hardware revision were not recorded, so this confirmation applies to that tested unit and must not be generalized to every AV-1280/1281 revision. Digital zoom behavior was inconclusive on that unit; preset recall speed is unsupported. The first field-test installer is Windows x64 only.

## Build in Visual Studio

1. Extract the project folder under your Visual Studio `repos` directory.
2. In Visual Studio, choose **File → Open → Folder** and open the folder containing this `README.md` and `CMakeLists.txt`.
3. Select the `windows-x64` CMake configure preset and the `RelWithDebInfo` configuration when building a plugin to load into the installed OBS application. Use `Debug` only when debugging against matching Debug OBS/Qt runtime DLLs.
4. Build the `simplecamptz` target. Build and run `simplecamptz_profile_tests` and `simplecamptz_packet_tests`, or run CTest, to check profile token rendering and packet fixtures.

The first CMake configure downloads the pinned OBS 32.2.1 sources and OBS build dependencies, then builds the OBS frontend API needed by the docks. That setup takes much longer than an ordinary plugin build, and Visual Studio may appear idle while it runs. The renamed project uses a fresh build directory, `out/build/simplecamptz-windows-x64`, to avoid stale SimpleCam Direct targets; downloads are kept under `.deps` and are not part of the source package.

### Build the Windows installer

Build the `simplecamptz` target in **RelWithDebInfo**, then build the `package-simplecamptz-installer` target. This stages the plugin and compiles `dist/SimpleCamPTZ-Setup-0.1.0-x64.exe` with Inno Setup. Alternatively, from PowerShell in the project directory run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-installer.ps1 -BuildDir out/build/simplecamptz-windows-x64 -Configuration RelWithDebInfo
```

The script requires CMake and Inno Setup. Run the installer on the production PC after closing OBS; it installs the x64 plugin under `%ProgramData%\obs-studio\plugins\simplecamptz` and removes the previous SimpleCam Direct plugin package to prevent both modules registering the same OBS source type. It leaves OBS scenes and settings files intact. The OBS source type remains `simplecamdirect_preset` so saved scene sources continue to resolve.

To stage a local install without writing to the system OBS plugin directory:

```powershell
cmake --install out/build/simplecamptz-windows-x64 --prefix out/install --config RelWithDebInfo
```

After closing OBS, copy `out/install/simplecamptz` to:

```text
%APPDATA%\obs-studio\plugins\simplecamptz
```

Reopen OBS and check **View → Docks** for Camera Control. Open profile and connection settings from **Tools → SimpleCamPTZ Cameras**. The plugin keeps its native source id, `simplecamdirect_preset`; it does not reuse the legacy Lua source id.

If OBS logs `LoadLibrary failed` with Windows error 126, Windows found the plugin DLL but could not find one of its required DLL dependencies. A Debug build can depend on Debug Qt DLLs (with a `d` suffix) that are not shipped with the normal OBS application. Rebuild and install `RelWithDebInfo`, then fully restart OBS.

## First camera test

Open **Tools → SimpleCamPTZ Cameras**, select Camera 1–5, optionally map **Raw camera feed** to that camera's OBS video input, choose its profile, enter its IP address and VISCA TCP port (1259 for the AV-1280W test setup), then click **Test TCP connection**. The feed mapping is used only for the live stack marker; it does not affect camera commands. Existing Camera 1 `simplecamdirect.ini` and other slot settings are retained during the module rename. Cameras 2–5 each use a separate settings file and start with a blank IP address. Use **Dual** in Camera Control to show two different slots at once; either card can select any of the five. **Single** returns to one card.

All five slots start with the AViPAS AV-1280W VISCA/TCP profile, and each can select another compatible raw VISCA/TCP profile. The five-slot selection and independent connection settings need Windows/OBS testing. Multiple physical cameras and switching between different hardware profiles have not been hardware-tested. Preset group names and visual mappings remain shared between the two cards in this first pass, while each profile's preset capacity is enforced per camera. Serial, UDP, ONVIF, NDI, and the general profile editor are not implemented yet.

The Profile Test window is a standalone development harness. Camera Control and OBS preset sources now render and send their commands from the selected profile. The runtime currently accepts raw VISCA/TCP profiles only; serial and UDP choices remain disabled. A test opens its own TCP connection, so avoid controlling the same camera from another panel or its web page during a test. Each action has a compact single-height row; multi-packet command strings are separated with semicolons. **Test All** begins with Power Inquiry, then sends every command in an explicit dependency-aware order, continues after failures, and marks each row PASS or FAIL with the response summary in its tooltip and raw TX/RX details in the trace. Manual focus precedes near/far, manual white balance precedes auto, Store precedes Recall, Clear runs after Recall, and Power Off runs last. A profile may add a separately documented preset-speed command; the AV-1280 profile currently marks that capability unsupported. Test All has one confirmation because the sweep can move the camera, alter settings, clear/store the selected preset number, and put the camera into standby. Before each individual command test, the tool queries camera power; if the camera reports standby, it sends Power On and repeats the inquiry until the camera reports On before sending the requested command. If power state cannot be verified, the requested command is skipped. Individual preset store/clear and camera standby commands in this test harness remain confirmation-gated. In the Camera Control dock, Set overwrites the selected camera preset immediately without a confirmation dialog and flashes the button to acknowledge the Set action. Zoom discovery moves continuously toward telephoto, polls until the value stabilizes, stops, records the endpoint in the in-memory profile draft, and restores the starting position when the profile has an absolute zoom command. Use **Save As** to write the draft as a separate `.camera.cfg` or JSON file. Camera-side digital zoom configuration is not changed by the tool.

Camera power, manual focus, focus movement, BackLight, and Exp Comp controls remain included. The existing VISCA transport waits asynchronously for replies and does not use arbitrary sleeps for completion.

## Project documents

The architecture and coding-agent instructions remain in [`AGENTS.md`](AGENTS.md) and [`docs/`](docs/). The build scaffolding follows the official [OBS plugin template](https://github.com/obsproject/obs-plugintemplate), and the project uses GPL-2.0-or-later in [`LICENSE`](LICENSE).

## Current accepted Controls Dock checkpoint

See [tested checkpoints](docs/TESTED_CHECKPOINTS.md) for the 2026-10-24 r9
MARK IT baseline: responsive centered one/two cards, compact preset heights,
floating-only size limits, rightmost black Settings gear, and yellow on-air
camera titles. This supersedes earlier fixed-size layout descriptions.
Windows/OBS passes are maintainer-reported; this editing environment did not
run Windows builds. Product version remains 0.1.0.

## Windows, Apple Silicon macOS and Ubuntu cloud packages

See [docs/CLOUD_BUILDS.md](docs/CLOUD_BUILDS.md). The **Build All Platforms**
GitHub Actions workflow packages one exact commit into Windows/macOS/Linux
folders after native builds and automated tests. It does not deploy production
or publish a release. Local Windows Install is unchanged. Mac and Linux need
separate OBS/runtime acceptance tests; CI is not a hardware support claim.

## Apple Silicon diagnostic build (r17)

The first M1 package loaded after a manual user-directory copy but did not
redock. The next diagnostic corrects the package destination and tests docking
without changing the accepted Windows behavior. See [MAC_DOCK_R17](docs/MAC_DOCK_R17.md).
Actions now offers a platform choice for Mac-only builds. Intel testing is planned.
