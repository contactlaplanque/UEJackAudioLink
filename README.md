# UEJackAudioLink

A JACK Audio Link plugin for Unreal Engine 5.6 providing:
- Server control and status (sample rate, buffer size, CPU load)
- JACK client lifecycle (connect/disconnect, port auto-registration)
- Audio I/O: read from JACK inputs, write to JACK outputs (ring buffers)
- Client discovery, port listing, and patching (connect/disconnect ports)
- Blueprint-accessible API and events for game logic

## Blueprint API surface

Subsystem: `UUEJackAudioLinkSubsystem` (Engine Subsystem)

- Server/client
  - RestartServer(SampleRate:int, BufferSize:int) -> bool
  - ConnectClient(ClientName:string, NumInputs:int, NumOutputs:int) -> bool
  - DisconnectClient()
  - IsServerRunning() -> bool
  - IsClientConnected() -> bool
  - GetSampleRate() -> int
  - GetBufferSize() -> int
  - GetCpuLoad() -> float (0..100)
  - GetJackClientName() -> string (this plugin's JACK client name)

- Audio I/O
  - ReadAudioBuffer(Channel:int, NumSamples:int) -> float[]
  - WriteAudioBuffer(Channel:int, AudioData: float[]) -> bool
  - GetInputLevel(Channel:int) -> float (RMS approximation)

- Discovery
  - GetConnectedClients() -> string[] (unique client names)
  - GetClientPorts(ClientName:string, out InputPorts:string[], out OutputPorts:string[])

- Routing (full names)
  - ConnectPorts(SourceFullName:string, DestFullName:string) -> bool
  - DisconnectPorts(SourceFullName:string, DestFullName:string) -> bool

- Routing (by client + port number)
  - ConnectPortsByIndex(SourceType:EJackPortDirection, SourceClient:string, SourcePortNumber:int,
                        DestType:EJackPortDirection,   DestClient:string,   DestPortNumber:int) -> bool
  - DisconnectPortsByIndex(SourceType:EJackPortDirection, SourceClient:string, SourcePortNumber:int,
                           DestType:EJackPortDirection,   DestClient:string,   DestPortNumber:int) -> bool

- Events
  - OnNewJackClientConnected(ClientName:string, NumInputPorts:int, NumOutputPorts:int)
  - OnJackClientDisconnected(ClientName:string)

Blueprint function library: `UUEJackAudioLinkBPLibrary` mirrors the same calls as static nodes.

## C++ API usage

Linking and includes
- In your .Build.cs: add "UEJackAudioLink" to your dependency module names.
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
  - `bool ConnectPortsByIndex(EJackPortDirection SrcType, const FString& SrcClient, int32 SrcPortNumber,
                              EJackPortDirection DstType, const FString& DstClient, int32 DstPortNumber);`
  - `bool DisconnectPortsByIndex(EJackPortDirection SrcType, const FString& SrcClient, int32 SrcPortNumber,
                                 EJackPortDirection DstType, const FString& DstClient, int32 DstPortNumber);`
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

## Port name format (JACK)

Full port names are "client_name:port_short_name". Examples:
- system:capture_1, system:capture_2 (hardware inputs)
- system:playback_1, system:playback_2 (hardware outputs)
- Unreal client ports from this plugin default to short names "unreal_in_X" and "unreal_out_X".

When connecting:
- Source must be an output port; destination must be an input port.
- Example: system:capture_1 -> UnrealJackClient-Project:unreal_in_1
- Example: UnrealJackClient-Project:unreal_out_1 -> system:playback_1

## Testing WriteAudioBuffer (sending audio to JACK)

WriteAudioBuffer fills an internal ring buffer that the JACK process callback reads each audio cycle. To hear output:

1) Ensure your Unreal JACK client has at least one output port registered
   - ConnectClient(ClientName, NumInputs, NumOutputs) with NumOutputs >= 1
   - Confirm with GetClientPorts(GetJackClientName(), Inputs, Outputs) and connect
     Outputs[0] -> system:playback_1 (and Outputs[1] -> system:playback_2 for stereo)

2) Feed audio samples each tick (or with a timer)
   - Typical sample format: float in [-1..1]
   - Buffer size: you can push chunks matching JACK buffer size, or any multiple; the ring buffer will stream them out

3) Simple Blueprint test (sine beep)
   - Get Sample Rate (SR)
   - Keep a phase accumulator (float) in your actor
   - On Tick (DeltaSeconds), generate N samples at 440Hz:
     - N = round(SR * DeltaSeconds) (clamp 64..4096 to avoid too-small/large bursts)
     - For i in 0..N-1: sample = sin(2*pi*440 * (phase + i)/SR)
     - Build a float array; call WriteAudioBuffer(Channel=0, AudioData=array)
     - Update phase += N; wrap phase by SR to avoid growth
   - Connect your Unreal out_1 to system:playback_1 and listen

4) Simple C++ test snippet (inside a tick component)
   - Query SR via subsystem
   - Generate a small buffer (e.g., 256 samples) per tick and call WriteAudioBuffer(0, Buffer)

Notes:
- If you underflow (not writing fast enough), output will be silence for missing samples; logs may show xruns depending on system.
- To stream an audio file:
  - Use UE’s `USoundWave` to PCM decode or `Audio::Transcoder` in runtime audio mixer (platform dependent)
  - Retrieve PCM float samples (convert 16-bit to float if needed)
  - Write chunks sequentially via WriteAudioBuffer for each output channel
- Channel mapping:
  - Channel index 0 -> unreal_out_1, 1 -> unreal_out_2, etc.

## Troubleshooting
- No sound:
  - Verify ConnectClient succeeded and output ports exist
  - Connect plugin output ports to system:playback_X
  - Confirm CPU load not pegged and buffer size sane
- Distorted/stuttering:
  - Ensure steady writes near the JACK callback cadence (use smaller chunks, e.g., 128–512 samples)
  - Watch the log for xruns and CPU load spikes
- Port by index fails:
  - Use GetConnectedClients/GetClientPorts to confirm available ports, and pass 1-based port numbers

## Build notes
- WITH_JACK is auto-enabled if headers/libs are found (JACK_SDK_ROOT or default Windows JACK2 path). Otherwise the API compiles but returns defaults.

## How to play sound by writing to the audio buffers from UE

This pattern shows how to tap a UE submix in real time and forward its audio to JACK outputs using this plugin. It’s minimal and does not cover connection/patching, only getting buffers out of a submix and into the plugin’s ring buffers.

Key idea
- Use UE 5.6’s ISubmixBufferListener on the Audio Mixer to receive interleaved float audio from a chosen `USoundSubmix`.
- Keep the listener as a lightweight, non-UObject C++ class owned by your Actor as a `TSharedPtr`/`TSharedRef` (thread-safe), and forward buffers back to the Actor for processing.
- In the Actor, deinterleave per channel and call `UUEJackAudioLinkSubsystem::WriteAudioBuffer(Channel, Samples)`.

Minimal flow
1) Actor setup (BeginPlay)
  - Get the subsystem: `GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>()`.
  - Ensure a JACK client is connected with enough output ports (e.g., stereo `NumOutputs = 2`).
  - Create a `TSharedRef` listener object implementing `ISubmixBufferListener`.
  - Get the active `Audio::FMixerDevice` and register: `RegisterSubmixBufferListener(ListenerRef, *SourceSubmix)`.

2) Listener callback (audio render thread)
  - `OnNewSubmixBuffer(const USoundSubmix*, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)`
  - Immediately forward to the Actor: `Owner->ProcessAudioBuffer(...)`.

3) Actor processing
  - Compute `NumFrames = NumSamples / NumChannels`.
  - For each channel `c` up to the number of JACK outputs:
    - Deinterleave: copy samples `AudioData[c + i*NumChannels]` for `i ∈ [0..NumFrames-1]` to a temporary `TArray<float>`.
    - Optionally apply gain.
    - Call `Subsystem->WriteAudioBuffer(c, ChannelBuffer)`.

4) Cleanup (EndPlay)
  - `UnregisterSubmixBufferListener(ListenerRef, *SourceSubmix)` and release the shared pointer.

Notes and tips
- UE submix buffers are interleaved; JACK plugin outputs are per-channel ring buffers, so deinterleave before writing.
- Threading: the callback runs on the audio render thread—avoid heavy work; preallocate/reuse scratch buffers where possible.
- Sample rate: if the submix SR differs from JACK SR, you’ll get a mismatch; this simple pattern does not resample.
- Channel mapping: UE channel 0 → plugin `unreal_out_1`, channel 1 → `unreal_out_2`, etc. Ensure the JACK client was created with at least that many outputs.
- Safety: never hold `AudioData` beyond the callback; copy what you need right away.

This approach keeps your gameplay logic in the Actor while using a dedicated listener to integrate safely with the Audio Mixer in UE 5.6.
