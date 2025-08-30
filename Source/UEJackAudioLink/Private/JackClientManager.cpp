#include "JackClientManager.h"
#include "UEJackAudioLinkLog.h"
#include "Async/Async.h"
#include "UEJackAudioLinkSubsystem.h"
#include "Engine/Engine.h"

#if WITH_JACK
#include <jack/jack.h>
#include <jack/types.h>
// Escape regex metacharacters for JACK's POSIX regex patterns in jack_get_ports
static FString EscapeRegex(const FString& In)
{
	// include backslash in the set; double-escape here to keep the literal valid at EOL
	static const TCHAR* Meta = TEXT(".^$|()[]{}*+?\\");
	FString Out;
	Out.Reserve(In.Len() * 2);
	for (TCHAR C : In)
	{
		if (FCString::Strchr(Meta, C) != nullptr)
		{
			Out.AppendChar(TEXT('\\'));
		}
		Out.AppendChar(C);
	}
	return Out;
}
#endif

// FAudioRingBuffer implementation
void FAudioRingBuffer::Write(const float* Data, int32 NumSamples)
{
    FScopeLock Lock(&CriticalSection);
    for (int32 i = 0; i < NumSamples; ++i)
    {
        Buffer[WritePos] = Data[i];
        WritePos = (WritePos + 1) % Capacity;
        // If buffer is full, advance read position to avoid overflow
        if (WritePos == ReadPos)
        {
            ReadPos = (ReadPos + 1) % Capacity;
        }
    }
}

int32 FAudioRingBuffer::Read(float* OutData, int32 NumSamples)
{
    FScopeLock Lock(&CriticalSection);
    int32 SamplesRead = 0;
    while (SamplesRead < NumSamples && ReadPos != WritePos)
    {
        OutData[SamplesRead] = Buffer[ReadPos];
        ReadPos = (ReadPos + 1) % Capacity;
        SamplesRead++;
    }
    // Zero remaining samples if requested more than available
    for (int32 i = SamplesRead; i < NumSamples; ++i)
    {
        OutData[i] = 0.0f;
    }
    return SamplesRead;
}

void FAudioRingBuffer::Clear()
{
    FScopeLock Lock(&CriticalSection);
    ReadPos = WritePos = 0;
    Buffer.SetNumZeroed(Capacity);
}

int32 FAudioRingBuffer::GetAvailableRead() const
{
    FScopeLock Lock(&CriticalSection);
    return (WritePos - ReadPos + Capacity) % Capacity;
}

float FAudioRingBuffer::GetRMSLevel() const
{
    FScopeLock Lock(&CriticalSection);
    if (ReadPos == WritePos) return 0.0f;
    
    float Sum = 0.0f;
    int32 Count = 0;
    int32 Pos = ReadPos;
    
    // Sample recent 1024 samples for RMS calculation
    int32 SamplesToCheck = FMath::Min(1024, GetAvailableRead());
    for (int32 i = 0; i < SamplesToCheck; ++i)
    {
        float Sample = Buffer[Pos];
        Sum += Sample * Sample;
        Pos = (Pos + 1) % Capacity;
        Count++;
    }
    
    return Count > 0 ? FMath::Sqrt(Sum / Count) : 0.0f;
}

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
	jack_on_shutdown(JackClient, [](void* arg)
	{
		FJackClientManager* Self = static_cast<FJackClientManager*>(arg);
		if (Self)
		{
			UE_LOG(LogJackAudioLink, Warning, TEXT("JACK server shutdown signaled (client manager)"));
			// Defer cleanup to game thread to avoid mutating arrays during JACK callback
			AsyncTask(ENamedThreads::GameThread, [Self]()
			{
				Self->UnregisterAllPorts();
				Self->JackClient = nullptr;
			});
		}
	}, this);
	
	jack_set_xrun_callback(JackClient, [](void* /*arg*/){ UE_LOG(LogJackAudioLink, Verbose, TEXT("JACK xrun")); return 0; }, this);
	jack_set_client_registration_callback(JackClient, &FJackClientManager::ClientRegistrationCallback, this);
	jack_set_port_registration_callback(JackClient, &FJackClientManager::PortRegistrationCallback, this);
	
	// Set the audio process callback
	jack_set_process_callback(JackClient, &FJackClientManager::ProcessCallback, this);
	
	return true;
#else
	UE_LOG(LogJackAudioLink, Warning, TEXT("WITH_JACK=0: Connect is a no-op"));
	return false;
#endif
}
void FJackClientManager::ClientRegistrationCallback(const char* Name, int Register, void* Arg)
{
#if WITH_JACK
	FJackClientManager* Self = static_cast<FJackClientManager*>(Arg);
	if (!Self || !Self->JackClient || !Name) { return; }
	const bool bRegistered = (Register != 0);
	const FString ClientName = UTF8_TO_TCHAR(Name);
	if (bRegistered)
	{
	// Filter out our own client to avoid noise
	if (ClientName == Self->GetClientName()) { return; }
	// Do not broadcast here: ports may not be registered yet. PortRegistrationCallback will announce with proper counts.
	UE_LOG(LogJackAudioLink, Verbose, TEXT("Client registered: %s"), *ClientName);
	}
	else
	{
		if (Self->KnownClientsLogged.Contains(ClientName))
		{
			UE_LOG(LogJackAudioLink, Log, TEXT("Client unregistered: %s"), *ClientName);
			Self->KnownClientsLogged.Remove(ClientName);
			AsyncTask(ENamedThreads::GameThread, [ClientName]()
			{
				if (GEngine)
				{
					if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
					{
						Subsys->NotifyClientDisconnected(ClientName);
					}
				}
			});
		}
	}
#endif
}

void FJackClientManager::Disconnect()
{
#if WITH_JACK
	if (JackClient)
	{
		// Deactivate first to halt callbacks, then unregister ports, then close client
		jack_deactivate(JackClient);
		UnregisterAllPorts();
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
	
	// Create ring buffers for inputs and outputs
	InputRingBuffers.Empty();
	OutputRingBuffers.Empty();
	
	for (int32 i = 0; i < NumInputs; ++i)
	{
		FString Name = FString::Printf(TEXT("%s_in_%d"), *BaseName, i + 1);
		FTCHARToUTF8 NameUtf8(*Name);
		jack_port_t* Port = jack_port_register(JackClient, NameUtf8.Get(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		if (Port) 
		{ 
			InputPorts.Add(Port);
			InputRingBuffers.Add(MakeUnique<FAudioRingBuffer>(8192));
		}
	}
	
	for (int32 i = 0; i < NumOutputs; ++i)
	{
		FString Name = FString::Printf(TEXT("%s_out_%d"), *BaseName, i + 1);
		FTCHARToUTF8 NameUtf8(*Name);
		jack_port_t* Port = jack_port_register(JackClient, NameUtf8.Get(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		if (Port) 
		{ 
			OutputPorts.Add(Port);
			OutputRingBuffers.Add(MakeUnique<FAudioRingBuffer>(8192));
		}
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
	InputRingBuffers.Empty();
	OutputRingBuffers.Empty();
#endif
}

// JACK Process Callback - This runs in real-time thread
int FJackClientManager::ProcessCallback(jack_nframes_t NumFrames, void* Arg)
{
#if WITH_JACK
	FJackClientManager* Self = static_cast<FJackClientManager*>(Arg);
	if (!Self || !Self->JackClient)
	{
		return 0;
	}

	// Process input ports
	for (int32 i = 0; i < Self->InputPorts.Num(); ++i)
	{
		if (Self->InputPorts[i] && Self->InputRingBuffers.IsValidIndex(i))
		{
			jack_default_audio_sample_t* InBuffer = static_cast<jack_default_audio_sample_t*>(
				jack_port_get_buffer(Self->InputPorts[i], NumFrames));
			if (InBuffer)
			{
				Self->InputRingBuffers[i]->Write(InBuffer, NumFrames);
			}
		}
	}

	// Process output ports
	for (int32 i = 0; i < Self->OutputPorts.Num(); ++i)
	{
		if (Self->OutputPorts[i] && Self->OutputRingBuffers.IsValidIndex(i))
		{
			jack_default_audio_sample_t* OutBuffer = static_cast<jack_default_audio_sample_t*>(
				jack_port_get_buffer(Self->OutputPorts[i], NumFrames));
			if (OutBuffer)
			{
				// Read from ring buffer to output
				Self->OutputRingBuffers[i]->Read(OutBuffer, NumFrames);
			}
		}
	}

	return 0;
#else
	return 0;
#endif
}

// Audio I/O methods
TArray<float> FJackClientManager::ReadAudioBuffer(int32 ChannelIndex, int32 NumSamples)
{
	TArray<float> Result;
#if WITH_JACK
	if (InputRingBuffers.IsValidIndex(ChannelIndex) && InputRingBuffers[ChannelIndex].IsValid())
	{
		Result.SetNumZeroed(NumSamples);
		InputRingBuffers[ChannelIndex]->Read(Result.GetData(), NumSamples);
	}
#endif
	return Result;
}

bool FJackClientManager::WriteAudioBuffer(int32 ChannelIndex, const TArray<float>& AudioData)
{
#if WITH_JACK
	if (OutputRingBuffers.IsValidIndex(ChannelIndex) && OutputRingBuffers[ChannelIndex].IsValid())
	{
		OutputRingBuffers[ChannelIndex]->Write(AudioData.GetData(), AudioData.Num());
		return true;
	}
#endif
	return false;
}

float FJackClientManager::GetInputLevel(int32 ChannelIndex) const
{
#if WITH_JACK
	if (InputRingBuffers.IsValidIndex(ChannelIndex) && InputRingBuffers[ChannelIndex].IsValid())
	{
		return InputRingBuffers[ChannelIndex]->GetRMSLevel();
	}
#endif
	return 0.0f;
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

float FJackClientManager::GetCpuLoad() const
{
#if WITH_JACK
	if (JackClient) { return jack_cpu_load(JackClient); }
#endif
	return 0.0f;
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
	if (!Self || !Self->JackClient)
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
	const bool bRegistered = (Register != 0);
	FString PortFull = UTF8_TO_TCHAR(NameC);
	FString ClientName;
	if (!PortFull.Split(TEXT(":"), &ClientName, nullptr))
	{
		return;
	}

	if (bRegistered)
	{
		// New/added port: if first time we see client, announce connect
		if (!Self->KnownClientsLogged.Contains(ClientName))
		{
			TArray<FString> Inputs = Self->GetClientInputPorts(ClientName);
			TArray<FString> Outputs = Self->GetClientOutputPorts(ClientName);
			const int32 NumIn = Inputs.Num();
			const int32 NumOut = Outputs.Num();
			UE_LOG(LogJackAudioLink, Log, TEXT("Client connected: %s (in:%d, out:%d)"), *ClientName, NumIn, NumOut);
			Self->KnownClientsLogged.Add(ClientName);
			// Fire subsystem event on game thread
			AsyncTask(ENamedThreads::GameThread, [ClientName, NumIn, NumOut]()
			{
				if (GEngine)
				{
					if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
					{
						Subsys->NotifyClientConnected(ClientName, NumIn, NumOut);
					}
				}
			});
			// Optional auto-connect
			Self->AutoConnectToClient(ClientName);
		}
	}
	else
	{
		// A port was unregistered; if client has no more ports, consider it disconnected
		TArray<FString> Inputs = Self->GetClientInputPorts(ClientName);
		TArray<FString> Outputs = Self->GetClientOutputPorts(ClientName);
		if (Inputs.Num() == 0 && Outputs.Num() == 0)
		{
			if (Self->KnownClientsLogged.Contains(ClientName))
			{
				UE_LOG(LogJackAudioLink, Log, TEXT("Client disconnected: %s"), *ClientName);
				Self->KnownClientsLogged.Remove(ClientName);
				AsyncTask(ENamedThreads::GameThread, [ClientName]()
				{
					if (GEngine)
					{
						if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
						{
							Subsys->NotifyClientDisconnected(ClientName);
						}
					}
				});
			}
		}
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
	const FString Pattern = FString::Printf(TEXT("^(%s):.*$"), *EscapeRegex(ClientName));
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

TArray<FString> FJackClientManager::GetClientInputPorts(const FString& ClientName) const
{
	TArray<FString> Ports;
#if WITH_JACK
	if (!JackClient) { return Ports; }
	const FString Pattern = FString::Printf(TEXT("^(%s):.*$"), *EscapeRegex(ClientName));
	FTCHARToUTF8 NameUtf8(*Pattern);
	const char** CPorts = jack_get_ports(JackClient, NameUtf8.Get(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
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