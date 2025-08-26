#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

class FJackServerController
{
public:
	static FJackServerController& Get();

	// Basic info
	FString GetVersion() const;
	bool IsServerRunning();
	// Returns true if any JACK server is available (even if not started by this plugin)
	bool IsAnyServerAvailable() const;

	// Lifecycle
	bool StartServer(int32 SampleRate, int32 BufferSize, const FString& DriverOverride = FString(), const FString& ExecutableOverride = FString());
	bool StopServer();
	bool RestartServer(int32 SampleRate, int32 BufferSize, const FString& DriverOverride = FString(), const FString& ExecutableOverride = FString());

	// Force-stop any JACK server (including external); Windows implementation uses taskkill
	bool StopAnyServer() const;

	// Ownership info removed

	// Query current server audio configuration (sample rate and buffer size). Returns false if unavailable.
	bool GetServerAudioConfig(int32& OutSampleRate, int32& OutBufferSize) const;
	
private:
	FJackServerController() = default;
	~FJackServerController();

	FString ResolveJackdExecutable(const FString& ExecutableOverride = FString()) const;
	FString BuildServerParams(int32 SampleRate, int32 BufferSize, const FString& DriverOverride) const;

	FProcHandle JackProcHandle;
};


