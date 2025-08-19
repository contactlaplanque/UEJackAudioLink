// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEJackAudioLink.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "TimerManager.h"
#include "LevelEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Containers/Ticker.h"
#include "UEJackAudioLinkLog.h"
#include "JackAudioLinkSettings.h"
#include "ISettingsModule.h"
#include "JackServerController.h"
#include "JackServerMonitor.h"
#include "JackClientManager.h"
#include "UEJackAudioLinkSubsystem.h"

#define LOCTEXT_NAMESPACE "FUEJackAudioLinkModule"

DEFINE_LOG_CATEGORY(LogJackAudioLink);

static const FName UEJackAudioLinkTabName("UEJackAudioLinkTab");
static TSharedPtr<FWorkspaceItem> JackWorkspaceMenuCategory;

void FUEJackAudioLinkModule::StartupModule()
{
	// Register tab spawner
	JackWorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("JackMenuGroup", "Jack Audio Link"));
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UEJackAudioLinkTabName, FOnSpawnTab::CreateRaw(this, &FUEJackAudioLinkModule::OnSpawnPluginTab))
	    .SetDisplayName(LOCTEXT("UEJackAudioLinkTabTitle", "Jack Audio Link"))
	    .SetMenuType(ETabSpawnerMenuType::Enabled)
	    .SetGroup(JackWorkspaceMenuCategory.ToSharedRef());

	// Automatically invoke the tab when in editor
	if (GIsEditor)
	{
	    FGlobalTabmanager::Get()->TryInvokeTab(UEJackAudioLinkTabName);
	}

	// Register settings in Project Settings → Plugins → Jack Audio Link
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
	    SettingsModule->RegisterSettings("Project", "Plugins", "Jack Audio Link",
	        LOCTEXT("JackSettingsName", "Jack Audio Link"),
	        LOCTEXT("JackSettingsDesc", "Configure JACK Audio Link plugin"),
	        GetMutableDefault<UJackAudioLinkSettings>());
	}

	// Log plugin load details to help diagnose duplicate/old copies being loaded
	if (IPluginManager::Get().FindPlugin(TEXT("UEJackAudioLink")).IsValid())
	{
		const FString BaseDir = IPluginManager::Get().FindPlugin(TEXT("UEJackAudioLink"))->GetBaseDir();
		UE_LOG(LogJackAudioLink, Display, TEXT("UEJackAudioLink Startup. BaseDir=%s, Built=%s %s, WITH_JACK=%d"), *BaseDir, TEXT(__DATE__), TEXT(__TIME__), WITH_JACK);
	}

	// Optionally auto-start server and connect client
#if WITH_JACK
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	if (Settings && Settings->bAutoStartServer)
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Auto-start enabled, enforcing single JACK instance..."));
		// Force stop any existing JACK server (external or plugin-owned) to ensure a clean start
		FJackServerController::Get().StopAnyServer();
		// Wait briefly until no server is available to avoid race with process teardown
		for (int i = 0; i < 30; ++i) // up to ~3s
		{
			if (!FJackServerController::Get().IsAnyServerAvailable()) { break; }
			FPlatformProcess::Sleep(0.1f);
		}

		UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Checking JACK server..."));
		// Check if any server is running (even from external apps like QJackCtl)
		bool bNeedsRestart = false;
		if (FJackServerController::Get().IsAnyServerAvailable())
		{
			UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Found existing JACK server, checking settings..."));
			// Server exists, check if settings match
			int32 CurrentSR = 0, CurrentBS = 0;
			if (FJackServerController::Get().GetServerAudioConfig(CurrentSR, CurrentBS))
			{
				if (CurrentSR != Settings->SampleRate || CurrentBS != Settings->BufferSize)
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK server running with wrong settings (%d Hz, %d frames). Expected: %d Hz, %d frames. Restarting..."), 
						CurrentSR, CurrentBS, Settings->SampleRate, Settings->BufferSize);
					bNeedsRestart = true;
				}
				else
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Existing JACK server has correct settings (%d Hz, %d frames)"), CurrentSR, CurrentBS);
				}
			}
		}
		else
		{
			UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: No JACK server found, will start new one"));
		}
		
		auto WaitForConfigMatch = [Settings]() -> bool
		{
			// Wait up to ~3s for server to report SR/BS and match settings
			for (int i = 0; i < 30; ++i)
			{
				int32 SR=0, BS=0;
				if (FJackServerController::Get().GetServerAudioConfig(SR, BS))
				{
					if (SR == Settings->SampleRate && BS == Settings->BufferSize)
					{
						return true;
					}
				}
				FPlatformProcess::Sleep(0.1f);
			}
			return false;
		};

		if (bNeedsRestart)
		{
			// Use RestartServer to ensure correct settings
			UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Restarting JACK server with correct settings..."));
			if (FJackServerController::Get().RestartServer(Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver, Settings->JackdPath))
			{
				if (WaitForConfigMatch())
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK server restarted with desired config"));
				}
				else
				{
					UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Server config mismatch after restart; retrying once"));
					FJackServerController::Get().StopAnyServer();
					FPlatformProcess::Sleep(0.2f);
					FJackServerController::Get().StartServer(Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver, Settings->JackdPath);
					WaitForConfigMatch();
				}
			}
			else
			{
				UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Failed to restart JACK server"));
			}
		}
		else
		{
			// No server running or settings are correct, start normally
			UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Starting JACK server..."));
			if (FJackServerController::Get().StartServer(Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver, Settings->JackdPath))
			{
				if (WaitForConfigMatch())
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK server started with desired config"));
				}
				else
				{
					UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Server config mismatch on first start; restarting with desired config"));
					FJackServerController::Get().StopAnyServer();
					FPlatformProcess::Sleep(0.2f);
					FJackServerController::Get().StartServer(Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver, Settings->JackdPath);
					WaitForConfigMatch();
				}
			}
			else
			{
				UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Failed to start JACK server"));
			}
		}
		
		FString ClientName = Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName;
		UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Connecting JACK client: %s"), *ClientName);
		
		if (FJackClientManager::Get().Connect(ClientName))
		{
			UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK client connected successfully"));
			if (FJackClientManager::Get().RegisterAudioPorts(Settings->InputChannels, Settings->OutputChannels, TEXT("unreal")))
			{
				UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Audio ports registered: %d inputs, %d outputs"), Settings->InputChannels, Settings->OutputChannels);
				if (FJackClientManager::Get().Activate())
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK client activated successfully"));
					if (GEngine && GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
					{
						GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>()->StartAutoConnect(Settings->ClientMonitorInterval, Settings->bAutoConnectToNewClients);
						UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Auto-connect started (enabled: %s, interval: %.1fs)"), 
							Settings->bAutoConnectToNewClients ? TEXT("yes") : TEXT("no"), Settings->ClientMonitorInterval);
					}
				}
				else
				{
					UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Failed to activate JACK client"));
				}
			}
			else
			{
				UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Failed to register audio ports"));
			}
		}
		else
		{
			UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Failed to connect JACK client"));
		}
	}
	else
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Auto-start disabled"));
	}

	// Start hybrid server monitor (sentinel + 1s probe)
	FJackServerMonitor::Get().Start();
#endif

	// Start periodic status updates using FTSTicker (avoids TimerManager lifetime issues)
	if (!StatusUpdateTickHandle.IsValid())
	{
		StatusUpdateTickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FUEJackAudioLinkModule::TickStatusUpdate), 1.0f);
	}
}

void FUEJackAudioLinkModule::ShutdownModule()
{
	// Remove ticker
	if (StatusUpdateTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(StatusUpdateTickHandle);
		StatusUpdateTickHandle.Reset();
	}

	// Unregister tab spawner
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UEJackAudioLinkTabName);

	// Unregister settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
	    SettingsModule->UnregisterSettings("Project", "Plugins", "Jack Audio Link");
	}

	// Stop server on shutdown only if owned (policy set in settings)
#if WITH_JACK
	// Stop hybrid monitor
	FJackServerMonitor::Get().Stop();
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	if (Settings && Settings->bKillServerOnShutdown && FJackServerController::Get().WasServerStartedByPlugin())
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("Plugin shutdown: Stopping JACK server..."));
		FJackServerController::Get().StopServer();
		UE_LOG(LogJackAudioLink, Display, TEXT("Plugin shutdown: JACK server stopped"));
	}
#endif
}

TSharedRef<SDockTab> FUEJackAudioLinkModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
	    .TabRole(ETabRole::NomadTab)
	    [
	        SNew(SVerticalBox)
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SAssignNew(StatusTextBlock, STextBlock)
	            .Text_Lambda([this]() { return GetStatusText(); })
	            .AutoWrapText(true)
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SHorizontalBox)
	            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
	            [
	                SNew(STextBlock)
	                .Text_Lambda([this]()
	                {
	                    FString Msg; return IsRestartRequired(Msg) ? FText::FromString(Msg) : FText();
	                })
	                .ColorAndOpacity(FLinearColor(1.f, 0.2f, 0.2f))
	            ]
	            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
	            [
	                SNew(SButton)
	                .Visibility_Lambda([this]() { FString M; return IsRestartRequired(M) ? EVisibility::Visible : EVisibility::Collapsed; })
	                .Text(LOCTEXT("ApplyRestartButton", "Apply & Restart"))
	                .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnApplyRestartClicked)
	            ]
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SHorizontalBox)
	            + SHorizontalBox::Slot().AutoWidth().Padding(4)
	            [
	                SNew(SButton)
	                .Text(LOCTEXT("StartServerButton", "Start Server"))
	                .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnStartServerClicked)
	            ]
	            + SHorizontalBox::Slot().AutoWidth().Padding(4)
	            [
	                SNew(SButton)
	                .Text(LOCTEXT("StopServerButton", "Stop Server"))
	                .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnStopServerClicked)
	            ]
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SHorizontalBox)
	            + SHorizontalBox::Slot().AutoWidth().Padding(4)
	            [
	                SNew(SButton)
	                .Text(LOCTEXT("ConnectClientButton", "Connect Client"))
	                .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnConnectClientClicked)
	            ]
	            + SHorizontalBox::Slot().AutoWidth().Padding(4)
	            [
	                SNew(SButton)
	                .Text(LOCTEXT("DisconnectClientButton", "Disconnect Client"))
	                .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnDisconnectClientClicked)
	            ]
	            + SHorizontalBox::Slot().AutoWidth().Padding(4)
	            [
	                SNew(SButton)
	                .Text(LOCTEXT("StopAnyServerButton", "Force Stop Any Server"))
	                .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnStopAnyServerClicked)
	            ]
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SCheckBox)
	            .OnCheckStateChanged_Raw(this, &FUEJackAudioLinkModule::OnAutoConnectChanged)
	            .IsChecked_Lambda([](){
				const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
				return (Settings && Settings->bAutoConnectToNewClients) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
	            [ SNew(STextBlock).Text(LOCTEXT("AutoConnectLabel", "Auto-connect new clients")) ]
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SButton)
	            .Text(LOCTEXT("OpenSettingsButton", "Open Plugin Settings"))
	            .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnOpenSettingsClicked)
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(STextBlock)
	            .AutoWrapText(true)
	            .Text_Lambda([]()
	            {
	                FString Info;
	#if WITH_JACK
	                Info += TEXT("Diagnostics:\n");
	                Info += FString::Printf(TEXT("  Version: %s\n"), *FJackServerController::Get().GetVersion());
	                
	                if (FJackServerController::Get().WasServerStartedByPlugin())
	                {
	                    FString Backend = FJackServerController::Get().GetLastStartedDriver();
	                    Info += FString::Printf(TEXT("  Backend: %s\n"), Backend.IsEmpty() ? TEXT("Unknown") : *Backend);
	                    Info += TEXT("  Plugin Started: Yes\n");
	                }
	                else
	                {
	                    Info += TEXT("  Backend: External/Unknown\n");
	                    Info += TEXT("  Plugin Started: No\n");
	                }
	                
	                if (FJackClientManager::Get().IsConnected())
	                {
	                    Info += FString::Printf(TEXT("  Client: %s\n"), *FJackClientManager::Get().GetClientName());
	                }
	                else
	                {
	                    Info += TEXT("  Client: Not connected\n");
	                }
	#else
	                Info += TEXT("Diagnostics: WITH_JACK=0 (headers/libs not linked)\n");
	#endif
	                return FText::FromString(Info);
	            })
	        ]
	    ];

	return Tab;
}

FText FUEJackAudioLinkModule::GetStatusText() const
{
	FString Status;
#if WITH_JACK
	// Read monitor state for server availability and SR/BS
	const FJackServerState MonState = FJackServerMonitor::Get().GetState();
	const bool bAnyServerRunning = MonState.bServerAvailable;
	const bool bOurServerRunning = FJackServerController::Get().IsServerRunning();
	const FString Version = FJackServerController::Get().GetVersion();
	
	bool bClient = FJackClientManager::Get().IsConnected();
	FString ClientName = bClient ? FJackClientManager::Get().GetClientName() : FString();
	uint32 SR = static_cast<uint32>(MonState.SampleRate);
	uint32 BS = static_cast<uint32>(MonState.BufferSize);
	
	// If monitor has no SR/BS yet but server is up, probe once
	if ((!SR || !BS) && bAnyServerRunning)
	{
		int32 ProbeSR = 0, ProbeBS = 0;
		if (FJackServerController::Get().GetServerAudioConfig(ProbeSR, ProbeBS))
		{
			SR = static_cast<uint32>(ProbeSR);
			BS = static_cast<uint32>(ProbeBS);
		}
	}
	const bool bHasCfg = (SR > 0 && BS > 0);
	
	// Show real server status with ownership info
	FString ServerStatus;
	if (bAnyServerRunning)
	{
		ServerStatus = bOurServerRunning ? TEXT("RUNNING (started by plugin)") : TEXT("RUNNING (external)");
	}
	else
	{
		ServerStatus = TEXT("NOT RUNNING");
	}
	
	Status = FString::Printf(TEXT("Server: %s\nVersion: %s\nClient: %s%s%s"),
		*ServerStatus,
		*Version,
		bClient ? TEXT("CONNECTED ") : TEXT("NOT CONNECTED"),
		bClient && !ClientName.IsEmpty() ? *FString::Printf(TEXT("(%s)"), *ClientName) : TEXT(""),
		bHasCfg ? *FString::Printf(TEXT("\nAudio: %u Hz, %u frames"), SR, BS) : TEXT(""));

	// Show backend information only for plugin-started servers
	if (FJackServerController::Get().WasServerStartedByPlugin())
	{
		FString Backend = FJackServerController::Get().GetLastStartedDriver();
		if (!Backend.IsEmpty())
		{
			Status += FString::Printf(TEXT("\nBackend: %s"), *Backend);
		}
	}

	// Expected vs actual
	if (bHasCfg)
	{
		if (const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>())
		{
			if (static_cast<int32>(SR) != Settings->SampleRate || static_cast<int32>(BS) != Settings->BufferSize)
			{
				Status += FString::Printf(TEXT("\nSettings mismatch: wants %d Hz, %d frames → Restart required"), Settings->SampleRate, Settings->BufferSize);
			}
		}
	}
#else
	Status = TEXT("Server: UNKNOWN (WITH_JACK=0)\nVersion: n/a");
#endif
	return FText::FromString(Status);
}

FReply FUEJackAudioLinkModule::OnRestartServerClicked()
{
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
#if WITH_JACK
	UE_LOG(LogJackAudioLink, Display, TEXT("UI: Apply & Restart button clicked"));
	UE_LOG(LogJackAudioLink, Display, TEXT("Restarting JACK server with settings: %d Hz, %d frames, driver: %s"), 
		Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver.IsEmpty() ? TEXT("default") : *Settings->BackendDriver);
	
	if (FJackServerController::Get().RestartServer(Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver, Settings->JackdPath))
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("JACK server restarted successfully"));
		
		FString ClientName = Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName;
		UE_LOG(LogJackAudioLink, Display, TEXT("Reconnecting JACK client: %s"), *ClientName);
		
		if (FJackClientManager::Get().Connect(ClientName))
		{
			UE_LOG(LogJackAudioLink, Display, TEXT("JACK client reconnected successfully"));
			if (FJackClientManager::Get().RegisterAudioPorts(Settings->InputChannels, Settings->OutputChannels, TEXT("unreal")))
			{
				UE_LOG(LogJackAudioLink, Display, TEXT("Audio ports re-registered: %d inputs, %d outputs"), Settings->InputChannels, Settings->OutputChannels);
				if (FJackClientManager::Get().Activate())
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("JACK client re-activated successfully"));
					
					if (GEngine && GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
					{
						GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>()->StartAutoConnect(Settings->ClientMonitorInterval, Settings->bAutoConnectToNewClients);
						UE_LOG(LogJackAudioLink, Display, TEXT("Auto-connect started (enabled: %s, interval: %.1fs)"), 
							Settings->bAutoConnectToNewClients ? TEXT("yes") : TEXT("no"), Settings->ClientMonitorInterval);
					}
				}
				else
				{
					UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to activate JACK client after restart"));
				}
			}
			else
			{
				UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to register audio ports after restart"));
			}
		}
		else
		{
			UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to reconnect JACK client after restart"));
		}
	}
	else
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to restart JACK server"));
	}
#endif
	UpdateStatusDisplay();
	return FReply::Handled();
}

FReply FUEJackAudioLinkModule::OnOpenSettingsClicked()
{
	if (GIsEditor)
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->ShowViewer("Project", "Plugins", "Jack Audio Link");
		}
	}
	return FReply::Handled();
}

FReply FUEJackAudioLinkModule::OnConnectClientClicked()
{
#if WITH_JACK
	UE_LOG(LogJackAudioLink, Display, TEXT("UI: Connect Client button clicked"));
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	// Guard: do not attempt to connect if no server is available
	if (!FJackServerController::Get().IsAnyServerAvailable())
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("Cannot connect client: JACK server not running"));
		UpdateStatusDisplay();
		return FReply::Handled();
	}
	FString ClientName = Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName;
	
	UE_LOG(LogJackAudioLink, Display, TEXT("Attempting to connect JACK client: %s"), *ClientName);
	if (FJackClientManager::Get().Connect(ClientName))
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("JACK client connected successfully"));
		UE_LOG(LogJackAudioLink, Display, TEXT("Registering audio ports: %d inputs, %d outputs"), Settings->InputChannels, Settings->OutputChannels);
		
		if (FJackClientManager::Get().RegisterAudioPorts(Settings->InputChannels, Settings->OutputChannels, TEXT("unreal")))
		{
			UE_LOG(LogJackAudioLink, Display, TEXT("Audio ports registered successfully"));
			if (FJackClientManager::Get().Activate())
			{
				UE_LOG(LogJackAudioLink, Display, TEXT("JACK client activated successfully"));
			}
			else
			{
				UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to activate JACK client"));
			}
		}
		else
		{
			UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to register audio ports"));
		}
	}
	else
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to connect JACK client"));
	}
#endif
	UpdateStatusDisplay();
	return FReply::Handled();
}

FReply FUEJackAudioLinkModule::OnDisconnectClientClicked()
{
#if WITH_JACK
	UE_LOG(LogJackAudioLink, Display, TEXT("UI: Disconnect Client button clicked"));
	UE_LOG(LogJackAudioLink, Display, TEXT("Deactivating JACK client..."));
	FJackClientManager::Get().Deactivate();
	
	UE_LOG(LogJackAudioLink, Display, TEXT("Disconnecting JACK client..."));
	FJackClientManager::Get().Disconnect();
	UE_LOG(LogJackAudioLink, Display, TEXT("JACK client disconnected successfully"));
#endif
	UpdateStatusDisplay();
	return FReply::Handled();
}

void FUEJackAudioLinkModule::OnAutoConnectChanged(ECheckBoxState NewState)
{
	UJackAudioLinkSettings* Settings = GetMutableDefault<UJackAudioLinkSettings>();
	Settings->bAutoConnectToNewClients = (NewState == ECheckBoxState::Checked);
	Settings->SaveConfig();
#if WITH_JACK
	if (GEngine && GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
	{
		GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>()->StartAutoConnect(Settings->ClientMonitorInterval, Settings->bAutoConnectToNewClients);
	}
#endif
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(GetStatusText());
	}
}

bool FUEJackAudioLinkModule::IsRestartRequired(FString& OutMessage) const
{
#if WITH_JACK
	uint32 SR = FJackClientManager::Get().GetSampleRate();
	uint32 BS = FJackClientManager::Get().GetBufferSize();
	if ((!SR || !BS))
	{
		int32 ProbeSR = 0, ProbeBS = 0;
		if (FJackServerController::Get().GetServerAudioConfig(ProbeSR, ProbeBS))
		{
			SR = static_cast<uint32>(ProbeSR);
			BS = static_cast<uint32>(ProbeBS);
		}
	}
	if (const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>())
	{
		if (SR && BS && (static_cast<int32>(SR) != Settings->SampleRate || static_cast<int32>(BS) != Settings->BufferSize))
		{
			OutMessage = FString::Printf(TEXT("Restart required: wants %d Hz, %d frames"), Settings->SampleRate, Settings->BufferSize);
			return true;
		}
	}
#endif
	return false;
}

FReply FUEJackAudioLinkModule::OnApplyRestartClicked()
{
	return OnRestartServerClicked();
}

FReply FUEJackAudioLinkModule::OnStartServerClicked()
{
#if WITH_JACK
	UE_LOG(LogJackAudioLink, Display, TEXT("UI: Start Server button clicked"));
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	if (Settings)
	{
		if (FJackServerController::Get().StartServer(Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver, Settings->JackdPath))
		{
			UE_LOG(LogJackAudioLink, Display, TEXT("JACK server started successfully (%d Hz, %d frames)"), Settings->SampleRate, Settings->BufferSize);
			// Immediately probe sentinel so UI reflects RUNNING without waiting for next tick
			FJackServerMonitor::Get().RequestImmediateProbe();
			// Also schedule a couple of short delayed probes to catch the server becoming ready
			if (GEditor)
			{
				FTimerHandle TmpH1, TmpH2;
				GEditor->GetTimerManager()->SetTimer(TmpH1, FTimerDelegate::CreateLambda([this]()
				{
					FJackServerMonitor::Get().RequestImmediateProbe();
					UpdateStatusDisplay();
				}), 0.35f, false);
				GEditor->GetTimerManager()->SetTimer(TmpH2, FTimerDelegate::CreateLambda([this]()
				{
					FJackServerMonitor::Get().RequestImmediateProbe();
					UpdateStatusDisplay();
				}), 0.75f, false);
			}
		}
		else
		{
			UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to start JACK server"));
		}
	}
#endif
	UpdateStatusDisplay();
	return FReply::Handled();
}

FReply FUEJackAudioLinkModule::OnStopServerClicked()
{
#if WITH_JACK
	UE_LOG(LogJackAudioLink, Display, TEXT("UI: Stop Server button clicked"));
	
	// First, properly disconnect our client to avoid invalid state
	if (FJackClientManager::Get().IsConnected())
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("Disconnecting JACK client before stopping server..."));
		FJackClientManager::Get().Deactivate();
		FJackClientManager::Get().Disconnect();
		UE_LOG(LogJackAudioLink, Display, TEXT("JACK client disconnected safely"));
	}
	
	// Now stop the server
	if (FJackServerController::Get().StopServer())
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("JACK server stopped successfully"));
		// Immediately mark monitor down to avoid stale RUNNING in UI
		FJackServerMonitor::Get().MarkServerDown();
	}
	else
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("Failed to stop JACK server (or no server was running)"));
		// If an external server is running, ask sentinel to reattach (so the stop button reflects reality)
		if (FJackServerController::Get().IsAnyServerAvailable())
		{
			FJackServerMonitor::Get().RequestImmediateProbe();
		}
	}
#endif
	UpdateStatusDisplay();
	return FReply::Handled();
}

FReply FUEJackAudioLinkModule::OnStopAnyServerClicked()
{
#if WITH_JACK
	UE_LOG(LogJackAudioLink, Display, TEXT("UI: Force Stop Any Server clicked"));
	if (FJackServerController::Get().StopAnyServer())
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("Force-stopped JACK server"));
		FJackServerMonitor::Get().MarkServerDown();
	}
	else
	{
		UE_LOG(LogJackAudioLink, Warning, TEXT("Force stop did not terminate any JACK server"));
		FJackServerMonitor::Get().RequestImmediateProbe();
	}
#endif
	UpdateStatusDisplay();
	return FReply::Handled();
}

bool FUEJackAudioLinkModule::TickStatusUpdate(float /*DeltaTime*/)
{
	UpdateStatusDisplay();
	return true;
}

void FUEJackAudioLinkModule::UpdateStatusDisplay()
{
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(this->GetStatusText());
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUEJackAudioLinkModule, UEJackAudioLink)
