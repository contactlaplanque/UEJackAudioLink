#include "JackClient.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Math/UnrealMathUtility.h"

FJackClient::FJackClient()
	: JackClient(nullptr)
	, ConnectionStatus(EJackConnectionStatus::Disconnected)
	, bServerWasStarted(false)
{
}

FJackClient::~FJackClient()
{
	UnregisterAllPorts();
	Disconnect();
}

bool FJackClient::CheckJackServerRunning()
{
	// Try to connect to an existing Jack server to check if one is running
	// Use a timeout to prevent hanging
	jack_status_t Status;
	jack_client_t* TestClient = jack_client_open("test_connection", JackNullOption, &Status);
	
	if (TestClient != nullptr)
	{
		jack_client_close(TestClient);
		return true;
	}
	else
	{
		// Check if the failure is due to server not running vs other errors
		// If it's a server not running error, that's expected
		if (Status & JackServerFailed)
		{
			return false;
		}
		else
		{
			// For other errors, assume server might be running but having issues
			// This prevents false negatives that could cause hanging
			return true;
		}
	}
}

bool FJackClient::KillJackServer()
{
	UE_LOG(LogTemp, Log, TEXT("JackClient: Attempting to kill existing Jack server..."));
	
	// Use cmd to execute taskkill to terminate any running jackd processes
	FString Command = TEXT("cmd");
	FString Params = TEXT("/c taskkill /f /im jackd.exe");
	FString Output;
	FString ErrorOutput;
	int32 ReturnCode = 0;
	
	bool bSuccess = FPlatformProcess::ExecProcess(*Command, *Params, &ReturnCode, &Output, &ErrorOutput);
	
	if (bSuccess && ReturnCode == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("JackClient: Successfully killed existing Jack server"));
		// Give the process a moment to fully terminate
		FPlatformProcess::Sleep(0.5f);
		return true;
	}
	else if (ReturnCode == 128)
	{
		// Return code 128 means "no processes found" - this is normal if no server was running
		UE_LOG(LogTemp, Log, TEXT("JackClient: No Jack server was running to kill"));
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("JackClient: Failed to kill Jack server. Return code: %d"), ReturnCode);
		return false;
	}
}

bool FJackClient::StartJackServer(const FString& JackdPath)
{
	UE_LOG(LogTemp, Log, TEXT("JackClient: Starting Jack server with custom parameters..."));
	
	// Check if jackd.exe exists at the specified path
	if (!FPaths::FileExists(JackdPath))
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Jack server executable not found at: %s"), *JackdPath);
		return false;
	}
	
	// Build the command parameters
	FString Params = TEXT("-S -X winmme -d portaudio -r 48000 -p 512");
	
	// Start the Jack server process
	FString Output;
	int32 ReturnCode = 0;
	
	// Start the process in the background
	FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*JackdPath, *Params, true, false, false, nullptr, 0, nullptr, nullptr);
	
	if (ProcessHandle.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("JackClient: Jack server process started successfully"));
		
		// Give the server a moment to start up
		FPlatformProcess::Sleep(2.0f);
		
		// Verify the server is running
		if (CheckJackServerRunning())
		{
			UE_LOG(LogTemp, Log, TEXT("JackClient: Jack server is now running and ready for connections"));
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JackClient: Jack server process started but server is not responding"));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to start Jack server process"));
		return false;
	}
}

bool FJackClient::StartJackServerWithParameters(const FString& JackdPath, int32 SampleRate, int32 BufferSize, const FString& Driver)
{
	UE_LOG(LogTemp, Log, TEXT("JackClient: Starting Jack server with custom parameters - Sample Rate: %d, Buffer Size: %d, Driver: %s"), SampleRate, BufferSize, *Driver);
	
	// Check if jackd.exe exists at the specified path
	if (!FPaths::FileExists(JackdPath))
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Jack server executable not found at: %s"), *JackdPath);
		return false;
	}
	
	// Build the command parameters
	FString Params = FString::Printf(TEXT("-S -X winmme -d %s -r %d -p %d"), *Driver, SampleRate, BufferSize);
	
	// Start the Jack server process
	FString Output;
	int32 ReturnCode = 0;
	
	// Start the process in the background
	FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*JackdPath, *Params, true, false, false, nullptr, 0, nullptr, nullptr);
	
	if (ProcessHandle.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("JackClient: Jack server process started successfully with custom parameters"));
		
		// Give the server a moment to start up
		FPlatformProcess::Sleep(2.0f);
		
		// Verify the server is running
		if (CheckJackServerRunning())
		{
			UE_LOG(LogTemp, Log, TEXT("JackClient: Jack server is now running and ready for connections"));
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JackClient: Jack server process started but server is not responding"));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to start Jack server process"));
		return false;
	}
}

bool FJackClient::EnsureJackServerRunning(const FString& JackdPath)
{
	UE_LOG(LogTemp, Log, TEXT("JackClient: Ensuring Jack server is running with custom parameters..."));
	
	// Check if a server is already running
	if (CheckJackServerRunning())
	{
		UE_LOG(LogTemp, Log, TEXT("JackClient: Found existing Jack server, killing it to restart with custom parameters"));
		KillJackServer();
		
		// Give it a moment to fully shut down
		FPlatformProcess::Sleep(1.0f);
	}
	
	// Start the server with our custom parameters (using the default parameters from StartJackServer)
	return StartJackServer(JackdPath);
}

bool FJackClient::Connect(const FString& ClientName)
{
	if (IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("JackClient: Already connected"));
		return true;
	}

	ConnectionStatus = EJackConnectionStatus::Connecting;

	// Check if Jack server is running (don't restart if it's already running)
	if (!CheckJackServerRunning())
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Jack server is not running"));
		ConnectionStatus = EJackConnectionStatus::Failed;
		return false;
	}

	// Convert FString to C string
	FTCHARToUTF8 Convert(*ClientName);
	const char* ClientNameStr = Convert.Get();

	// Open connection to Jack server
	jack_status_t Status;
	JackClient = jack_client_open(ClientNameStr, JackNullOption, &Status);

	if (JackClient == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to connect to Jack server. Status: 0x%x"), Status);
		ConnectionStatus = EJackConnectionStatus::Failed;
		return false;
	}

	// Since we started the server ourselves, mark it as started by this client
	bServerWasStarted = true;
	UE_LOG(LogTemp, Log, TEXT("JackClient: Connected to Jack server (started by this client with custom parameters)"));

	if (Status & JackNameNotUnique)
	{
		FString ActualName = FString(jack_get_client_name(JackClient));
		UE_LOG(LogTemp, Log, TEXT("JackClient: Unique name assigned: %s"), *ActualName);
	}

	// Set up callbacks
	jack_on_shutdown(JackClient, ShutdownCallback, this);
	jack_set_process_callback(JackClient, FJackClient::ProcessCallback, this);
	jack_set_xrun_callback(JackClient, XRunCallback, this);

	ConnectionStatus = EJackConnectionStatus::Connected;
	UE_LOG(LogTemp, Log, TEXT("JackClient: Successfully connected to Jack server"));
	UE_LOG(LogTemp, Log, TEXT("JackClient: Sample rate: %d, Buffer size: %d"), GetSampleRate(), GetBufferSize());

	return true;
}

void FJackClient::Disconnect()
{
	if (JackClient != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("JackClient: Closing Jack client connection..."));
		
		// Deactivate first to stop audio processing
		Deactivate();
		
		// Close the client connection
		jack_client_close(JackClient);
		JackClient = nullptr;
		ConnectionStatus = EJackConnectionStatus::Disconnected;
		
		UE_LOG(LogTemp, Log, TEXT("JackClient: Successfully disconnected from Jack server"));
	}
}

bool FJackClient::IsConnected() const
{
	// Only check if we have a valid client and are in connected status
	// Don't do additional API calls that might hang when server is dead
	return (JackClient != nullptr && ConnectionStatus == EJackConnectionStatus::Connected);
}

FString FJackClient::GetClientName() const
{
	if (JackClient != nullptr && ConnectionStatus == EJackConnectionStatus::Connected)
	{
		const char* ClientName = jack_get_client_name(JackClient);
		if (ClientName != nullptr)
		{
			return FString(ClientName);
		}
	}
	return TEXT("");
}

uint32 FJackClient::GetSampleRate() const
{
	if (JackClient != nullptr && ConnectionStatus == EJackConnectionStatus::Connected)
	{
		jack_nframes_t SampleRate = jack_get_sample_rate(JackClient);
		if (SampleRate > 0)
		{
			return SampleRate;
		}
	}
	return 0;
}

uint32 FJackClient::GetBufferSize() const
{
	if (JackClient != nullptr && ConnectionStatus == EJackConnectionStatus::Connected)
	{
		jack_nframes_t BufferSize = jack_get_buffer_size(JackClient);
		if (BufferSize > 0)
		{
			return BufferSize;
		}
	}
	return 0;
}

float FJackClient::GetCPUUsage() const
{
	if (JackClient != nullptr && ConnectionStatus == EJackConnectionStatus::Connected)
	{
		float CPUUsage = jack_cpu_load(JackClient);
		if (CPUUsage >= 0.0f)
		{
			return CPUUsage;
		}
	}
	return 0.0f;
}

jack_port_t* FJackClient::RegisterPort(const FString& PortName, const FString& PortType, uint32 Flags, uint32 BufferSize)
{
	if (!IsConnected())
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Cannot register port - not connected"));
		return nullptr;
	}

	FTCHARToUTF8 PortNameConvert(*PortName);
	FTCHARToUTF8 PortTypeConvert(*PortType);

	jack_port_t* Port = jack_port_register(JackClient, PortNameConvert.Get(), PortTypeConvert.Get(), Flags, BufferSize);
	
	if (Port == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to register port: %s"), *PortName);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("JackClient: Successfully registered port: %s"), *PortName);
	}

	return Port;
}

bool FJackClient::UnregisterPort(jack_port_t* Port)
{
	if (!IsConnected() || Port == nullptr)
	{
		return false;
	}

	int Result = jack_port_unregister(JackClient, Port);
	if (Result != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to unregister port"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("JackClient: Successfully unregistered port"));
	return true;
}

TArray<FString> FJackClient::GetAvailablePorts(const FString& NamePattern, const FString& TypePattern, uint32 Flags)
{
	TArray<FString> Ports;

	if (!IsConnected())
	{
		return Ports;
	}

	FTCHARToUTF8 NamePatternConvert(*NamePattern);
	FTCHARToUTF8 TypePatternConvert(*TypePattern);

	const char** JackPorts = jack_get_ports(JackClient, 
		NamePattern.IsEmpty() ? nullptr : NamePatternConvert.Get(),
		TypePattern.IsEmpty() ? nullptr : TypePatternConvert.Get(),
		Flags);

	if (JackPorts != nullptr)
	{
		for (int i = 0; JackPorts[i] != nullptr; i++)
		{
			Ports.Add(FString(JackPorts[i]));
		}
		jack_free(JackPorts);
	}

	return Ports;
}

bool FJackClient::ConnectPorts(const FString& SourcePort, const FString& DestinationPort)
{
	if (!IsConnected())
	{
		return false;
	}

	FTCHARToUTF8 SourceConvert(*SourcePort);
	FTCHARToUTF8 DestConvert(*DestinationPort);

	int Result = jack_connect(JackClient, SourceConvert.Get(), DestConvert.Get());
	if (Result != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to connect ports %s -> %s"), *SourcePort, *DestinationPort);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("JackClient: Connected ports %s -> %s"), *SourcePort, *DestinationPort);
	return true;
}

bool FJackClient::DisconnectPorts(const FString& SourcePort, const FString& DestinationPort)
{
	if (!IsConnected())
	{
		return false;
	}

	FTCHARToUTF8 SourceConvert(*SourcePort);
	FTCHARToUTF8 DestConvert(*DestinationPort);

	int Result = jack_disconnect(JackClient, SourceConvert.Get(), DestConvert.Get());
	if (Result != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to disconnect ports %s -> %s"), *SourcePort, *DestinationPort);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("JackClient: Disconnected ports %s -> %s"), *SourcePort, *DestinationPort);
	return true;
}

bool FJackClient::SetProcessCallback(JackProcessCallback ProcessCallback, void* Arg)
{
	if (!IsConnected())
	{
		return false;
	}

	int Result = jack_set_process_callback(JackClient, ProcessCallback, Arg);
	if (Result != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to set process callback"));
		return false;
	}

	return true;
}

bool FJackClient::SetShutdownCallback(JackShutdownCallback ShutdownCallback, void* Arg)
{
	if (!IsConnected())
	{
		return false;
	}

	jack_on_shutdown(JackClient, ShutdownCallback, Arg);
	return true;
}

bool FJackClient::SetXRunCallback(JackXRunCallback XRunCallback, void* Arg)
{
	if (!IsConnected())
	{
		return false;
	}

	int Result = jack_set_xrun_callback(JackClient, XRunCallback, Arg);
	if (Result != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to set xrun callback"));
		return false;
	}

	return true;
}

bool FJackClient::Activate()
{
	if (!IsConnected())
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Cannot activate - not connected"));
		return false;
	}

	int Result = jack_activate(JackClient);
	if (Result != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to activate client"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("JackClient: Client activated"));
	return true;
}

bool FJackClient::Deactivate()
{
	if (JackClient == nullptr)
	{
		return false;
	}

	// Don't check IsConnected() here as it might call API functions that hang
	// Just try to deactivate if we have a client pointer
	int Result = jack_deactivate(JackClient);
	if (Result != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("JackClient: Failed to deactivate client (server may be dead)"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("JackClient: Client deactivated"));
	return true;
}

FJackPerformanceMetrics FJackClient::GetPerformanceMetrics() const
{
	FJackPerformanceMetrics Metrics = PerformanceMetrics;
	
	// Update real-time values
	if (IsConnected())
	{
		Metrics.CPUUsage = GetCPUUsage();
		// Note: XRuns and other metrics are updated in callbacks
	}
	
	return Metrics;
}

// Static callback implementations
int FJackClient::ProcessCallback(jack_nframes_t Nframes, void* Arg)
{
	FJackClient* Client = static_cast<FJackClient*>(Arg);
	if (Client)
	{
		Client->UpdatePerformanceMetrics();
		Client->ComputeInputLevels(Nframes);
	}
	return 0;
}

void FJackClient::ShutdownCallback(void* Arg)
{
	FJackClient* Client = static_cast<FJackClient*>(Arg);
	if (Client)
	{
		Client->ConnectionStatus = EJackConnectionStatus::Failed;
		Client->JackClient = nullptr; // Clear the client pointer
		UE_LOG(LogTemp, Warning, TEXT("JackClient: Jack server shutdown detected"));
	}
}

int FJackClient::XRunCallback(void* Arg)
{
	FJackClient* Client = static_cast<FJackClient*>(Arg);
	if (Client)
	{
		Client->PerformanceMetrics.XRuns++;
		UE_LOG(LogTemp, Warning, TEXT("JackClient: XRun detected! Total XRuns: %d"), Client->PerformanceMetrics.XRuns);
	}
	return 0;
}

void FJackClient::UpdatePerformanceMetrics()
{
	// Update performance metrics in real-time callback
	// This is called from the Jack audio thread, so keep it minimal
	PerformanceMetrics.AudioCallbackTime = FPlatformTime::Seconds() * 1000.0f; // Convert to milliseconds
}

bool FJackClient::WasServerStarted() const
{
	return bServerWasStarted;
}

FString FJackClient::GetServerInfo() const
{
	if (!IsConnected())
	{
		return TEXT("Not connected to Jack server");
	}

	FString Info = FString::Printf(TEXT("Jack Server Info:\n"));
	Info += FString::Printf(TEXT("  Server Started by this client: %s\n"), bServerWasStarted ? TEXT("Yes") : TEXT("No"));
	Info += FString::Printf(TEXT("  Sample Rate: %d Hz\n"), GetSampleRate());
	Info += FString::Printf(TEXT("  Buffer Size: %d frames\n"), GetBufferSize());
	Info += FString::Printf(TEXT("  CPU Usage: %.2f%%\n"), GetCPUUsage());
	
	// Calculate latency
	float LatencyMs = (float)GetBufferSize() * 1000.0f / (float)GetSampleRate();
	Info += FString::Printf(TEXT("  Latency: %.2f ms\n"), LatencyMs);
	
	return Info;
}

// ================== Audio Level Helpers ==================

void FJackClient::ComputeInputLevels(jack_nframes_t Nframes)
{
	// Ensure InputLevels array matches number of ports
	if (InputLevels.Num() != InputPorts.Num())
	{
		InputLevels.SetNumZeroed(InputPorts.Num());
	}

	for (int32 PortIdx = 0; PortIdx < InputPorts.Num(); ++PortIdx)
	{
		jack_port_t* Port = InputPorts[PortIdx];
		if (!Port)
		{
			InputLevels[PortIdx] = 0.0f;
			continue;
		}

		jack_default_audio_sample_t* Buffer = static_cast<jack_default_audio_sample_t*>(jack_port_get_buffer(Port, Nframes));
		if (!Buffer)
		{
			InputLevels[PortIdx] = 0.0f;
			continue;
		}

		double SumSq = 0.0;
		for (uint32 Frame = 0; Frame < Nframes; ++Frame)
		{
			float Sample = Buffer[Frame];
			SumSq += static_cast<double>(Sample) * static_cast<double>(Sample);
		}

		float RMS = 0.0f;
		if (Nframes > 0)
		{
			RMS = FMath::Sqrt(static_cast<float>(SumSq / static_cast<double>(Nframes)));
		}

		InputLevels[PortIdx] = RMS;
	}
}

float FJackClient::GetInputLevel(int32 ChannelIndex) const
{
	if (InputLevels.IsValidIndex(ChannelIndex))
	{
		return InputLevels[ChannelIndex];
	}
	return 0.0f;
}

bool FJackClient::RegisterAudioPorts(int32 NumInputs, int32 NumOutputs, const FString& BaseName)
{
	if (!IsConnected())
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Cannot register ports - not connected"));
		return false;
	}

	// Clear existing ports
	UnregisterAllPorts();

	bool bSuccess = true;

	// Register input ports
	for (int32 i = 0; i < NumInputs; i++)
	{
		FString PortName = FString::Printf(TEXT("%s_in_%d"), *BaseName, i + 1);
		jack_port_t* Port = RegisterPort(PortName, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
		
		if (Port != nullptr)
		{
			InputPorts.Add(Port);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to register input port %d"), i + 1);
			bSuccess = false;
		}
	}

	// Register output ports
	for (int32 i = 0; i < NumOutputs; i++)
	{
		FString PortName = FString::Printf(TEXT("%s_out_%d"), *BaseName, i + 1);
		jack_port_t* Port = RegisterPort(PortName, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
		
		if (Port != nullptr)
		{
			OutputPorts.Add(Port);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to register output port %d"), i + 1);
			bSuccess = false;
		}
	}

	// Initialize level arrays
	InputLevels.SetNumZeroed(NumInputs);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("JackClient: Successfully registered %d input and %d output ports"), NumInputs, NumOutputs);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("JackClient: Failed to register some ports"));
	}

	return bSuccess;
}

void FJackClient::UnregisterAllPorts()
{
	// Unregister input ports
	for (jack_port_t* Port : InputPorts)
	{
		if (Port != nullptr)
		{
			UnregisterPort(Port);
		}
	}
	InputPorts.Empty();

	// Unregister output ports
	for (jack_port_t* Port : OutputPorts)
	{
		if (Port != nullptr)
		{
			UnregisterPort(Port);
		}
	}
	OutputPorts.Empty();

	// Clear cached levels
	InputLevels.Empty();

	UE_LOG(LogTemp, Log, TEXT("JackClient: Unregistered all ports"));
}

TArray<jack_port_t*> FJackClient::GetInputPorts() const
{
	return InputPorts;
}

TArray<jack_port_t*> FJackClient::GetOutputPorts() const
{
	return OutputPorts;
}

TArray<FString> FJackClient::GetAllClients() const
{
	TArray<FString> ClientNames;
	if (!IsConnected())
	{
		return ClientNames;
	}

	// Use a TSet to store unique client names
	TSet<FString> UniqueClientNames;

	// Get all ports on the server
	const char** Ports = jack_get_ports(JackClient, nullptr, nullptr, 0);
	if (Ports)
	{
		for (int i = 0; Ports[i] != nullptr; i++)
		{
			FString PortName(Ports[i]);
			FString ClientName;
			if (PortName.Split(TEXT(":"), &ClientName, nullptr))
			{
				UniqueClientNames.Add(ClientName);
			}
		}
		// Free the memory allocated by jack_get_ports
		jack_free(Ports);
	}

	// Exclude our own client name
	FString OwnName = GetClientName();
	if(OwnName.Contains(TEXT(":")))
	{
		OwnName.Split(TEXT(":"), &OwnName, nullptr);
	}
	UniqueClientNames.Remove(OwnName);


	return UniqueClientNames.Array();
} 

FString FJackClient::GetPortFullName(jack_port_t* Port) const
{
	if (!IsConnected() || !Port)
	{
		return TEXT("");
	}
	
	const char* PortName = jack_port_name(Port);
	return FString(PortName ? PortName : "");
} 