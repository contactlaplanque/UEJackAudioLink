#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Jack/JackClient.h"
#include "Jack/JackTypes.h"
#include "JackAudioInterface.generated.h"

UCLASS()
class AKMCONTROL_API AJackAudioInterface : public AActor
{
	GENERATED_BODY()
	
public:	
	AJackAudioInterface();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Jack connection management
	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	bool ConnectToJack(const FString& ClientName = TEXT("UnrealJackClient"));

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	void DisconnectFromJack();

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	bool IsConnectedToJack() const;

	// Jack server management (internal use only)
	bool CheckJackServerRunning();

	bool KillJackServer();

	bool StartJackServer(const FString& JackdPath = TEXT("C:/Program Files/JACK2/jackd.exe"));

	bool EnsureJackServerRunning(const FString& JackdPath = TEXT("C:/Program Files/JACK2/jackd.exe"));

	bool StartJackServerWithParameters(const FString& JackdPath, int32 InSampleRate, int32 InBufferSize, const FString& InDriver = TEXT("portaudio"));

	// Jack information
	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	FString GetJackClientName() const;

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	int32 GetJackSampleRate() const;

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	int32 GetJackBufferSize() const;

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	float GetJackCPUUsage() const;

	// Port management
	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	TArray<FString> GetAvailableJackPorts();

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	bool ConnectJackPorts(const FString& SourcePort, const FString& DestinationPort);

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	bool DisconnectJackPorts(const FString& SourcePort, const FString& DestinationPort);

	// Audio port registration
	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	bool RegisterAudioPorts(int32 NumInputs = 64, int32 NumOutputs = 64, const FString& BaseName = TEXT("unreal"));

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	void UnregisterAllAudioPorts();

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	int32 GetNumRegisteredInputPorts() const;

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	int32 GetNumRegisteredOutputPorts() const;

	// Performance metrics
	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	FJackPerformanceMetrics GetJackPerformanceMetrics() const;

	// Connection status
	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	EJackConnectionStatus GetJackConnectionStatus() const;

	// Server information
	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	bool WasJackServerStarted() const;

	UFUNCTION(BlueprintCallable, Category = "Jack Audio")
	FString GetJackServerInfo() const;

	// Server monitoring (public for ImGui access)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Server Monitoring")
	bool bMonitorServerStatus = true; // Enabled by default for automatic management

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Server Monitoring")
	float ServerCheckInterval = 1.0f; // Check every second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Server Monitoring")
	bool bKillServerOnShutdown = true;

	// Make these public for ImGui access
	UPROPERTY(BlueprintReadOnly, Category = "Jack Audio|Server Monitoring")
	FTimerHandle ServerStatusTimerHandle;

	// Server monitoring methods (internal use only)
	void StartServerMonitoring();

	void StopServerMonitoring();

	void OnServerStatusCheck();

	// Client connection monitoring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Client Monitoring")
	bool bMonitorNewClientConnections = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Client Monitoring")
	float ClientMonitorInterval = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Client Monitoring", meta = (DisplayName = "Auto-Connect to New Clients"))
	bool bAutoConnectToNewClients = true;

	// Public getter for audio levels
	UFUNCTION(BlueprintPure, Category = "Jack Audio|Monitoring")
	float GetInputChannelLevel(int32 ChannelIndex) const;

protected:
	// Jack client instance (regular C++ class, not a UObject)
	FJackClient JackClient;

	// Jack server configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Server Configuration")
	FString JackServerPath = TEXT("C:/Program Files/JACK2/jackd.exe");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Server Configuration")
	int32 SampleRate = 48000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Server Configuration")
	int32 BufferSize = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Server Configuration")
	FString AudioDriver = TEXT("portaudio");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jack Audio|Server Configuration")
	bool bAutoStartServer = true;

	// Connection state tracking for logging
	bool bWasConnected = false;

private:
	// Internal helper functions
	void CheckServerStatus();

	// Timer for client monitoring
	FTimerHandle ClientMonitorTimerHandle;
	
	// Set of known clients to track new connections
	TSet<FString> KnownClients;
	
	// Function to check for new clients
	void MonitorNewClients();
	
	// Helper to connect to a client's ports
	void AutoConnectToClient(FString ClientName);
};
