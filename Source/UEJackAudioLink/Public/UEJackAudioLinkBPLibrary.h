#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UEJackAudioLinkBPLibrary.generated.h"

UCLASS()
class UEJACKAUDIOLINK_API UUEJackAudioLinkBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="JackAudioLink")
	static bool RestartServer(int32 SampleRate, int32 BufferSize);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink")
	static bool ConnectClient(const FString& ClientName, int32 NumInputs, int32 NumOutputs);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink")
	static void DisconnectClient();

	UFUNCTION(BlueprintPure, Category="JackAudioLink")
	static bool IsServerRunning();

	UFUNCTION(BlueprintPure, Category="JackAudioLink")
	static bool IsClientConnected();
};


