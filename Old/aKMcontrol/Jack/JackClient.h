#pragma once

#include "CoreMinimal.h"
#include "JackTypes.h"
#include <jack/jack.h>
#include <jack/types.h>

/**
 * Jack Client wrapper class
 * Handles connection to Jack server and basic client operations
 */
class AKMCONTROL_API FJackClient
{
public:
	FJackClient();
	~FJackClient();

	// Server management
	bool CheckJackServerRunning();
	bool KillJackServer();
	bool StartJackServer(const FString& JackdPath = TEXT("C:/Program Files/JACK2/jackd.exe"));
	bool StartJackServerWithParameters(const FString& JackdPath, int32 SampleRate, int32 BufferSize, const FString& Driver = TEXT("portaudio"));
	bool EnsureJackServerRunning(const FString& JackdPath = TEXT("C:/Program Files/JACK2/jackd.exe"));

	// Connection management
	bool Connect(const FString& ClientName = TEXT("UnrealJackClient"));
	void Disconnect();
	bool IsConnected() const;

	// Server information
	bool WasServerStarted() const;
	FString GetServerInfo() const;

	// Client information
	FString GetClientName() const;
	uint32 GetSampleRate() const;
	uint32 GetBufferSize() const;
	float GetCPUUsage() const;

	// Port management
	jack_port_t* RegisterPort(const FString& PortName, const FString& PortType, uint32 Flags, uint32 BufferSize = 0);
	bool UnregisterPort(jack_port_t* Port);
	TArray<FString> GetAvailablePorts(const FString& NamePattern = TEXT(""), const FString& TypePattern = TEXT(""), uint32 Flags = 0);

	// Multiple port registration
	bool RegisterAudioPorts(int32 NumInputs = 64, int32 NumOutputs = 64, const FString& BaseName = TEXT("audio"));
	void UnregisterAllPorts();
	TArray<jack_port_t*> GetInputPorts() const;
	TArray<jack_port_t*> GetOutputPorts() const;

	// Client discovery
	TArray<FString> GetAllClients() const;

	FString GetPortFullName(jack_port_t* Port) const;

	// Connection management
	bool ConnectPorts(const FString& SourcePort, const FString& DestinationPort);
	bool DisconnectPorts(const FString& SourcePort, const FString& DestinationPort);

	// Callback management
	bool SetProcessCallback(JackProcessCallback ProcessCallback, void* Arg);
	bool SetShutdownCallback(JackShutdownCallback ShutdownCallback, void* Arg);
	bool SetXRunCallback(JackXRunCallback XRunCallback, void* Arg);

	// Activation
	bool Activate();
	bool Deactivate();

	// Performance metrics
	FJackPerformanceMetrics GetPerformanceMetrics() const;

	// Getters
	jack_client_t* GetClient() const { return JackClient; }
	EJackConnectionStatus GetConnectionStatus() const { return ConnectionStatus; }

	// Audio level monitoring
	float GetInputLevel(int32 ChannelIndex) const;

private:
	jack_client_t* JackClient;
	EJackConnectionStatus ConnectionStatus;
	FJackPerformanceMetrics PerformanceMetrics;
	bool bServerWasStarted;

	// Port management
	TArray<jack_port_t*> InputPorts;
	TArray<jack_port_t*> OutputPorts;

	// Audio levels (updated in the realtime callback)
	TArray<float> InputLevels;

	// Callback handlers
	static int ProcessCallback(jack_nframes_t Nframes, void* Arg);
	static void ShutdownCallback(void* Arg);
	static int XRunCallback(void* Arg);

	// Internal state
	void UpdatePerformanceMetrics();

	// Internal helpers
	void ComputeInputLevels(jack_nframes_t Nframes);
}; 