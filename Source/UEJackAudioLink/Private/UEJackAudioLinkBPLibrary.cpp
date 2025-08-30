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

TArray<float> UUEJackAudioLinkBPLibrary::ReadAudioBuffer(int32 ChannelIndex, int32 NumSamples)
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->ReadAudioBuffer(ChannelIndex, NumSamples);
		}
	}
	return TArray<float>();
}

bool UUEJackAudioLinkBPLibrary::WriteAudioBuffer(int32 ChannelIndex, const TArray<float>& AudioData)
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->WriteAudioBuffer(ChannelIndex, AudioData);
		}
	}
	return false;
}

float UUEJackAudioLinkBPLibrary::GetInputLevel(int32 ChannelIndex)
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->GetInputLevel(ChannelIndex);
		}
	}
	return 0.0f;
}

int32 UUEJackAudioLinkBPLibrary::GetSampleRate()
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->GetSampleRate();
		}
	}
	return 0;
}

int32 UUEJackAudioLinkBPLibrary::GetBufferSize()
{
	if (GEngine)
	{
		if (UUEJackAudioLinkSubsystem* Subsys = GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			return Subsys->GetBufferSize();
		}
	}
	return 0;
}



