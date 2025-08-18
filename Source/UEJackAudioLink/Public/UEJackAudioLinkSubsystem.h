#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Containers/Ticker.h"
#include "UEJackAudioLinkSubsystem.generated.h"

UCLASS()
class UEJACKAUDIOLINK_API UUEJackAudioLinkSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:
	// Basic controls
	UFUNCTION(BlueprintCallable, Category="JackAudioLink")
	bool RestartServer(int32 SampleRate, int32 BufferSize);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink")
	bool ConnectClient(const FString& ClientName, int32 NumInputs, int32 NumOutputs);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink")
	void DisconnectClient();

	UFUNCTION(BlueprintPure, Category="JackAudioLink")
	bool IsServerRunning() const;

	UFUNCTION(BlueprintPure, Category="JackAudioLink")
	bool IsClientConnected() const;

	// Auto-connect management
	void StartAutoConnect(float IntervalSeconds, bool bEnable);
	void StopAutoConnect();

private:
	FTSTicker::FDelegateHandle TickHandle;
};


