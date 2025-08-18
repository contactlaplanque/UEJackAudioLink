#include "UEJackAudioLinkSubsystem.h"
#include "JackServerController.h"
#include "JackClientManager.h"
#include "JackAudioLinkSettings.h"
#include "Misc/App.h"
#include "Containers/Ticker.h"

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

void UUEJackAudioLinkSubsystem::StartAutoConnect(float IntervalSeconds, bool bEnable)
{
#if WITH_JACK
	if (!bEnable)
	{
		StopAutoConnect();
		return;
	}

	if (TickHandle.IsValid())
	{
		return;
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, IntervalSeconds](float DeltaTime)
	{
		if (!FJackClientManager::Get().IsConnected())
		{
			return true;
		}
		// Gather external clients
		TArray<FString> Clients = FJackClientManager::Get().GetAllClients();
		FString OurName = FJackClientManager::Get().GetClientName();
		Clients.Remove(OurName);
		Clients.Remove(TEXT("system"));

		TArray<FString> OurInputs = FJackClientManager::Get().GetInputPortNames();
		for (const FString& Client : Clients)
		{
			TArray<FString> Outs = FJackClientManager::Get().GetClientOutputPorts(Client);
			int32 Num = FMath::Min(Outs.Num(), OurInputs.Num());
			for (int32 i = 0; i < Num; ++i)
			{
				FJackClientManager::Get().ConnectPorts(Outs[i], OurInputs[i]);
			}
		}
		return true;
	}), IntervalSeconds);
#endif
}

void UUEJackAudioLinkSubsystem::StopAutoConnect()
{
#if WITH_JACK
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
#endif
}


