#include "UEJackAudioLinkBPLibrary.h"
#include "UEJackAudioLinkSubsystem.h"
#include "Engine/Engine.h"

bool UUEJackAudioLinkBPLibrary::RestartServer(int32 SampleRate, int32 BufferSize)
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->RestartServer(SampleRate, BufferSize);
		}
	}
	return false;
}

bool UUEJackAudioLinkBPLibrary::ConnectClient(const FString& ClientName, int32 NumInputs, int32 NumOutputs)
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->ConnectClient(ClientName, NumInputs, NumOutputs);
		}
	}
	return false;
}

void UUEJackAudioLinkBPLibrary::DisconnectClient()
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			Subsys->DisconnectClient();
		}
	}
}

bool UUEJackAudioLinkBPLibrary::IsServerRunning()
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->IsServerRunning();
		}
	}
	return false;
}

bool UUEJackAudioLinkBPLibrary::IsClientConnected()
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->IsClientConnected();
		}
	}
	return false;
}


