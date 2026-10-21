# SimpleCam Direct for OBS

**Status:** first Windows implementation pass; not yet built in Windows OBS or hardware-tested.

SimpleCam Direct is an OBS PTZ-control plugin. It is independent of the SimpleCamQt desktop application and the legacy Lua/file-bridge plugin. The initial build targets Windows x64 and OBS Studio 32.2.1, with the AViPAS AV-1280W over VISCA TCP as the first camera path.

The first implementation contains:

- OBS-managed **Camera Control** and **Cameras** docks.
- A non-rendering scene preset source with Program, Preview, and Both trigger choices and an optional delay.
- A shared asynchronous VISCA-over-TCP command service for dock controls and scene preset actions.
- Five initial pages of 14 preset buttons, page cycling through **PG**, virtual button swapping, and persisted names/mappings.
- Packet-construction tests for the documented VISCA actions used by this pass.

The scene source fires when the selected Program or Preview scene changes, preserving the order in which its visible source entries appear in that scene. Delayed recalls wait in the same camera queue, so a later source cannot overtake an earlier delayed request. In Studio Mode off, the single current scene acts as Program, Preview, and Both. On scene collection load, the active scene's preset entries are also processed once.

It does not yet claim camera compatibility or replace the proven SimpleCamQt production workflow. The first test needs a Windows OBS 32.2.1 build and a real AViPAS AV-1280W.

## Build in Visual Studio

1. Extract the project folder under your Visual Studio `repos` directory.
2. In Visual Studio, choose **File → Open → Folder** and open the folder containing this `README.md` and `CMakeLists.txt`.
3. Select the `windows-x64` CMake configure preset and the `Debug` configuration.
4. Build the `simplecamdirect` target. Build and run `simplecamdirect_packet_tests` or run the `windows-debug` CTest preset for packet tests.

The first CMake configure downloads the pinned OBS 32.2.1 sources and OBS build dependencies, then builds the OBS frontend API needed by the docks. That setup takes much longer than an ordinary plugin build, and Visual Studio may appear idle while it runs. Build output is kept under `out/build/windows-x64`; downloads are kept under `.deps` and are not part of the source package.

To stage a local install without writing to the system OBS plugin directory:

```powershell
cmake --install out/build/windows-x64 --prefix out/install --config Debug
```

After closing OBS, copy `out/install/simplecamdirect` to:

```text
%APPDATA%\obs-studio\plugins\simplecamdirect
```

Reopen OBS and check **View → Docks** for Camera Control and Cameras. The plugin has a distinct native source id, `simplecamdirect_preset`; it does not reuse the legacy Lua source id.

## First camera test

In the Cameras dock, enter the camera's IP address and VISCA TCP port (1259 for the user's AV-1280W test setup) and click **Test TCP**. The build does not include a private LAN address. Start with manual dock-button tests while OBS is not streaming, then test a scene preset source with a known safe preset. Preset Set overwrites the selected physical camera preset and should be tested with a slot you are willing to replace. Only test TCP checks connectivity; it sends no VISCA motion command.

This pass intentionally omits serial, ONVIF, NDI, generic profile editing, camera power-off, and multi-camera selection. Advanced focus and white-balance controls are also not included yet. It does not use arbitrary sleeps for VISCA completion; the transport waits asynchronously for the response frames it expects and reports timeouts in the dock and OBS log.

## Project documents

The architecture and coding-agent instructions remain in [`AGENTS.md`](AGENTS.md) and [`docs/`](docs/). The build scaffolding follows the official [OBS plugin template](https://github.com/obsproject/obs-plugintemplate), and the project uses GPL-2.0-or-later in [`LICENSE`](LICENSE).
