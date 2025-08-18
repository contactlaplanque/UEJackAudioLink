#include "JackServerController.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/Paths.h"
#include "UEJackAudioLinkLog.h"
#if WITH_JACK
#include <jack/jack.h>
#endif

FJackServerController& FJackServerController::Get()
{
	static FJackServerController Singleton;
	return Singleton;
}

FJackServerController::~FJackServerController()
{
	StopServer();
}

FString FJackServerController::GetVersion() const
{
	FString Executable = ResolveJackdExecutable();
	if (Executable.IsEmpty())
	{
		return TEXT("Unknown");
	}

	FString StdOut, StdErr;
	int32 ReturnCode = 0;
	FPlatformProcess::ExecProcess(*Executable, TEXT(" --version"), &ReturnCode, &StdOut, &StdErr);
	if (ReturnCode == 0 && !StdOut.IsEmpty())
	{
		StdOut.TrimStartAndEndInline();
		return StdOut;
	}
	return TEXT("Unknown");
}

bool FJackServerController::IsServerRunning()
{
	return JackProcHandle.IsValid() && FPlatformProcess::IsProcRunning(JackProcHandle);
}

bool FJackServerController::IsAnyServerAvailable() const
{
#if WITH_JACK
	jack_status_t Status = JackServerFailed;
	jack_client_t* Test = jack_client_open("ue_probe", JackNoStartServer, &Status);
	if (Test)
	{
		jack_client_close(Test);
		return true;
	}
	return (Status & JackServerStarted) != 0;
#else
	return false;
#endif
}

bool FJackServerController::StartServer(int32 SampleRate, int32 BufferSize, const FString& DriverOverride, const FString& ExecutableOverride)
{
	if (IsServerRunning())
	{
		return true;
	}

	const FString Executable = ResolveJackdExecutable(ExecutableOverride);
	if (Executable.IsEmpty())
	{
		UE_LOG(LogJackAudioLink, Error, TEXT("JACK executable not found. Cannot start server."));
		return false;
	}

	const FString Params = BuildServerParams(SampleRate, BufferSize, DriverOverride);
	UE_LOG(LogJackAudioLink, Display, TEXT("Starting JACK server: %s %s"), *Executable, *Params);
	JackProcHandle = FPlatformProcess::CreateProc(*Executable, *Params, true, false, false, nullptr, 0, nullptr, nullptr);
	if (!JackProcHandle.IsValid())
	{
		UE_LOG(LogJackAudioLink, Error, TEXT("Failed to launch jackd"));
		return false;
	}
	bServerOwnedByPlugin = true;
	LastDriver = DriverOverride;
	LastSampleRate = SampleRate;
	LastBufferSize = BufferSize;
	return true;
}

bool FJackServerController::StopServer()
{
	if (!IsServerRunning())
	{
		return false;
	}
	FPlatformProcess::TerminateProc(JackProcHandle, true);
	JackProcHandle.Reset();
	bServerOwnedByPlugin = false;
	return true;
}

bool FJackServerController::RestartServer(int32 SampleRate, int32 BufferSize, const FString& DriverOverride, const FString& ExecutableOverride)
{
	StopServer();
	return StartServer(SampleRate, BufferSize, DriverOverride, ExecutableOverride);
}

FString FJackServerController::ResolveJackdExecutable(const FString& ExecutableOverride) const
{
#if PLATFORM_WINDOWS
	if (!ExecutableOverride.IsEmpty())
	{
		return ExecutableOverride;
	}
#else
	if (!ExecutableOverride.IsEmpty())
	{
		return ExecutableOverride;
	}
#endif
#if PLATFORM_WINDOWS
	// Common install path for JACK2 on Windows
	const FString DefaultPath = TEXT("C:/Program Files/JACK2/jackd.exe");
	if (FPaths::FileExists(DefaultPath))
	{
		return DefaultPath;
	}
	return TEXT("jackd.exe");
#else
	return TEXT("jackd");
#endif
}

FString FJackServerController::BuildServerParams(int32 SampleRate, int32 BufferSize, const FString& DriverOverride) const
{
	FString Driver;
#if PLATFORM_WINDOWS
	Driver = DriverOverride.IsEmpty() ? TEXT("portaudio") : DriverOverride;
	return FString::Printf(TEXT("-S -X winmme -d %s -r %d -p %d"), *Driver, SampleRate, BufferSize);
#elif PLATFORM_MAC
	Driver = DriverOverride.IsEmpty() ? TEXT("coreaudio") : DriverOverride;
	return FString::Printf(TEXT("-d %s -r %d -p %d"), *Driver, SampleRate, BufferSize);
#else
	Driver = DriverOverride.IsEmpty() ? TEXT("alsa") : DriverOverride;
	return FString::Printf(TEXT("-d %s -r %d -p %d"), *Driver, SampleRate, BufferSize);
#endif
}

bool FJackServerController::GetServerAudioConfig(int32& OutSampleRate, int32& OutBufferSize) const
{
#if WITH_JACK
	jack_status_t Status = JackServerFailed;
	jack_client_t* Test = jack_client_open("ue_probe_cfg", JackNoStartServer, &Status);
	if (!Test)
	{
		return false;
	}
	OutSampleRate = static_cast<int32>(jack_get_sample_rate(Test));
	OutBufferSize = static_cast<int32>(jack_get_buffer_size(Test));
	jack_client_close(Test);
	return true;
#else
	return false;
#endif
}


