#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Containers/CircularBuffer.h"

#if WITH_JACK
#include <jack/jack.h>
#include <jack/types.h>
#endif

// Audio ring buffer for thread-safe audio I/O
class FAudioRingBuffer
{
public:
    FAudioRingBuffer(int32 InCapacity = 8192) : Capacity(InCapacity), WritePos(0), ReadPos(0)
    {
        Buffer.SetNumZeroed(Capacity);
    }

    void Write(const float* Data, int32 NumSamples);
    int32 Read(float* OutData, int32 NumSamples);
    void Clear();
    int32 GetAvailableRead() const;
    float GetRMSLevel() const; // For debug level monitoring

private:
    TArray<float> Buffer;
    int32 Capacity;
    std::atomic<int32> WritePos;
    std::atomic<int32> ReadPos;
    mutable FCriticalSection CriticalSection;
};

class FJackClientManager
{
public:
	static FJackClientManager& Get();

	bool Connect(const FString& ClientName);
	void Disconnect();
	bool IsConnected() const;

	bool Activate();
	bool Deactivate();

	bool RegisterAudioPorts(int32 NumInputs, int32 NumOutputs, const FString& BaseName);
	void UnregisterAllPorts();

	TArray<FString> GetAvailablePorts(const FString& NamePattern = TEXT(""), const FString& TypePattern = TEXT(""), uint32 Flags = 0) const;
	bool ConnectPorts(const FString& SourcePort, const FString& DestinationPort);
	bool DisconnectPorts(const FString& SourcePort, const FString& DestinationPort);

	// Discovery helpers
	TArray<FString> GetAllClients() const;
	TArray<FString> GetInputPortNames() const;
	TArray<FString> GetClientOutputPorts(const FString& ClientName) const;

	// Auto-connect helper
	void AutoConnectToClient(const FString& ClientName);

	// Audio I/O methods
	TArray<float> ReadAudioBuffer(int32 ChannelIndex, int32 NumSamples);
	bool WriteAudioBuffer(int32 ChannelIndex, const TArray<float>& AudioData);
	float GetInputLevel(int32 ChannelIndex) const; // For debug monitoring

	// Info
	FString GetClientName() const;
	uint32 GetSampleRate() const;
	uint32 GetBufferSize() const;

private:
	FJackClientManager();
	~FJackClientManager();

	// JACK callbacks
	static void PortRegistrationCallback(unsigned int PortId, int Register, void* Arg);
	static int ProcessCallback(jack_nframes_t NumFrames, void* Arg);

	jack_client_t* JackClient = nullptr;
	TArray<jack_port_t*> InputPorts;
	TArray<jack_port_t*> OutputPorts;
	TSet<FString> KnownClientsLogged;

	// Audio ring buffers for thread-safe I/O
	TArray<TUniquePtr<FAudioRingBuffer>> InputRingBuffers;
	TArray<TUniquePtr<FAudioRingBuffer>> OutputRingBuffers;
};