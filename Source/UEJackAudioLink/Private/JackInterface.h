#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

/**
 * Minimal helper to start/stop a JACK audio server by spawning the external `jackd` process.
 * NOTE: This is a very lightweight implementation meant for development/testing only.
 */
class FJackInterface
{
public:
    /** Singleton accessor */
    static FJackInterface& Get();

    /** Returns true if the JACK server process we launched is still running */
    bool IsServerRunning();

    /** Attempts to start JACK server (dummy backend by default). Returns true on success. */
    bool StartServer(int32 InSampleRate, int32 InBufferSize);

    /** Stops the JACK server if it was started by this plugin */
    bool StopServer();

    /** Convenience helper: stop then start. */
    bool RestartServer(int32 InSampleRate, int32 InBufferSize);

    /** Registers a JACK client; stub for now */
    void RegisterClient(const FString& InClientName, int32 InInputChannels, int32 InOutputChannels);

    /** Returns the version string returned by `jackd --version`, or "Unknown" if not found. */
    FString GetVersion() const;

private:
    FJackInterface() = default;
    ~FJackInterface();

    /** Non-copyable */
    FJackInterface(const FJackInterface&) = delete;
    FJackInterface& operator=(const FJackInterface&) = delete;

    /** Handle to the spawned JACK process */
    FProcHandle JackProcHandle;
    int32 CurrentSampleRate = 0;
    int32 CurrentBufferSize = 0;
}; 