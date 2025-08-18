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
#include "LevelEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "UEJackAudioLinkLog.h"
#include "JackAudioLinkSettings.h"
#include "ISettingsModule.h"
#include "JackServerController.h"
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
		FJackServerController::Get().StartServer(Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver, Settings->JackdPath);
		FString ClientName = Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName;
		if (FJackClientManager::Get().Connect(ClientName))
		{
			FJackClientManager::Get().RegisterAudioPorts(Settings->InputChannels, Settings->OutputChannels, TEXT("unreal"));
			FJackClientManager::Get().Activate();
			if (GEngine && GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
			{
				GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>()->StartAutoConnect(Settings->ClientMonitorInterval, Settings->bAutoConnectToNewClients);
			}
		}
	}
#endif
}

void FUEJackAudioLinkModule::ShutdownModule()
{
	// Unregister tab spawner
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UEJackAudioLinkTabName);

	// Unregister settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
	    SettingsModule->UnregisterSettings("Project", "Plugins", "Jack Audio Link");
	}

	// Stop server on shutdown only if owned (policy set in settings)
#if WITH_JACK
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	if (Settings && Settings->bKillServerOnShutdown && FJackServerController::Get().WasServerStartedByPlugin())
	{
		FJackServerController::Get().StopServer();
	}
#endif
}

TSharedRef<SDockTab> FUEJackAudioLinkModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
	    .TabRole(ETabRole::NomadTab)
	    [
	        SNew(SVerticalBox)
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SAssignNew(StatusTextBlock, STextBlock)
	            .Text(GetStatusText())
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
	            SNew(SButton)
	            .Text(LOCTEXT("ConnectClientButton", "Connect Client"))
	            .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnConnectClientClicked)
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SButton)
	            .Text(LOCTEXT("DisconnectClientButton", "Disconnect Client"))
	            .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnDisconnectClientClicked)
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
	                Info += FString::Printf(TEXT("  Backend: %s\n"), *FJackServerController::Get().GetLastStartedDriver());
	                Info += FString::Printf(TEXT("  Client: %s\n"), *FJackClientManager::Get().GetClientName());
	#else
	                Info += TEXT("Diagnostics: WITH_JACK=0 (headers/libs not linked)\n");
	#endif
	                return FText::FromString(Info);
	            })
	        ]
	    ];
}

FText FUEJackAudioLinkModule::GetStatusText() const
{
	FString Status;
#if WITH_JACK
	const bool bRunning = FJackServerController::Get().IsServerRunning();
	const FString Version = FJackServerController::Get().GetVersion();
	const bool bClient = FJackClientManager::Get().IsConnected();
	const FString ClientName = FJackClientManager::Get().GetClientName();
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
	const bool bHasCfg = (SR > 0 && BS > 0);
	Status = FString::Printf(TEXT("Server: %s\nVersion: %s\nClient: %s%s%s"),
		bRunning ? TEXT("RUNNING") : TEXT("NOT RUNNING"),
		*Version,
		bClient ? TEXT("CONNECTED ") : TEXT("NOT CONNECTED"),
		bClient ? *FString::Printf(TEXT("(%s)"), *ClientName) : TEXT(""),
		bHasCfg ? *FString::Printf(TEXT("\nAudio: %u Hz, %u frames"), SR, BS) : TEXT(""));

	if (FJackServerController::Get().WasServerStartedByPlugin())
	{
		Status += FString::Printf(TEXT("\nBackend: %s"), *FJackServerController::Get().GetLastStartedDriver());
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
	FJackServerController::Get().RestartServer(Settings->SampleRate, Settings->BufferSize, Settings->BackendDriver, Settings->JackdPath);
	FString ClientName = Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName;
	if (FJackClientManager::Get().Connect(ClientName))
	{
		FJackClientManager::Get().RegisterAudioPorts(Settings->InputChannels, Settings->OutputChannels, TEXT("unreal"));
		FJackClientManager::Get().Activate();
		if (GEngine && GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>())
		{
			GEngine->GetEngineSubsystem<UUEJackAudioLinkSubsystem>()->StartAutoConnect(Settings->ClientMonitorInterval, Settings->bAutoConnectToNewClients);
		}
	}
#endif
	if (StatusTextBlock.IsValid())
	{
	    StatusTextBlock->SetText(GetStatusText());
	}
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
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	FString ClientName = Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName;
	if (FJackClientManager::Get().Connect(ClientName))
	{
		FJackClientManager::Get().RegisterAudioPorts(Settings->InputChannels, Settings->OutputChannels, TEXT("unreal"));
		FJackClientManager::Get().Activate();
	}
#endif
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(GetStatusText());
	}
	return FReply::Handled();
}

FReply FUEJackAudioLinkModule::OnDisconnectClientClicked()
{
#if WITH_JACK
	FJackClientManager::Get().Deactivate();
	FJackClientManager::Get().Disconnect();
#endif
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(GetStatusText());
	}
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

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUEJackAudioLinkModule, UEJackAudioLink)
