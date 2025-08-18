// JackAudioInterface.cpp

#include "JackAudioInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"
#include "jack/jack.h"

AJackAudioInterface::AJackAudioInterface()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AJackAudioInterface::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Starting Jack audio system..."));
	
	// Ensure Jack server is running with our custom parameters
	if (bAutoStartServer)
	{
		// Check if a server is already running and kill it
		if (CheckJackServerRunning())
		{
			UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Found existing Jack server, killing it to restart with custom parameters"));
			KillJackServer();
			FPlatformProcess::Sleep(1.0f); // Give it time to shut down
		}
		
		// Start the server with our custom parameters
		if (StartJackServerWithParameters(JackServerPath, SampleRate, BufferSize, AudioDriver))
		{
			UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Jack server started with custom parameters"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JackAudioInterface: Failed to start Jack server with custom parameters"));
			return;
				}
	}

	// Connect to the Jack server
	UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Jack server is ready, connecting client..."));
	
			if (ConnectToJack())
		{
			// Register 64 I/O ports after successful connection
			if (RegisterAudioPorts(64, 64, TEXT("unreal")))
			{
				UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Successfully registered 64 I/O ports"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("JackAudioInterface: Failed to register audio ports"));
			}

		// Always start server monitoring for automatic reconnection attempts
		StartServerMonitoring();

		// Start client connection monitoring if enabled
		if (bMonitorNewClientConnections)
		{
			// Initial check
			MonitorNewClients();
			
			// Set up timer for subsequent checks
			GetWorld()->GetTimerManager().SetTimer(ClientMonitorTimerHandle, this, &AJackAudioInterface::MonitorNewClients, ClientMonitorInterval, true);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("JackAudioInterface: Failed to connect to Jack server"));
	}
}

void AJackAudioInterface::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop server monitoring
	if (ServerStatusTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(ServerStatusTimerHandle);
	}

	// Kill Jack server if requested
	if (bKillServerOnShutdown)
	{
		UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Shutting down, killing Jack server..."));
		KillJackServer();
	}

	// Stop client monitoring timer
	if (ClientMonitorTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(ClientMonitorTimerHandle);
	}

	// Disconnect from Jack
	DisconnectFromJack();

	Super::EndPlay(EndPlayReason);
}

void AJackAudioInterface::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AJackAudioInterface::ConnectToJack(const FString& ClientName)
{
	bool bSuccess = JackClient.Connect(ClientName);
	
	if (bSuccess)
	{
		// Activate the client
		if (JackClient.Activate())
		{
			UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Jack client activated"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JackAudioInterface: Failed to activate Jack client"));
		}
		bWasConnected = true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("JackAudioInterface: Failed to connect to Jack server"));
	}
	
	return bSuccess;
}

void AJackAudioInterface::DisconnectFromJack()
{
	UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Disconnecting from Jack server..."));
	
	// Stop server monitoring first
	StopServerMonitoring();
	
	// Disconnect the Jack client
	JackClient.Disconnect();
	bWasConnected = false;
	
	UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Disconnected from Jack server"));
}

bool AJackAudioInterface::IsConnectedToJack() const
{
	return JackClient.IsConnected();
}

bool AJackAudioInterface::CheckJackServerRunning()
{
	return JackClient.CheckJackServerRunning();
}

bool AJackAudioInterface::KillJackServer()
{
	UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Killing Jack server..."));
	
	// Completely disable server monitoring to prevent hanging
	StopServerMonitoring();
	
	// Force disconnect first to prevent hanging
	// This is crucial - we need to close the Jack client before killing the server
	DisconnectFromJack();
	
	// Give the disconnect a moment to complete
	FPlatformProcess::Sleep(0.1f);
	
	bool bResult = JackClient.KillJackServer();
	
	UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Jack server kill result: %s"), bResult ? TEXT("SUCCESS") : TEXT("FAILED"));
	
	// Don't re-enable monitoring automatically - let user manually restart if needed
	// This prevents the system from getting stuck in a hanging loop
	
	return bResult;
}

bool AJackAudioInterface::StartJackServer(const FString& JackdPath)
{
	return JackClient.StartJackServer(JackdPath);
}

bool AJackAudioInterface::EnsureJackServerRunning(const FString& JackdPath)
{
	return JackClient.EnsureJackServerRunning(JackdPath);
}

bool AJackAudioInterface::StartJackServerWithParameters(const FString& JackdPath, int32 InSampleRate, int32 InBufferSize, const FString& InDriver)
{
	return JackClient.StartJackServerWithParameters(JackdPath, InSampleRate, InBufferSize, InDriver);
}

FString AJackAudioInterface::GetJackClientName() const
{
	return JackClient.GetClientName();
}

int32 AJackAudioInterface::GetJackSampleRate() const
{
	return JackClient.GetSampleRate();
}

int32 AJackAudioInterface::GetJackBufferSize() const
{
	return JackClient.GetBufferSize();
}

float AJackAudioInterface::GetJackCPUUsage() const
{
	return JackClient.GetCPUUsage();
}

TArray<FString> AJackAudioInterface::GetAvailableJackPorts()
{
	return JackClient.GetAvailablePorts();
}

bool AJackAudioInterface::ConnectJackPorts(const FString& SourcePort, const FString& DestinationPort)
{
	return JackClient.ConnectPorts(SourcePort, DestinationPort);
}

bool AJackAudioInterface::DisconnectJackPorts(const FString& SourcePort, const FString& DestinationPort)
{
	return JackClient.DisconnectPorts(SourcePort, DestinationPort);
}

bool AJackAudioInterface::RegisterAudioPorts(int32 NumInputs, int32 NumOutputs, const FString& BaseName)
{
	return JackClient.RegisterAudioPorts(NumInputs, NumOutputs, BaseName);
}

void AJackAudioInterface::UnregisterAllAudioPorts()
{
	JackClient.UnregisterAllPorts();
}

int32 AJackAudioInterface::GetNumRegisteredInputPorts() const
{
	return JackClient.GetInputPorts().Num();
}

int32 AJackAudioInterface::GetNumRegisteredOutputPorts() const
{
	return JackClient.GetOutputPorts().Num();
}

FJackPerformanceMetrics AJackAudioInterface::GetJackPerformanceMetrics() const
{
	return JackClient.GetPerformanceMetrics();
}

EJackConnectionStatus AJackAudioInterface::GetJackConnectionStatus() const
{
	return JackClient.GetConnectionStatus();
}

bool AJackAudioInterface::WasJackServerStarted() const
{
	return JackClient.WasServerStarted();
}

FString AJackAudioInterface::GetJackServerInfo() const
{
	return JackClient.GetServerInfo();
}

void AJackAudioInterface::CheckServerStatus()
{
	// Get current status
	EJackConnectionStatus CurrentStatus = JackClient.GetConnectionStatus();
	bool bClientConnected = (CurrentStatus == EJackConnectionStatus::Connected);
	
	// Track connection state changes for logging
	if (bClientConnected != bWasConnected)
	{
		if (bClientConnected)
		{
			UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Connected to Jack server"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("JackAudioInterface: Disconnected from Jack server"));
		}
		bWasConnected = bClientConnected;
	}
	
	// If we're not connected, try to reconnect automatically
	if (!bClientConnected && bAutoStartServer)
	{
		// Check if server is running
		bool bServerRunning = CheckJackServerRunning();
		
		if (bServerRunning)
		{
			// Server is running but we're not connected - try to connect
			UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Server is running, attempting to reconnect..."));
			if (ConnectToJack())
			{
				// Re-register ports after successful reconnection
				if (RegisterAudioPorts(64, 64, TEXT("unreal")))
				{
					UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Successfully re-registered audio ports"));
				}
			}
		}
		else
		{
			// Server is not running - restart it automatically
			UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Server not running, restarting automatically..."));
			if (StartJackServerWithParameters(JackServerPath, SampleRate, BufferSize, AudioDriver))
			{
				UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Server restarted, attempting to connect..."));
				// Give the server a moment to start
				FPlatformProcess::Sleep(2.0f);
				if (ConnectToJack())
				{
					// Register ports after successful connection
					if (RegisterAudioPorts(64, 64, TEXT("unreal")))
					{
						UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Successfully registered audio ports after restart"));
					}
				}
			}
		}
	}
}

void AJackAudioInterface::OnServerStatusCheck()
{
	CheckServerStatus();
}

void AJackAudioInterface::StartServerMonitoring()
{
	if (!bMonitorServerStatus)
	{
		bMonitorServerStatus = true;
		
		// Use a longer interval to reduce the chance of hanging
		float SafeCheckInterval = FMath::Max(ServerCheckInterval, 2.0f);
		GetWorld()->GetTimerManager().SetTimer(ServerStatusTimerHandle, this, &AJackAudioInterface::OnServerStatusCheck, SafeCheckInterval, true);
		UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Started server monitoring (checking every %.1f seconds)"), SafeCheckInterval);
	}
}

void AJackAudioInterface::StopServerMonitoring()
{
	if (bMonitorServerStatus)
	{
		bMonitorServerStatus = false;
		if (ServerStatusTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().ClearTimer(ServerStatusTimerHandle);
		}
		UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Stopped server monitoring"));
	}
}

float AJackAudioInterface::GetInputChannelLevel(int32 ChannelIndex) const
{
	if (IsConnectedToJack())
	{
		return JackClient.GetInputLevel(ChannelIndex);
	}
	return 0.0f;
}

void AJackAudioInterface::MonitorNewClients()
{
	if (!IsConnectedToJack())
	{
		return;
	}

	// Get the current list of clients from the server
	TArray<FString> CurrentClientArray = JackClient.GetAllClients();
	TSet<FString> CurrentClients(CurrentClientArray);

	// Check for newly connected clients
	TSet<FString> NewClients = CurrentClients.Difference(KnownClients);
	for (const FString& ClientName : NewClients)
	{
		UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Client connected -> %s"), *ClientName);

		// If auto-connect is enabled, establish the connections
		if (bAutoConnectToNewClients)
		{
			// Ignore the system client for auto-connection
			if (ClientName.Equals(TEXT("system"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Ignoring 'system' client for auto-connection."));
				continue;
			}
			
			// Use a small delay to ensure the client has fully initialized its ports
			FTimerHandle TempTimer;
			FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &AJackAudioInterface::AutoConnectToClient, ClientName);
			GetWorld()->GetTimerManager().SetTimer(TempTimer, TimerDelegate, 0.5f, false);
		}
	}

	// Check for disconnected clients
	TSet<FString> DisconnectedClients = KnownClients.Difference(CurrentClients);
	for (const FString& ClientName : DisconnectedClients)
	{
		UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Client disconnected -> %s"), *ClientName);
	}
	
	// Update the set of known clients for the next check
	KnownClients = CurrentClients;
}

void AJackAudioInterface::AutoConnectToClient(FString ClientName)
{
	if (!IsConnectedToJack())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Attempting to auto-connect to '%s'..."), *ClientName);

	// Get the output ports from the target client that are of the standard audio type
	const FString NamePattern = FString::Printf(TEXT("%s:*"), *ClientName);
	TArray<FString> ClientOutputPorts = JackClient.GetAvailablePorts(NamePattern, TEXT(JACK_DEFAULT_AUDIO_TYPE), JackPortIsOutput);

	if (ClientOutputPorts.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("JackAudioInterface: No compatible audio output ports found on '%s'. Ignoring."), *ClientName);
		return;
	}

	// Get our own client's input ports
	TArray<jack_port_t*> OurInputPorts = JackClient.GetInputPorts();
	if (OurInputPorts.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("JackAudioInterface: We have no registered input ports to connect to."));
		return;
	}

	// Determine the number of connections to make (the minimum of the two port counts)
	const int32 NumConnectionsToMake = FMath::Min(ClientOutputPorts.Num(), OurInputPorts.Num());
	UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Found %d output ports on '%s' and %d input ports on our client. Connecting %d port(s)."), ClientOutputPorts.Num(), *ClientName, OurInputPorts.Num(), NumConnectionsToMake);

	for (int32 i = 0; i < NumConnectionsToMake; ++i)
	{
		const FString& SourcePort = ClientOutputPorts[i];
		const FString DestinationPort = JackClient.GetPortFullName(OurInputPorts[i]);

		if (SourcePort.IsEmpty() || DestinationPort.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("JackAudioInterface: Could not get full port name for connection index %d. Skipping."), i);
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: Connecting '%s' -> '%s'..."), *SourcePort, *DestinationPort);
		if (ConnectJackPorts(SourcePort, DestinationPort))
		{
			UE_LOG(LogTemp, Log, TEXT("JackAudioInterface: ...Connection successful."));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JackAudioInterface: ...Connection FAILED."));
		}
	}
}

