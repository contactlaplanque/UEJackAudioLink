# UEJackAudioLink – Getting Started

This guide helps you set up JACK, build the plugin, and use it in the Unreal Editor. It also covers common troubleshooting tips.

## 1) Prerequisites

- Unreal Engine 5.x installed and working
- JACK Audio installed on your machine
  - Windows: Install JACK2 (includes `jackd.exe`)
  - macOS: `brew install jack`
  - Linux: Install your distro package (e.g., `sudo apt install jackd2 qjackctl`)
- Optional but recommended: QJackCtl to visualize server/clients and connections

Verify JACK installation:

```bash
jackd --version
```

If the command prints a version, JACK is installed and on your PATH.

References: JACK API docs [jackaudio.org/api](https://jackaudio.org/api/index.html)

## 2) Building the plugin

The plugin supports building with or without the JACK SDK headers/libs available. For full functionality in-editor, enable JACK at build time.

### Option A: Build with local JACK SDK (recommended for development)

1) Set an environment variable pointing to your JACK SDK install root:

   - Windows (PowerShell):
   ```powershell
   $env:JACK_SDK_ROOT="C:\Program Files\JACK2"
   ```

   - macOS/Linux (bash/zsh):
   ```bash
   export JACK_SDK_ROOT=/usr/local     # or wherever include/lib live
   ```

2) Build your project or the plugin as usual (Editor or UAT). The build script detects `JACK_SDK_ROOT` and sets `WITH_JACK=1`, adding include/lib paths and linking `jack`.

### Option B: Build without local SDK (status only)

If `JACK_SDK_ROOT` is not set, the plugin builds with `WITH_JACK=0`. The Editor tab will still load and show diagnostic text, but client operations are disabled.

### Optional: Clang compile database for better IDE linting

Enable “Generate clang compilation database” in the Editor, or run a quick build so `compile_commands.json` appears at your project root. This lets Cursor/clangd resolve Unreal and generated headers.

## 3) Installing the plugin

Place the `UEJackAudioLink` plugin folder inside your project’s `Plugins/` directory. Open the project in the Unreal Editor; the plugin should appear under Plugins → Project.

For automated packaging, you can use:

```powershell
# Windows example (from UE root or via RunUAT)
RunUAT BuildPlugin -Plugin="<PathToProject>\Plugins\UEJackAudioLink\UEJackAudioLink.uplugin" -Package="<OutputDir>" -TargetPlatforms=Win64
```

## 4) Configuring the plugin

In Editor: Project Settings → Plugins → Jack Audio Link

- Server
  - Sample Rate (Hz) – desired JACK sample rate
  - Buffer Size (frames) – desired block size
  - Auto Start Server – start JACK on Editor load
  - Backend Driver – optional override (Windows: `portaudio`, macOS: `coreaudio`, Linux: `alsa`)
  - Jackd Path – optional explicit path to `jackd` (e.g., `C:/Program Files/JACK2/jackd.exe`)
- Client
  - Client Name – name for Unreal’s JACK client
  - Input Channels / Output Channels – how many ports to create
- Auto‑Connect
  - Auto‑connect new clients – automatically connect new client outputs to Unreal inputs
  - Client Monitor Interval – polling interval used as a fallback
- Shutdown
  - Kill server on shutdown – stop JACK only if the plugin started it

## 5) Using the Editor UI

Open Window → UEJackAudioLink (or it opens automatically in Editor). The panel shows:

- Server status and JACK version
- Client status and name
- Audio config (sample rate, buffer size) when available
- Backend (if the plugin started the server)
- Restart required banner if current JACK SR/BS don’t match settings
- Buttons: Apply & Restart, Restart, Connect, Disconnect, Open Settings, Auto‑connect toggle

You can also monitor and patch connections in QJackCtl.

## 6) Typical workflows

### Start the server and client automatically

1) Enable Auto Start Server in settings
2) Pick SR/BS, backend driver if needed
3) Relaunch the Editor: the plugin starts `jackd` and connects the client

### Work with an existing external server

1) Start JACK externally (e.g., via QJackCtl)
2) In the Editor panel, Click “Connect Client”
3) If SR/BS differ from settings, you’ll see a banner; click Apply & Restart only if you want the server restarted with your settings

## 7) Troubleshooting

- `jackd` not found:
  - Ensure JACK is installed and `jackd` is on PATH, or set Jackd Path in settings
  - Verify with `jackd --version`

- Editor shows WITH_JACK=0:
  - Set `JACK_SDK_ROOT` before building, or install SDK headers/libs so the build can link `jack`

- No audio config shown:
  - The plugin queries SR/BS from the client if connected; otherwise it probes the server. Ensure the server is running

- Auto‑connect did not patch ports:
  - The plugin connects a 1:1 mapping of new client outputs to Unreal inputs. Ensure you have enough input ports and the external client exposes outputs

References:

- JACK API overview: [jackaudio.org/api](https://jackaudio.org/api/index.html)
- Control API (programmatic server): [jackaudio.org/api/group__ControlAPI.html](https://jackaudio.org/api/group__ControlAPI.html)


