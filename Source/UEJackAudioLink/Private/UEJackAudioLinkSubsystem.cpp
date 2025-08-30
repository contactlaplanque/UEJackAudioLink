#include "UEJackAudioLinkSubsystem.h"
#include "JackServerController.h"
#include "JackClientManager.h"
#include "JackAudioLinkSettings.h"
#include "UEJackAudioLinkLog.h"
#include "Containers/Ticker.h"

void UUEJackAudioLinkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogJackAudioLink, Log, TEXT("UEJackAudioLinkSubsystem initialized"));
}

void UUEJackAudioLinkSubsystem::Deinitialize()
{
	if (DebugTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DebugTickHandle);
		DebugTickHandle.Reset();
	}
	Super::Deinitialize();
}

bool UUEJackAudioLinkSubsystem::RestartServer(int32 SampleRate, int32 BufferSize)
{
#if WITH_JACK
	return FJackServerController::Get().RestartServer(SampleRate, BufferSize);
#else
	return false;
#endif
}

bool UUEJackAudioLinkSubsystem::ConnectClient(const FString& ClientName, int32 NumInputs, int32 NumOutputs)
{
#if WITH_JACK
	if (!FJackClientManager::Get().Connect(ClientName))
	{
		return false;
	}
	if (!FJackClientManager::Get().RegisterAudioPorts(NumInputs, NumOutputs, TEXT("unreal")))
	{
		return false;
	}
	return FJackClientManager::Get().Activate();
#else
	return false;
#endif
}

void UUEJackAudioLinkSubsystem::DisconnectClient()
{
#if WITH_JACK
	FJackClientManager::Get().Disconnect();
#endif
}

bool UUEJackAudioLinkSubsystem::IsServerRunning() const
{
#if WITH_JACK
	return FJackServerController::Get().IsServerRunning();
#else
	return false;
#endif
}

bool UUEJackAudioLinkSubsystem::IsClientConnected() const
{
#if WITH_JACK
	return FJackClientManager::Get().IsConnected();
#else
	return false;
#endif
}

// Audio I/O methods
TArray<float> UUEJackAudioLinkSubsystem::ReadAudioBuffer(int32 ChannelIndex, int32 NumSamples)
{
#if WITH_JACK
	if (IsClientConnected())
	{
		return FJackClientManager::Get().ReadAudioBuffer(ChannelIndex, NumSamples);
	}
#endif
	return TArray<float>();
}

bool UUEJackAudioLinkSubsystem::WriteAudioBuffer(int32 ChannelIndex, const TArray<float>& AudioData)
{
#if WITH_JACK
	if (IsClientConnected())
	{
		return FJackClientManager::Get().WriteAudioBuffer(ChannelIndex, AudioData);
	}
#endif
	return false;
}

float UUEJackAudioLinkSubsystem::GetInputLevel(int32 ChannelIndex) const
{
#if WITH_JACK
	if (IsClientConnected())
	{
		return FJackClientManager::Get().GetInputLevel(ChannelIndex);
	}
#endif
	return 0.0f;
}

int32 UUEJackAudioLinkSubsystem::GetSampleRate() const
{
#if WITH_JACK
	return static_cast<int32>(FJackClientManager::Get().GetSampleRate());
#else
	return 0;
#endif
}

int32 UUEJackAudioLinkSubsystem::GetBufferSize() const
{
#if WITH_JACK
	return static_cast<int32>(FJackClientManager::Get().GetBufferSize());
#else
	return 0;
#endif
}

float UUEJackAudioLinkSubsystem::GetCpuLoad() const
{
#if WITH_JACK
	return FJackClientManager::Get().GetCpuLoad();
#else
	return 0.0f;
#endif
}

TArray<FString> UUEJackAudioLinkSubsystem::GetConnectedClients() const
{
	return FJackClientManager::Get().GetAllClients();
}

void UUEJackAudioLinkSubsystem::GetClientPorts(const FString& ClientName, TArray<FString>& OutInputPorts, TArray<FString>& OutOutputPorts) const
{
#if WITH_JACK
	OutInputPorts = FJackClientManager::Get().GetClientInputPorts(ClientName);
	OutOutputPorts = FJackClientManager::Get().GetClientOutputPorts(ClientName);
#else
	OutInputPorts.Reset();
	OutOutputPorts.Reset();
#endif
}

bool UUEJackAudioLinkSubsystem::ConnectPorts(const FString& SourcePort, const FString& DestinationPort)
{
	return FJackClientManager::Get().ConnectPorts(SourcePort, DestinationPort);
}

bool UUEJackAudioLinkSubsystem::DisconnectPorts(const FString& SourcePort, const FString& DestinationPort)
{
	return FJackClientManager::Get().DisconnectPorts(SourcePort, DestinationPort);
}

void UUEJackAudioLinkSubsystem::NotifyClientConnected(const FString& ClientName, int32 NumInputs, int32 NumOutputs)
{
	OnNewJackClientConnected.Broadcast(ClientName, NumInputs, NumOutputs);
}

void UUEJackAudioLinkSubsystem::NotifyClientDisconnected(const FString& ClientName)
{
	OnJackClientDisconnected.Broadcast(ClientName);
}

static FString GetPortByIndexHelper(const FString& ClientName, int32 Number1Based, bool bWantInput)
{
#if WITH_JACK
	const auto& Mgr = FJackClientManager::Get();
	TArray<FString> Ports = bWantInput ? Mgr.GetClientInputPorts(ClientName) : Mgr.GetClientOutputPorts(ClientName);
	// Be forgiving: if 0 is provided, treat it as 1 (first port)
	const int32 ClampedIndex = (Number1Based <= 0) ? 1 : Number1Based;
	if (ClampedIndex >= 1 && ClampedIndex <= Ports.Num())
	{
		return Ports[ClampedIndex - 1];
	}
	UE_LOG(LogJackAudioLink, Warning, TEXT("GetPortByIndexHelper: Port Number out of range. Client=%s, WantInput=%s, RequestedNumber=%d, Available=%d"),
		*ClientName, bWantInput ? TEXT("true") : TEXT("false"), Number1Based, Ports.Num());
#endif
	return FString();
}

bool UUEJackAudioLinkSubsystem::ConnectPortsByIndex(EJackPortDirection SourceType, const FString& SourceClientName, int32 SourcePortNumber,
													EJackPortDirection DestType,   const FString& DestClientName,   int32 DestPortNumber)
{
	// Validate directions: JACK requires Source=Output, Dest=Input
	if (SourceType != EJackPortDirection::Output || DestType != EJackPortDirection::Input)
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("ConnectPortsByIndex: Invalid directions (Source must be Output, Dest must be Input). Requested: SrcType=%s, DstType=%s, SrcClient=%s Port#=%d, DstClient=%s Port#=%d"),
			SourceType == EJackPortDirection::Output ? TEXT("Output") : TEXT("Input"),
			DestType   == EJackPortDirection::Input  ? TEXT("Input")  : TEXT("Output"),
			*SourceClientName, SourcePortNumber, *DestClientName, DestPortNumber);
		return false;
	}

	const FString SourcePort = GetPortByIndexHelper(SourceClientName, SourcePortNumber, /*bWantInput*/ false);
	const FString DestPort   = GetPortByIndexHelper(DestClientName,   DestPortNumber,   /*bWantInput*/ true);
	if (SourcePort.IsEmpty() || DestPort.IsEmpty())
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("ConnectPortsByIndex: Failed to resolve port names. SrcClient=%s Port#=%d -> '%s' ; DstClient=%s Port#=%d -> '%s'"),
			*SourceClientName, SourcePortNumber, *SourcePort, *DestClientName, DestPortNumber, *DestPort);
		return false;
	}
	UE_LOG(LogJackAudioLink, Display, TEXT("ConnectPortsByIndex: Connecting '%s' -> '%s'"), *SourcePort, *DestPort);
	const bool bOK = ConnectPorts(SourcePort, DestPort);
	if (!bOK)
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("ConnectPortsByIndex: jack_connect failed for '%s' -> '%s'"), *SourcePort, *DestPort);
	}
	return bOK;
}

bool UUEJackAudioLinkSubsystem::DisconnectPortsByIndex(EJackPortDirection SourceType, const FString& SourceClientName, int32 SourcePortNumber,
													   EJackPortDirection DestType,   const FString& DestClientName,   int32 DestPortNumber)
{
	if (SourceType != EJackPortDirection::Output || DestType != EJackPortDirection::Input)
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("DisconnectPortsByIndex: Invalid directions (Source must be Output, Dest must be Input). Requested: SrcType=%s, DstType=%s, SrcClient=%s Port#=%d, DstClient=%s Port#=%d"),
			SourceType == EJackPortDirection::Output ? TEXT("Output") : TEXT("Input"),
			DestType   == EJackPortDirection::Input  ? TEXT("Input")  : TEXT("Output"),
			*SourceClientName, SourcePortNumber, *DestClientName, DestPortNumber);
		return false;
	}
	const FString SourcePort = GetPortByIndexHelper(SourceClientName, SourcePortNumber, /*bWantInput*/ false);
	const FString DestPort   = GetPortByIndexHelper(DestClientName,   DestPortNumber,   /*bWantInput*/ true);
	if (SourcePort.IsEmpty() || DestPort.IsEmpty())
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("DisconnectPortsByIndex: Failed to resolve port names. SrcClient=%s Port#=%d -> '%s' ; DstClient=%s Port#=%d -> '%s'"),
			*SourceClientName, SourcePortNumber, *SourcePort, *DestClientName, DestPortNumber, *DestPort);
		return false;
	}
	UE_LOG(LogJackAudioLink, Display, TEXT("DisconnectPortsByIndex: Disconnecting '%s' -> '%s'"), *SourcePort, *DestPort);
	const bool bOK = DisconnectPorts(SourcePort, DestPort);
	if (!bOK)
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("DisconnectPortsByIndex: jack_disconnect failed for '%s' -> '%s'"), *SourcePort, *DestPort);
	}
	return bOK;
}

FString UUEJackAudioLinkSubsystem::GetJackClientName() const
{
	return FJackClientManager::Get().GetClientName();
}
