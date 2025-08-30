#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Containers/Ticker.h"
#include "UEJackAudioLinkSubsystem.generated.h"

UENUM(BlueprintType)
enum class EJackPortDirection : uint8
{
	Input  UMETA(DisplayName="Input"),
	Output UMETA(DisplayName="Output")
};

// Blueprint event signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNewJackClientConnected, const FString&, ClientName, int32, NumInputPorts, int32, NumOutputPorts);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJackClientDisconnected, const FString&, ClientName);

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

	// CPU load (0..100)
	UFUNCTION(BlueprintPure, Category="JackAudioLink|Info")
	float GetCpuLoad() const;

	// Client/port discovery
	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Discovery")
	TArray<FString> GetConnectedClients() const;

	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Discovery")
	void GetClientPorts(const FString& ClientName, TArray<FString>& OutInputPorts, TArray<FString>& OutOutputPorts) const;

	// Make/Break connections
	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Routing")
	bool ConnectPorts(const FString& SourcePort, const FString& DestinationPort);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Routing")
	bool DisconnectPorts(const FString& SourcePort, const FString& DestinationPort);

	// Routing by client and index (1-based index)
	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Routing")
	bool ConnectPortsByIndex(EJackPortDirection SourceType, const FString& SourceClientName, UPARAM(DisplayName="Source Port Number") int32 SourcePortNumber,
							 EJackPortDirection DestType,   const FString& DestClientName,   UPARAM(DisplayName="Destination Port Number") int32 DestPortNumber);

	UFUNCTION(BlueprintCallable, Category="JackAudioLink|Routing")
	bool DisconnectPortsByIndex(EJackPortDirection SourceType, const FString& SourceClientName, UPARAM(DisplayName="Source Port Number") int32 SourcePortNumber,
								EJackPortDirection DestType,   const FString& DestClientName,   UPARAM(DisplayName="Destination Port Number") int32 DestPortNumber);

	// Our Unreal JACK client name
	UFUNCTION(BlueprintPure, Category="JackAudioLink|Info")
	FString GetJackClientName() const;

	// Blueprint-assignable events
	UPROPERTY(BlueprintAssignable, Category="JackAudioLink|Events")
	FOnNewJackClientConnected OnNewJackClientConnected;

	UPROPERTY(BlueprintAssignable, Category="JackAudioLink|Events")
	FOnJackClientDisconnected OnJackClientDisconnected;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	FTSTicker::FDelegateHandle TickHandle;
	FTSTicker::FDelegateHandle DebugTickHandle;
    
public:
	// Internal notifications from JACK manager (dispatched on game thread)
	void NotifyClientConnected(const FString& ClientName, int32 NumInputs, int32 NumOutputs);
	void NotifyClientDisconnected(const FString& ClientName);
};