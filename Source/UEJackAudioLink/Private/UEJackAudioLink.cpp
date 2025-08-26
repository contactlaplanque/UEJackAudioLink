// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEJackAudioLink.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "TimerManager.h"
#include "LevelEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
// #include "Engine/Engine.h" // not used here
#include "Misc/App.h"
#include "Containers/Ticker.h"
#include "UEJackAudioLinkLog.h"
#include "JackAudioLinkSettings.h"
#include "ISettingsModule.h"
#include "JackServerController.h"
#include "JackServerMonitor.h"
#include "JackClientManager.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FUEJackAudioLinkModule"

DEFINE_LOG_CATEGORY(LogJackAudioLink);

static const FName UEJackAudioLinkTabName("UEJackAudioLinkTab");
static TSharedPtr<FWorkspaceItem> JackWorkspaceMenuCategory;

// Helper to consistently obtain current JACK server audio config
// Prefers monitor snapshot, then live client, then a probe
static bool GetEffectiveServerAudioConfig(uint32& OutSampleRate, uint32& OutBufferSize)
{
#if WITH_JACK
    const FJackServerState MonState = FJackServerMonitor::Get().GetState();
    if (MonState.SampleRate > 0 && MonState.BufferSize > 0)
    {
        OutSampleRate = static_cast<uint32>(MonState.SampleRate);
        OutBufferSize = static_cast<uint32>(MonState.BufferSize);
        return true;
    }

    const uint32 ClientSR = FJackClientManager::Get().GetSampleRate();
    const uint32 ClientBS = FJackClientManager::Get().GetBufferSize();
    if (ClientSR > 0 && ClientBS > 0)
    {
        OutSampleRate = ClientSR;
        OutBufferSize = ClientBS;
        return true;
    }

    int32 ProbeSR = 0, ProbeBS = 0;
    if (FJackServerController::Get().GetServerAudioConfig(ProbeSR, ProbeBS))
    {
        OutSampleRate = static_cast<uint32>(ProbeSR);
        OutBufferSize = static_cast<uint32>(ProbeBS);
        return true;
    }
#endif
    return false;
}

void FUEJackAudioLinkModule::StartupModule()
{
	// Register tab spawner
	JackWorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("JackMenuGroup", "Jack Audio Link"));
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UEJackAudioLinkTabName, FOnSpawnTab::CreateRaw(this, &FUEJackAudioLinkModule::OnSpawnPluginTab))
	    .SetDisplayName(LOCTEXT("UEJackAudioLinkTabTitle", "Jack Audio Link"))
	    .SetMenuType(ETabSpawnerMenuType::Enabled)
	    .SetGroup(JackWorkspaceMenuCategory.ToSharedRef())
	    .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	// Removed auto-open to allow UE to restore the user’s saved layout/docked state

	// Register settings in Project Settings → Plugins → Jack Audio Link
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
	    SettingsModule->RegisterSettings("Project", "Plugins", "Jack Audio Link",
	        LOCTEXT("JackSettingsName", "Jack Audio Link"),
	        LOCTEXT("JackSettingsDesc", "Configure JACK Audio Link plugin"),
	        GetMutableDefault<UJackAudioLinkSettings>());
	}

	// Optionally auto-start server and connect client
#if WITH_JACK
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	if (Settings && Settings->bAutoStartServer)
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Auto-start enabled; will reconcile any existing JACK server with desired config"));
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
				if (CurrentSR != Settings->GetSampleRateValue() || CurrentBS != Settings->GetBufferSizeValue())
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK server running with wrong settings (%d Hz, %d frames). Expected: %d Hz, %d frames. Restarting..."), 
						CurrentSR, CurrentBS, Settings->GetSampleRateValue(), Settings->GetBufferSizeValue());
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
		
		int32 ObservedSR = 0, ObservedBS = 0;
		auto WaitForConfigMatch = [Settings, &ObservedSR, &ObservedBS]() -> bool
		{
			// Wait up to ~6s for server to report SR/BS and match settings
			for (int i = 0; i < 60; ++i)
			{
				int32 SR = 0, BS = 0;
				if (FJackServerController::Get().GetServerAudioConfig(SR, BS))
				{
					ObservedSR = SR;
					ObservedBS = BS;
					if (SR == Settings->GetSampleRateValue() && BS == Settings->GetBufferSizeValue())
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
			if (FJackServerController::Get().RestartServer(Settings->GetSampleRateValue(), Settings->GetBufferSizeValue()))
			{
				FJackServerMonitor::Get().RequestImmediateProbe();
				if (WaitForConfigMatch())
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK server restarted with desired config"));
				}
				else
				{
					UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Server config mismatch after restart; retrying once"));
					FJackServerController::Get().StopAnyServer();
					FPlatformProcess::Sleep(0.2f);
					FJackServerController::Get().StartServer(Settings->GetSampleRateValue(), Settings->GetBufferSizeValue());
					FJackServerMonitor::Get().RequestImmediateProbe();
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
			// Start server only if none is running; otherwise use the existing one
			if (!FJackServerController::Get().IsAnyServerAvailable())
			{
				UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Starting JACK server..."));
				if (FJackServerController::Get().StartServer(Settings->GetSampleRateValue(), Settings->GetBufferSizeValue()))
				{
					// Kick the monitor to attach ASAP so UI reflects state quickly
					FJackServerMonitor::Get().RequestImmediateProbe();
					if (WaitForConfigMatch())
					{
						UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK server started with desired config"));
					}
					else
					{
						if (ObservedSR == 0 || ObservedBS == 0)
						{
							UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Server not ready to report config within timeout; will not restart"));
							// Avoid unnecessary restart when we simply could not read SR/BS yet
							FJackServerMonitor::Get().RequestImmediateProbe();
						}
						else
						{
							UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Server config mismatch on first start (got %d Hz, %d frames); restarting with desired config"), ObservedSR, ObservedBS);
							FJackServerController::Get().StopAnyServer();
							FPlatformProcess::Sleep(0.2f);
							FJackServerController::Get().StartServer(Settings->GetSampleRateValue(), Settings->GetBufferSizeValue());
							FJackServerMonitor::Get().RequestImmediateProbe();
							WaitForConfigMatch();
						}
					}
				}
				else
				{
					UE_LOG(LogJackAudioLink, Warning, TEXT("Plugin startup: Failed to start JACK server"));
				}
			}
			else
			{
				UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Using existing JACK server (no restart needed)"));
				FJackServerMonitor::Get().RequestImmediateProbe();
			}
		}
		
		FString ClientName = Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName;
		UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Connecting JACK client: %s"), *ClientName);
		
		if (FJackClientManager::Get().Connect(ClientName))
		{
			UE_LOG(LogJackAudioLink, Display, TEXT("JACK client connected successfully"));
			if (FJackClientManager::Get().RegisterAudioPorts(Settings->InputChannels, Settings->OutputChannels, TEXT("unreal")))
			{
				UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: Audio ports registered: %d inputs, %d outputs"), Settings->InputChannels, Settings->OutputChannels);
				if (FJackClientManager::Get().Activate())
				{
					UE_LOG(LogJackAudioLink, Display, TEXT("Plugin startup: JACK client activated successfully"));
					// Auto-connect removed
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

	// Do not kill JACK server on shutdown; only stop our monitor
#if WITH_JACK
	FJackServerMonitor::Get().Stop();
	// Ensure our JACK client is fully disconnected to release JACK threads
	if (FJackClientManager::Get().IsConnected())
	{
		FJackClientManager::Get().Deactivate();
		FJackClientManager::Get().Disconnect();
	}
#endif
}

TSharedRef<SDockTab> FUEJackAudioLinkModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
	    .TabRole(ETabRole::NomadTab)
	    [
	        SNew(SVerticalBox)
	        // Colored server status
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SHorizontalBox)
	            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
	            [ SNew(STextBlock).Text(LOCTEXT("ServerLabel", "Server:")) ]
	            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
	            [
	                SNew(STextBlock)
	                .Text_Lambda([]()
	                {
	                    const bool bAny = FJackServerMonitor::Get().GetState().bServerAvailable;
	                    const bool bOurs = FJackServerController::Get().IsServerRunning();
	                    return bAny ? FText::FromString(bOurs ? TEXT("RUNNING (plugin)") : TEXT("RUNNING (external)")) : FText::FromString(TEXT("NOT RUNNING"));
	                })
	                .ColorAndOpacity_Lambda([]()
	                {
	                    const bool bAny = FJackServerMonitor::Get().GetState().bServerAvailable;
	                    return bAny ? FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f)) : FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f));
	                })
	            ]
	        ]
	        // Colored client status
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SHorizontalBox)
	            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
	            [ SNew(STextBlock).Text(LOCTEXT("ClientLabel", "Client:")) ]
	            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
	            [
	                SNew(STextBlock)
	                .Text_Lambda([]()
	                {
	                    const bool bClient = FJackClientManager::Get().IsConnected();
	                    return bClient ? FText::FromString(TEXT("CONNECTED")) : FText::FromString(TEXT("NOT CONNECTED"));
	                })
	                .ColorAndOpacity_Lambda([]()
	                {
	                    const bool bClient = FJackClientManager::Get().IsConnected();
	                    return bClient ? FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f)) : FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f));
	                })
	            ]
	        ]
	        // Existing detailed text
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SAssignNew(StatusTextBlock, STextBlock)
	            .Text_Lambda([this]() { return GetStatusText(); })
	            .AutoWrapText(true)
	        ]
	        // Restart required banner and button
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
	        // Controls grid
	        + SVerticalBox::Slot().AutoHeight()
	        [
	            SNew(SUniformGridPanel)
	            .SlotPadding(FMargin(4))
	            .MinDesiredSlotWidth(160.f)
	            .MinDesiredSlotHeight(32.f)
	            + SUniformGridPanel::Slot(0,0)
	            [ SNew(SButton).Text(LOCTEXT("StartServerButton", "Start Server")).OnClicked_Raw(this, &FUEJackAudioLinkModule::OnStartServerClicked) ]
	            + SUniformGridPanel::Slot(0,1)
	            [ SNew(SButton).Text(LOCTEXT("StopServerButton",  "Stop Server")).OnClicked_Raw(this, &FUEJackAudioLinkModule::OnStopServerClicked) ]
	            + SUniformGridPanel::Slot(1,0)
	            [ SNew(SButton).Text(LOCTEXT("ConnectClientButton",    "Connect Client")).OnClicked_Raw(this, &FUEJackAudioLinkModule::OnConnectClientClicked) ]
	            + SUniformGridPanel::Slot(1,1)
	            [ SNew(SButton).Text(LOCTEXT("DisconnectClientButton", "Disconnect Client")).OnClicked_Raw(this, &FUEJackAudioLinkModule::OnDisconnectClientClicked) ]
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SButton)
	            .Text(LOCTEXT("OpenSettingsButton", "Open Plugin Settings"))
	            .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnOpenSettingsClicked)
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
	uint32 SR = 0;
	uint32 BS = 0;
	GetEffectiveServerAudioConfig(SR, BS);
	
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
	
	Status = FString::Printf(TEXT("Server Audio Settings: %s"),
		
		bHasCfg ? *FString::Printf(TEXT("\nAudio: %u Hz, %u frames"), SR, BS) : TEXT(""));

	// Expected vs actual
	if (bHasCfg)
	{
		if (const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>())
		{
			if (static_cast<int32>(SR) != Settings->GetSampleRateValue() || static_cast<int32>(BS) != Settings->GetBufferSizeValue())
			{
				Status += FString::Printf(TEXT("\nSettings mismatch: wants %d Hz, %d frames → Restart required"), Settings->GetSampleRateValue(), Settings->GetBufferSizeValue());
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
	UE_LOG(LogJackAudioLink, Display, TEXT("Restarting JACK server with settings: %d Hz, %d frames"), 
		Settings->GetSampleRateValue(), Settings->GetBufferSizeValue());
	
	if (FJackServerController::Get().RestartServer(Settings->GetSampleRateValue(), Settings->GetBufferSizeValue()))
	{
		UE_LOG(LogJackAudioLink, Display, TEXT("JACK server restarted successfully"));
		// Ensure monitor reattaches quickly after restart
		FJackServerMonitor::Get().RequestImmediateProbe();
		
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
					
					// Auto-connect removed
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

// Auto-connect removed

bool FUEJackAudioLinkModule::IsRestartRequired(FString& OutMessage) const
{
#if WITH_JACK
	uint32 SR = 0, BS = 0;
	GetEffectiveServerAudioConfig(SR, BS);
	if (const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>())
	{
		if (SR && BS && (static_cast<int32>(SR) != Settings->GetSampleRateValue() || static_cast<int32>(BS) != Settings->GetBufferSizeValue()))
		{
			OutMessage = FString::Printf(TEXT("Restart required: wants %d Hz, %d frames"), Settings->GetSampleRateValue(), Settings->GetBufferSizeValue());
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
		if (FJackServerController::Get().StartServer(Settings->GetSampleRateValue(), Settings->GetBufferSizeValue()))
		{
			UE_LOG(LogJackAudioLink, Display, TEXT("JACK server started successfully (%d Hz, %d frames)"), Settings->GetSampleRateValue(), Settings->GetBufferSizeValue());
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

	// Simplified behavior: force stop any JACK server processes
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

// Removed Force Stop handler: Stop button now performs this behavior

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
