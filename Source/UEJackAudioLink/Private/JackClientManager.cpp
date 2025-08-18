#include "JackClientManager.h"
#include "UEJackAudioLinkLog.h"

#if WITH_JACK
#include <jack/jack.h>
#include <jack/types.h>
#endif

FJackClientManager& FJackClientManager::Get()
{
	static FJackClientManager Singleton;
	return Singleton;
}

FJackClientManager::FJackClientManager() = default;
FJackClientManager::~FJackClientManager()
{
	Disconnect();
}

bool FJackClientManager::Connect(const FString& ClientName)
{
#if WITH_JACK
	if (JackClient)
	{
		return true;
	}
	FTCHARToUTF8 NameUtf8(*ClientName);
	jack_status_t Status = JackServerFailed;
	JackClient = jack_client_open(NameUtf8.Get(), JackNullOption, &Status);
	if (!JackClient)
	{
		UE_LOG(LogJackAudioLink, Error, TEXT("Failed to open JACK client (status 0x%x)"), (uint32)Status);
		return false;
	}
	if (Status & JackNameNotUnique)
	{
		UE_LOG(LogJackAudioLink, Verbose, TEXT("JACK assigned unique name: %s"), UTF8_TO_TCHAR(jack_get_client_name(JackClient)));
	}
	// Setup essential callbacks
#if WITH_JACK
	jack_on_shutdown(JackClient, [](void* arg){ UE_LOG(LogJackAudioLink, Warning, TEXT("JACK server shutdown signaled")); }, this);
	jack_set_xrun_callback(JackClient, [](void* arg){ UE_LOG(LogJackAudioLink, Verbose, TEXT("JACK xrun")); return 0; }, this);
	jack_set_port_registration_callback(JackClient, &FJackClientManager::PortRegistrationCallback, this);
#endif
	return true;
#else
	UE_LOG(LogJackAudioLink, Warning, TEXT("WITH_JACK=0: Connect is a no-op"));
	return false;
#endif
}

void FJackClientManager::Disconnect()
{
#if WITH_JACK
	UnregisterAllPorts();
	if (JackClient)
	{
		jack_deactivate(JackClient);
		jack_client_close(JackClient);
		JackClient = nullptr;
	}
#endif
}

bool FJackClientManager::IsConnected() const
{
	return JackClient != nullptr;
}

bool FJackClientManager::RegisterAudioPorts(int32 NumInputs, int32 NumOutputs, const FString& BaseName)
{
#if WITH_JACK
	if (!JackClient)
	{
		return false;
	}
	UnregisterAllPorts();
	for (int32 i = 0; i < NumInputs; ++i)
	{
		FString Name = FString::Printf(TEXT("%s_in_%d"), *BaseName, i + 1);
		FTCHARToUTF8 NameUtf8(*Name);
		jack_port_t* Port = jack_port_register(JackClient, NameUtf8.Get(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		if (Port) { InputPorts.Add(Port); }
	}
	for (int32 i = 0; i < NumOutputs; ++i)
	{
		FString Name = FString::Printf(TEXT("%s_out_%d"), *BaseName, i + 1);
		FTCHARToUTF8 NameUtf8(*Name);
		jack_port_t* Port = jack_port_register(JackClient, NameUtf8.Get(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		if (Port) { OutputPorts.Add(Port); }
	}
	return true;
#else
	return false;
#endif
}

void FJackClientManager::UnregisterAllPorts()
{
#if WITH_JACK
	for (jack_port_t* Port : InputPorts)
	{
		if (Port && JackClient) { jack_port_unregister(JackClient, Port); }
	}
	for (jack_port_t* Port : OutputPorts)
	{
		if (Port && JackClient) { jack_port_unregister(JackClient, Port); }
	}
	InputPorts.Empty();
	OutputPorts.Empty();
#endif
}

TArray<FString> FJackClientManager::GetAvailablePorts(const FString& NamePattern, const FString& TypePattern, uint32 Flags) const
{
	TArray<FString> Ports;
#if WITH_JACK
	if (!JackClient) { return Ports; }
	FTCHARToUTF8 NameUtf8(*NamePattern);
	FTCHARToUTF8 TypeUtf8(*TypePattern);
	const char** CPorts = jack_get_ports(JackClient,
		NamePattern.IsEmpty() ? nullptr : NameUtf8.Get(),
		TypePattern.IsEmpty() ? nullptr : TypeUtf8.Get(),
		Flags);
	if (CPorts)
	{
		for (int i = 0; CPorts[i] != nullptr; ++i)
		{
			Ports.Add(UTF8_TO_TCHAR(CPorts[i]));
		}
		jack_free(const_cast<char**>(CPorts));
	}
#endif
	return Ports;
}

bool FJackClientManager::ConnectPorts(const FString& SourcePort, const FString& DestinationPort)
{
#if WITH_JACK
	if (!JackClient) { return false; }
	FTCHARToUTF8 Src(*SourcePort);
	FTCHARToUTF8 Dst(*DestinationPort);
	return jack_connect(JackClient, Src.Get(), Dst.Get()) == 0;
#else
	return false;
#endif
}

bool FJackClientManager::DisconnectPorts(const FString& SourcePort, const FString& DestinationPort)
{
#if WITH_JACK
	if (!JackClient) { return false; }
	FTCHARToUTF8 Src(*SourcePort);
	FTCHARToUTF8 Dst(*DestinationPort);
	return jack_disconnect(JackClient, Src.Get(), Dst.Get()) == 0;
#else
	return false;
#endif
}

FString FJackClientManager::GetClientName() const
{
#if WITH_JACK
	if (JackClient)
	{
		return FString(UTF8_TO_TCHAR(jack_get_client_name(JackClient)));
	}
#endif
	return FString();
}

uint32 FJackClientManager::GetSampleRate() const
{
#if WITH_JACK
	if (JackClient) { return jack_get_sample_rate(JackClient); }
#endif
	return 0;
}

uint32 FJackClientManager::GetBufferSize() const
{
#if WITH_JACK
	if (JackClient) { return jack_get_buffer_size(JackClient); }
#endif
	return 0;
}

bool FJackClientManager::Activate()
{
#if WITH_JACK
	if (!JackClient) { return false; }
	return jack_activate(JackClient) == 0;
#else
	return false;
#endif
}

bool FJackClientManager::Deactivate()
{
#if WITH_JACK
	if (!JackClient) { return false; }
	return jack_deactivate(JackClient) == 0;
#else
	return false;
#endif
}

void FJackClientManager::PortRegistrationCallback(unsigned int PortId, int Register, void* Arg)
{
#if WITH_JACK
	FJackClientManager* Self = static_cast<FJackClientManager*>(Arg);
	if (!Self || !Self->JackClient || Register == 0)
	{
		return;
	}
	jack_port_t* Port = jack_port_by_id(Self->JackClient, PortId);
	if (!Port)
	{
		return;
	}
	const char* NameC = jack_port_name(Port);
	if (!NameC)
	{
		return;
	}
	FString PortFull = UTF8_TO_TCHAR(NameC);
	FString ClientName;
	if (!PortFull.Split(TEXT(":"), &ClientName, nullptr))
	{
		return;
	}
	if (!Self->KnownClientsLogged.Contains(ClientName))
	{
		TArray<FString> Outputs = Self->GetClientOutputPorts(ClientName);
		int32 NumOut = Outputs.Num();
		UE_LOG(LogJackAudioLink, Log, TEXT("Client connected: %s (outputs: %d)"), *ClientName, NumOut);
		Self->KnownClientsLogged.Add(ClientName);
		// Optionally auto-connect
		Self->AutoConnectToClient(ClientName);
	}
#endif
}

void FJackClientManager::AutoConnectToClient(const FString& ClientName)
{
#if WITH_JACK
	if (!JackClient) { return; }
	TArray<FString> Outs = GetClientOutputPorts(ClientName);
	TArray<FString> Ins = GetInputPortNames();
	const int32 Num = FMath::Min(Outs.Num(), Ins.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		jack_connect(JackClient, TCHAR_TO_ANSI(*Outs[i]), TCHAR_TO_ANSI(*Ins[i]));
	}
#endif
}

TArray<FString> FJackClientManager::GetAllClients() const
{
    TArray<FString> Clients;
#if WITH_JACK
    if (!JackClient) { return Clients; }
    const char** Ports = jack_get_ports(JackClient, nullptr, nullptr, 0);
    if (Ports)
    {
        TSet<FString> Unique;
        for (int i = 0; Ports[i] != nullptr; ++i)
        {
            FString PortName = UTF8_TO_TCHAR(Ports[i]);
            FString Client;
            if (PortName.Split(TEXT(":"), &Client, nullptr))
            {
                Unique.Add(Client);
            }
        }
        Clients = Unique.Array();
        jack_free(const_cast<char**>(Ports));
    }
#endif
    return Clients;
}

TArray<FString> FJackClientManager::GetInputPortNames() const
{
    TArray<FString> Names;
#if WITH_JACK
    for (jack_port_t* Port : InputPorts)
    {
        if (Port && JackClient)
        {
            Names.Add(UTF8_TO_TCHAR(jack_port_name(Port)));
        }
    }
#endif
    return Names;
}

TArray<FString> FJackClientManager::GetClientOutputPorts(const FString& ClientName) const
{
    TArray<FString> Ports;
#if WITH_JACK
    if (!JackClient) { return Ports; }
    FString Pattern = ClientName + TEXT(":*");
    FTCHARToUTF8 NameUtf8(*Pattern);
    const char** CPorts = jack_get_ports(JackClient, NameUtf8.Get(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    if (CPorts)
    {
        for (int i = 0; CPorts[i] != nullptr; ++i)
        {
            Ports.Add(UTF8_TO_TCHAR(CPorts[i]));
        }
        jack_free(const_cast<char**>(CPorts));
    }
#endif
    return Ports;
}


