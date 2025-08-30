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

	// Audio I/O methods
	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Audio")
	TArray<float> ReadAudioBuffer(int32 ChannelIndex, int32 NumSamples);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Audio")
	bool WriteAudioBuffer(int32 ChannelIndex, const TArray<float>& AudioData);

	UFUNCTION(BlueprintPure, Category="JackAudioLink|Audio")
	float GetInputLevel(int32 ChannelIndex) const;

	UFUNCTION(BlueprintPure, Category="JackAudioLink|Audio")
	int32 GetSampleRate() const;

	UFUNCTION(BlueprintPure, Category="JackAudioLink|Audio")
	int32 GetBufferSize() const;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	FTSTicker::FDelegateHandle TickHandle;
	FTSTicker::FDelegateHandle DebugTickHandle;
	
};