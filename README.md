# SimpleCam Direct for OBS

**Status:** Windows OBS 32.2.1 build and AViPAS AV-1280W VISCA/TCP workflow tested. Camera Control layout checkpoint verified by the maintainer on October 22, 2026.

SimpleCam Direct is an OBS PTZ-control plugin. It is independent of the SimpleCamQt desktop application and the legacy Lua/file-bridge plugin. The initial build targets Windows x64 and OBS Studio 32.2.1, with the AViPAS AV-1280W over VISCA TCP as the first camera path.

The first implementation contains:

- OBS-managed **Camera Control** and **Cameras** docks, with a Dual toggle for two independent AViPAS camera cards.
- A non-rendering scene preset source with Program, Preview, and Both trigger choices and an optional delay.
- A shared asynchronous VISCA-over-TCP command service for dock controls and scene preset actions.
- Five initial pages of 14 preset buttons per camera, page cycling in Settings, virtual button swapping, and persisted names/mappings.
- Packet-construction tests for the documented VISCA actions used by this pass.

The scene source fires when the selected Program or Preview scene changes, preserving the order in which its visible source entries appear in that scene. Delayed recalls wait in the same camera queue, so a later source cannot overtake an earlier delayed request. In Studio Mode off, the single current scene acts as Program, Preview, and Both. On scene collection load, the active scene's preset entries are also processed once.

SimpleCam Direct is still an early implementation and does not replace the proven SimpleCamQt production workflow. The AViPAS AV-1280W path has been exercised on the maintainer's Windows OBS setup. A second camera slot and dual physical-camera operation remain separate validation items.

## Build in Visual Studio

1. Extract the project folder under your Visual Studio `repos` directory.
2. In Visual Studio, choose **File → Open → Folder** and open the folder containing this `README.md` and `CMakeLists.txt`.
3. Select the `windows-x64` CMake configure preset and the `RelWithDebInfo` configuration when building a plugin to load into the installed OBS application. Use `Debug` only when debugging against matching Debug OBS/Qt runtime DLLs.
4. Build the `simplecamdirect` target. Build and run `simplecamdirect_packet_tests` or run the `windows-debug` CTest preset for packet tests.

The first CMake configure downloads the pinned OBS 32.2.1 sources and OBS build dependencies, then builds the OBS frontend API needed by the docks. That setup takes much longer than an ordinary plugin build, and Visual Studio may appear idle while it runs. Build output is kept under `out/build/windows-x64`; downloads are kept under `.deps` and are not part of the source package.

To stage a local install without writing to the system OBS plugin directory:

```powershell
cmake --install out/build/windows-x64 --prefix out/install --config RelWithDebInfo
```

After closing OBS, copy `out/install/simplecamdirect` to:

```text
%APPDATA%\obs-studio\plugins\simplecamdirect
```

Reopen OBS and check **View → Docks** for Camera Control and Cameras. The plugin has a distinct native source id, `simplecamdirect_preset`; it does not reuse the legacy Lua source id.

If OBS logs `LoadLibrary failed` with Windows error 126, Windows found the plugin DLL but could not find one of its required DLL dependencies. A Debug build can depend on Debug Qt DLLs (with a `d` suffix) that are not shipped with the normal OBS application. Rebuild and install `RelWithDebInfo`, then fully restart OBS.

## First camera test

In the Cameras dock, select Camera 1 or Camera 2, enter that camera's IP address and VISCA TCP port (1259 for the user's AV-1280W test setup), and click **Test TCP**. Camera 1 keeps the original `simplecamdirect.ini` settings. Camera 2 uses a separate `simplecamdirect-camera2.ini` file and starts with a blank IP address, so it cannot accidentally send commands to Camera 1. Use **Dual** in Camera Control to show both independent cards; **Single** returns to one card.

Both slots currently use the same AViPAS AV-1280W VISCA/TCP implementation. Camera 2 is included for independent settings and the dual-card layout; dual physical-camera operation has not been hardware-tested. Scene preset sources can select either camera. Serial, ONVIF, NDI, and the generic profile editor are not implemented yet. Camera power, manual focus, focus movement, BackLight, and Exp Comp controls are included. The VISCA transport waits asynchronously for replies and does not use arbitrary sleeps for completion.

## Project documents

The architecture and coding-agent instructions remain in [`AGENTS.md`](AGENTS.md) and [`docs/`](docs/). The build scaffolding follows the official [OBS plugin template](https://github.com/obsproject/obs-plugintemplate), and the project uses GPL-2.0-or-later in [`LICENSE`](LICENSE).
