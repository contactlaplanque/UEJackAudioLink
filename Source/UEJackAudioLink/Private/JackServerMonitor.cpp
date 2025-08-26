#include "JackServerMonitor.h"
#include "Containers/Ticker.h"
#include "Async/Async.h"
#include "UEJackAudioLinkLog.h"
#include "Misc/App.h"

#if WITH_JACK
#include <jack/jack.h>
#endif

FJackServerMonitor& FJackServerMonitor::Get()
{
	static FJackServerMonitor Singleton;
	return Singleton;
}

FJackServerMonitor::~FJackServerMonitor()
{
	Stop();
}

void FJackServerMonitor::Start()
{
#if WITH_JACK
	bIsActive = true;
	// Attempt to open sentinel immediately
	OpenSentinel();
	// Start 1s ticker to probe when disconnected
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime)
		{
			return Tick(DeltaTime);
		}), 1.0f);
	}
#endif
}

void FJackServerMonitor::Stop()
{
	bIsActive = false;
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
#if WITH_JACK
	CloseSentinel();
#endif
}

FJackServerState FJackServerMonitor::GetState() const
{
	FScopeLock Lock(&StateMutex);
	return State;
}

bool FJackServerMonitor::Tick(float /*DeltaTime*/)
{
#if WITH_JACK
	if (!bIsActive)
	{
		return false;
	}
	// If sentinel missing, probe once per tick
	if (!SentinelClient)
	{
		if (ProbeServerAvailable())
		{
			OpenSentinel();
		}
	}
#endif
	return true; // keep ticking
}

#if WITH_JACK
void FJackServerMonitor::RequestImmediateProbe()
{
	if (!bIsActive)
	{
		return;
	}
	if (!SentinelClient && ProbeServerAvailable())
	{
		OpenSentinel();
	}
}

void FJackServerMonitor::MarkServerDown()
{
	{
		FScopeLock Lock(&StateMutex);
		State.bServerAvailable = false;
		State.SampleRate = 0;
		State.BufferSize = 0;
	}
	CloseSentinel();
}

bool FJackServerMonitor::OpenSentinel()
{
	if (!bIsActive)
	{
		return false;
	}
	if (SentinelClient)
	{
		return true;
	}
	jack_status_t Status = JackServerFailed;
	const FString MonitorName = FString::Printf(TEXT("UEJackMonitor-%s"), FApp::GetProjectName());
	jack_client_t* Client = jack_client_open(TCHAR_TO_ANSI(*MonitorName), JackNoStartServer, &Status);
	if (!Client)
	{
		// No server
		{
			FScopeLock Lock(&StateMutex);
			State.bServerAvailable = false;
			State.SampleRate = 0;
			State.BufferSize = 0;
		}
		return false;
	}
	SentinelClient = reinterpret_cast<_jack_client*>(Client);
	// Callbacks
	jack_on_shutdown(Client, &FJackServerMonitor::StaticShutdownCallback, this);
	jack_set_sample_rate_callback(Client, &FJackServerMonitor::StaticSampleRateCallback, this);
	jack_set_buffer_size_callback(Client, &FJackServerMonitor::StaticBufferSizeCallback, this);

	// Ensure callbacks are reliably delivered by activating the sentinel client
	jack_activate(Client);

	UpdateSRBSFromSentinel();
	{
		FScopeLock Lock(&StateMutex);
		State.bServerAvailable = true;
	}
	UE_LOG(LogJackAudioLink, VeryVerbose, TEXT("Sentinel connected; SR=%d, BS=%d"), State.SampleRate, State.BufferSize);
	return true;
}

void FJackServerMonitor::CloseSentinel()
{
	if (SentinelClient)
	{
		jack_client_close(reinterpret_cast<jack_client_t*>(SentinelClient));
		SentinelClient = nullptr;
	}
}

bool FJackServerMonitor::ProbeServerAvailable() const
{
	jack_status_t Status = JackServerFailed;
	const FString ProbeName = FString::Printf(TEXT("UEJackProbe-%s"), FApp::GetProjectName());
	jack_client_t* Probe = jack_client_open(TCHAR_TO_ANSI(*ProbeName), JackNoStartServer, &Status);
	if (Probe)
	{
		jack_client_close(Probe);
		return true;
	}
	return false;
}

void FJackServerMonitor::UpdateSRBSFromSentinel()
{
	if (!SentinelClient)
	{
		return;
	}
	jack_client_t* Client = reinterpret_cast<jack_client_t*>(SentinelClient);
	int32 SR = static_cast<int32>(jack_get_sample_rate(Client));
	int32 BS = static_cast<int32>(jack_get_buffer_size(Client));
	FScopeLock Lock(&StateMutex);
	State.SampleRate = SR;
	State.BufferSize = BS;
}

/* static */ void FJackServerMonitor::StaticShutdownCallback(void* Arg)
{
	FJackServerMonitor* Self = static_cast<FJackServerMonitor*>(Arg);
	if (!Self) return;
	// Update state and schedule close and re-probe on game thread
	AsyncTask(ENamedThreads::GameThread, [Self]()
	{
		if (!Self->bIsActive)
		{
			return;
		}
		{
			FScopeLock Lock(&Self->StateMutex);
			Self->State.bServerAvailable = false;
			Self->State.SampleRate = 0;
			Self->State.BufferSize = 0;
		}
		// Avoid closing the JACK client from inside the shutdown callback path.
		// JACK is already tearing down the client; calling jack_client_close here can crash.
		Self->SentinelClient = nullptr;
		UE_LOG(LogJackAudioLink, Display, TEXT("JACK server shutdown detected by sentinel"));
		// quick delayed re-probe to pick up external restarts fast
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float){ if (Self->bIsActive) { Self->RequestImmediateProbe(); } return false; }), 0.25f);
	});
}

/* static */ int FJackServerMonitor::StaticSampleRateCallback(unsigned int NewRate, void* Arg)
{
	FJackServerMonitor* Self = static_cast<FJackServerMonitor*>(Arg);
	if (!Self) return 0;
	AsyncTask(ENamedThreads::GameThread, [Self, NewRate]()
	{
		if (!Self->bIsActive) { return; }
		Self->OnSampleRateChanged(static_cast<int32>(NewRate));
	});
	return 0;
}

/* static */ int FJackServerMonitor::StaticBufferSizeCallback(unsigned int NewFrames, void* Arg)
{
	FJackServerMonitor* Self = static_cast<FJackServerMonitor*>(Arg);
	if (!Self) return 0;
	AsyncTask(ENamedThreads::GameThread, [Self, NewFrames]()
	{
		if (!Self->bIsActive) { return; }
		Self->OnBufferSizeChanged(static_cast<int32>(NewFrames));
	});
	return 0;
}

// OnServerShutdown removed (unused)

void FJackServerMonitor::OnSampleRateChanged(int32 NewSR)
{
	FScopeLock Lock(&StateMutex);
	State.SampleRate = NewSR;
}

void FJackServerMonitor::OnBufferSizeChanged(int32 NewBS)
{
	FScopeLock Lock(&StateMutex);
	State.BufferSize = NewBS;
}
#endif // WITH_JACK


