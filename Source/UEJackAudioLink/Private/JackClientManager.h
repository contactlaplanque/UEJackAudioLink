#pragma once

#include "CoreMinimal.h"

#if WITH_JACK
#include <jack/jack.h>
#include <jack/types.h>
#endif

class FJackClientManager
{
public:
	static FJackClientManager& Get();

	bool Connect(const FString& ClientName);
	void Disconnect();
	bool IsConnected() const;

	// Basic callbacks
	void SetupCallbacks();

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

	// Info
	FString GetClientName() const;
	uint32 GetSampleRate() const;
	uint32 GetBufferSize() const;

private:
	FJackClientManager();
	~FJackClientManager();

	// JACK callbacks
	static void PortRegistrationCallback(unsigned int PortId, int Register, void* Arg);

	jack_client_t* JackClient = nullptr;
	TArray<jack_port_t*> InputPorts;
	TArray<jack_port_t*> OutputPorts;
	TSet<FString> KnownClientsLogged;
};


