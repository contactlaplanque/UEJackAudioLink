#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UEJackAudioLinkSubsystem.h" // for EJackPortDirection
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

	// Audio I/O functions
	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Audio")
	static TArray<float> ReadAudioBuffer(int32 ChannelIndex, int32 NumSamples);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Audio")
	static bool WriteAudioBuffer(int32 ChannelIndex, const TArray<float>& AudioData);

	UFUNCTION(BlueprintPure, Category="JackAudioLink|Audio")
	static float GetInputLevel(int32 ChannelIndex);

	UFUNCTION(BlueprintPure, Category="JackAudioLink|Audio")
	static int32 GetSampleRate();

	UFUNCTION(BlueprintPure, Category="JackAudioLink|Audio")
	static int32 GetBufferSize();

	// Info
	UFUNCTION(BlueprintPure, Category="JackAudioLink|Info")
	static float GetCpuLoad();

	// Discovery
	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Discovery")
	static TArray<FString> GetConnectedClients();

	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Discovery")
	static void GetClientPorts(const FString& ClientName, TArray<FString>& OutInputPorts, TArray<FString>& OutOutputPorts);

	// Routing
	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Routing")
	static bool ConnectPorts(const FString& SourcePort, const FString& DestinationPort);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Routing")
	static bool DisconnectPorts(const FString& SourcePort, const FString& DestinationPort);

	// Routing by client + index (1-based)
	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Routing")
	static bool ConnectPortsByIndex(EJackPortDirection SourceType, const FString& SourceClientName, int32 SourcePortNumber,
									 EJackPortDirection DestType,   const FString& DestClientName,   int32 DestPortNumber);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Routing")
	static bool DisconnectPortsByIndex(EJackPortDirection SourceType, const FString& SourceClientName, int32 SourcePortNumber,
									   EJackPortDirection DestType,   const FString& DestClientName,   int32 DestPortNumber);

	// Our Unreal JACK client name
	UFUNCTION(BlueprintPure, Category="JackAudioLink|Info")
	static FString GetJackClientName();
};