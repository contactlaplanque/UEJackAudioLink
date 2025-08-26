#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

struct FJackServerState
{
	bool bServerAvailable = false;
	int32 SampleRate = 0;
	int32 BufferSize = 0;
};

/**
 * Hybrid JACK server monitor (Windows-focused):
 * - Keeps a lightweight sentinel client open when server is available
 * - Receives shutdown/SR/BS callbacks for instant updates
 * - When disconnected, probes once per second to reconnect sentinel
 */
class FJackServerMonitor
{
public:
	static FJackServerMonitor& Get();

	void Start();
	void Stop();

	// Try to connect sentinel immediately if server is available (one-shot)
	void RequestImmediateProbe();

	// Mark server as down and close sentinel (used after we stop server)
	void MarkServerDown();

	// Thread-safe snapshot of state
	FJackServerState GetState() const;

private:
	FJackServerMonitor() = default;
	~FJackServerMonitor();
	FJackServerMonitor(const FJackServerMonitor&) = delete;
	FJackServerMonitor& operator=(const FJackServerMonitor&) = delete;

	bool OpenSentinel();
	void CloseSentinel();
	bool ProbeServerAvailable() const;
	void UpdateSRBSFromSentinel();

	bool Tick(float DeltaTime);

	// OnServerShutdown removed (unused)
	void OnSampleRateChanged(int32 NewSR);
	void OnBufferSizeChanged(int32 NewBS);

private:
	mutable FCriticalSection StateMutex;
	FJackServerState State;

	FTSTicker::FDelegateHandle TickHandle;
	bool bIsActive = false;

#if WITH_JACK
	struct _jack_client; 
	_jack_client* SentinelClient = nullptr; // opaque jack_client_t

	static void StaticShutdownCallback(void* Arg);
	static int StaticSampleRateCallback(unsigned int NewRate, void* Arg);
	static int StaticBufferSizeCallback(unsigned int NewFrames, void* Arg);
#endif
};


