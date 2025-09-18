# UEJackAudioLink

Realtime JACK Audio Link plugin for Unreal Engine 5.6

[![License Badge](https://img.shields.io/badge/GNU_GPLv3-blue.svg?style=plastic&logo=gnu&label=license)](https://www.gnu.org/licenses/gpl-3.0.en.html)

Pre-release prototype.

---

## Table of Contents

- [Overview](#overview)
- [Installation](#installation)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Port Name Format (JACK)](#port-name-format-jack)
- [Build Notes](#build-notes)
- [Credits and Support](#credits-and-support)
- [License](#license)
- [Notes](#notes)

## Overview

**UEJackAudioLink** provides a bridge between Unreal Engine 5.6 and the JACK audio server:

- Server control and status (sample rate, buffer size, CPU load)
- JACK client lifecycle (connect/disconnect, port auto-registration)
- Audio I/O: read from JACK inputs, write to JACK outputs (ring buffers)
- Client discovery, port listing, and patching (connect/disconnect ports)
- Blueprint-accessible API and events for game logic

## Installation

### Dependencies

- Unreal Engine 5.6
- JACK2 runtime; headers/libs for building
  - Windows: JACK2 (ASIO), optionally VB-Audio Matrix ASIO for multi-channel
  - macOS: JACK (or BlackHole as a system device) via CoreAudio
  - Linux: JACK (jackd) with sufficient I/O channels

### Windows

1) Install JACK2 for Windows and ensure the ASIO device exposes enough channels.
2) Optionally install VB-Audio Matrix ASIO to aggregate channels.
3) Build your project with this plugin enabled. `WITH_JACK` is auto-detected when headers/libs are found (see Build Notes).

### macOS

1) Install JACK (e.g., via Homebrew) or set up BlackHole as a system device.
2) Ensure your interface exposes required input/output channels.
3) Build your project with the plugin enabled.

### Linux

1) Install JACK and the corresponding development packages.
2) Configure a JACK session with the desired I/O channel counts.
3) Build your project with the plugin enabled.

## Usage

### In Unreal Editor

1) Acquire the engine subsystem:
   - Blueprint: Get Engine Subsystem → `UEJackAudioLinkSubsystem`
   - C++: `GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>()`
2) Start or restart the JACK server with your desired sample rate and buffer size.
3) Connect a JACK client with the number of input/output ports you need.
4) Route JACK ports (either by full names or by index) to your hardware or other clients.
5) Stream audio using `WriteAudioBuffer` and/or read with `ReadAudioBuffer`.

## API Reference

> [!NOTE]
> A proper documentation will be available soon.

### Subsystem: `UUEJackAudioLinkSubsystem` (Engine Subsystem)

- Server/client
  - `RestartServer(SampleRate:int, BufferSize:int) -> bool`
  - `ConnectClient(ClientName:string, NumInputs:int, NumOutputs:int) -> bool`
  - `DisconnectClient()`
  - `IsServerRunning() -> bool`
  - `IsClientConnected() -> bool`
  - `GetSampleRate() -> int`
  - `GetBufferSize() -> int`
  - `GetCpuLoad() -> float (0..100)`
  - `GetJackClientName() -> string` (this plugin's JACK client name)

- Audio I/O
  - `ReadAudioBuffer(Channel:int, NumSamples:int) -> float[]`
  - `WriteAudioBuffer(Channel:int, AudioData: float[]) -> bool`
  - `GetInputLevel(Channel:int) -> float` (RMS approximation)

- Discovery
  - `GetConnectedClients() -> string[]` (unique client names)
  - `GetClientPorts(ClientName:string, out InputPorts:string[], out OutputPorts:string[])`

- Routing (full names)
  - `ConnectPorts(SourceFullName:string, DestFullName:string) -> bool`
  - `DisconnectPorts(SourceFullName:string, DestFullName:string) -> bool`

- Routing (by client + port number)
  - `ConnectPortsByIndex(SourceType:EJackPortDirection, SourceClient:string, SourcePortNumber:int,`
    `DestType:EJackPortDirection,   DestClient:string,   DestPortNumber:int) -> bool`
  - `DisconnectPortsByIndex(SourceType:EJackPortDirection, SourceClient:string, SourcePortNumber:int,`
    `DestType:EJackPortDirection,   DestClient:string,   DestPortNumber:int) -> bool`

- Events
  - `OnNewJackClientConnected(ClientName:string, NumInputPorts:int, NumOutputPorts:int)`
  - `OnJackClientDisconnected(ClientName:string)`

Blueprint function library: `UUEJackAudioLinkBPLibrary` mirrors the same calls as static nodes.

### C++ API Usage

Linking and includes
- In your `.Build.cs`: add `UEJackAudioLink` to your dependency module names.
- Includes you’ll typically need:
  - `#include "UEJackAudioLinkSubsystem.h"`
  - Optional utilities/logs: `#include "UEJackAudioLinkLog.h"`

Getting the subsystem
- Engine subsystem access (game thread):
  - `UUEJackAudioLinkSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>() : nullptr;`
- Always nullptr-check before use.

Core calls (selected)
- Server/client
  - `bool RestartServer(int32 SampleRate, int32 BufferSize);`
  - `bool ConnectClient(const FString& ClientName, int32 NumInputs, int32 NumOutputs);`
  - `void DisconnectClient();`
  - `bool IsServerRunning() const;`
  - `bool IsClientConnected() const;`
  - `int32 GetSampleRate() const;`
  - `int32 GetBufferSize() const;`
  - `float GetCpuLoad() const;`
  - `FString GetJackClientName() const;`
- Discovery
  - `TArray<FString> GetConnectedClients() const;`
  - `void GetClientPorts(const FString& Client, TArray<FString>& OutInputs, TArray<FString>& OutOutputs) const;`
- Routing
  - `bool ConnectPorts(const FString& SourceFullName, const FString& DestFullName);`
  - `bool DisconnectPorts(const FString& SourceFullName, const FString& DestFullName);`
  - `bool ConnectPortsByIndex(EJackPortDirection SrcType, const FString& SrcClient, int32 SrcPortNumber,`
                              `EJackPortDirection DstType, const FString& DstClient, int32 DstPortNumber);`
  - `bool DisconnectPortsByIndex(EJackPortDirection SrcType, const FString& SrcClient, int32 SrcPortNumber,`
                                 `EJackPortDirection DstType, const FString& DstClient, int32 DstPortNumber);`
- Audio I/O
  - `TArray<float> ReadAudioBuffer(int32 Channel, int32 NumSamples) const;`
  - `bool WriteAudioBuffer(int32 Channel, const TArray<float>& AudioData);`

Events in C++
- You can bind to the Blueprint-assignable multicast delegates from C++:
  - `Subsystem->OnNewJackClientConnected.AddDynamic(this, &ThisClass::HandleJackClientConnected);`
  - `Subsystem->OnJackClientDisconnected.AddDynamic(this, &ThisClass::HandleJackClientDisconnected);`
- Handlers must be UFUNCTIONs with matching signatures.

Threading notes
- Call routing, discovery, and server/client methods on the game thread.
- Writing/reading the audio buffers should be done regularly (e.g., in Tick or a timer) to avoid underflows; keep chunks near the JACK buffer size for best latency.


## Port Name Format (JACK)

Full port names are `client_name:port_short_name`. Examples:
- `system:capture_1`, `system:capture_2` (hardware inputs)
- `system:playback_1`, `system:playback_2` (hardware outputs)
- Unreal client ports from this plugin default to short names `unreal_in_X` and `unreal_out_X`.

When connecting:
- Source must be an output port; destination must be an input port.
- Example: `system:capture_1 -> UnrealJackClient-Project:unreal_in_1`
- Example: `UnrealJackClient-Project:unreal_out_1 -> system:playback_1`

## Build Notes
- `WITH_JACK` is auto-enabled if headers/libs are found (`JACK_SDK_ROOT` or default Windows JACK2 path). Otherwise the API compiles but returns defaults.


## Credits and Support

aKM is a project developped within _laplanque_, a non-profit for artistic
creation and the sharing of expertise, bringing together multifaceted artists working in both analog and digital media.

More details: [WEBSITE](https://laplanque.eu/) / [DISCORD](https://discord.gg/c7PK5h3PKE) / [INSTAGRAM](https://www.instagram.com/contact.laplanque/)

- The aKM Project is developped by: Vincent Bergeron, Samy Bérard, Nicolas Désilles
- Software design and development: [Nicolas Désilles](https://github.com/nicolasdesilles). 

- Supporters: La Région AURA, GRAME CNCM, la Métropole de Lyon, Hangar Computer Club, and private contributors

![logos-partenaires](https://laplanque.eu/wp-content/uploads/2025/07/LOGO-partenaire-siteweb.png)

## License

GNU GPL‑3.0. See `LICENSE`.

## Notes
- Sample rate mismatches between UE and JACK are not automatically resampled by this plugin.
- Keep `WriteAudioBuffer` chunk sizes near the JACK buffer size for best latency.

