#include "JackInterface.h"
#include "HAL/PlatformProcess.h"
#include "UEJackAudioLinkLog.h"

FJackInterface& FJackInterface::Get()
{
    static FJackInterface Singleton;
    return Singleton;
}

FJackInterface::~FJackInterface()
{
    StopServer();
}

bool FJackInterface::IsServerRunning()
{
    return JackProcHandle.IsValid() && FPlatformProcess::IsProcRunning(JackProcHandle);
}

bool FJackInterface::StartServer(int32 InSampleRate, int32 InBufferSize)
{
    if (IsServerRunning() && InSampleRate == CurrentSampleRate && InBufferSize == CurrentBufferSize)
    {
        UE_LOG(LogJackAudioLink, Verbose, TEXT("JACK server already running with requested settings (%d Hz, %d)"), InSampleRate, InBufferSize);
        return true;
    }

    CurrentSampleRate = InSampleRate;
    CurrentBufferSize = InBufferSize;

    StopServer();

#if PLATFORM_WINDOWS
    const FString Executable = TEXT("jackd.exe");
#else
    const FString Executable = TEXT("jackd");
#endif

    const FString Params = FString::Printf(TEXT("-r -d dummy -r %d -p %d"), InSampleRate, InBufferSize);
    UE_LOG(LogJackAudioLink, Display, TEXT("Starting JACK server: %s %s"), *Executable, *Params);

    JackProcHandle = FPlatformProcess::CreateProc(*Executable, *Params, true, false, false, nullptr, 0, nullptr, nullptr);
    if (!JackProcHandle.IsValid())
    {
        UE_LOG(LogJackAudioLink, Error, TEXT("Failed to launch jackd executable"));
        return false;
    }
    return true;
}

bool FJackInterface::StopServer()
{
    if (!IsServerRunning())
    {
        return false;
    }

    FPlatformProcess::TerminateProc(JackProcHandle, true);
    JackProcHandle.Reset();
    return true;
}

bool FJackInterface::RestartServer(int32 InSampleRate, int32 InBufferSize)
{
    StopServer();
    return StartServer(InSampleRate, InBufferSize);
}

FString FJackInterface::GetVersion() const
{
#if PLATFORM_WINDOWS
    const FString Executable = TEXT("jackd.exe");
#elif PLATFORM_MAC || PLATFORM_LINUX
    const FString Executable = TEXT("jackd");
#else
    const FString Executable = TEXT("jackd");
#endif

    FString StdOut, StdErr;
    int32 ReturnCode = 0;

    FPlatformProcess::ExecProcess(*Executable, TEXT(" --version"), &ReturnCode, &StdOut, &StdErr);

    if (ReturnCode == 0 && !StdOut.IsEmpty())
    {
        StdOut.TrimStartAndEndInline();
        return StdOut;
    }

    return FString(TEXT("Unknown"));
}

void FJackInterface::RegisterClient(const FString& InClientName, int32 InInputChannels, int32 InOutputChannels)
{
    UE_LOG(LogJackAudioLink, Display, TEXT("[STUB] Registering JACK client '%s' (%d in, %d out)"), *InClientName, InInputChannels, InOutputChannels);
} 