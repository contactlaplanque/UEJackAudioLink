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
