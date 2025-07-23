// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEJackAudioLink.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "JackInterface.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "UEJackAudioLinkLog.h"
#include "JackAudioLinkSettings.h"
#include "ISettingsModule.h"

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

	// Start JACK server with configured settings and register client
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	if (Settings)
	{
	    FJackInterface::Get().StartServer(Settings->SampleRate, Settings->BufferSize);
	    FJackInterface::Get().RegisterClient(Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName,
	        Settings->InputChannels, Settings->OutputChannels);
	}
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

	// Nothing else to clean up.
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
	        ]
	        + SVerticalBox::Slot().AutoHeight().Padding(4)
	        [
	            SNew(SButton)
	            .Text(LOCTEXT("RestartJackButton", "Restart JACK Server"))
	            .OnClicked_Raw(this, &FUEJackAudioLinkModule::OnRestartServerClicked)
	        ]
	    ];
}

FText FUEJackAudioLinkModule::GetStatusText() const
{
	FString Version = FJackInterface::Get().GetVersion();
	const bool bRunning = FJackInterface::Get().IsServerRunning();
	FString Status = FString::Printf(TEXT("Server: %s\nVersion: %s"), bRunning ? TEXT("RUNNING") : TEXT("NOT RUNNING"), *Version);
	return FText::FromString(Status);
}

FReply FUEJackAudioLinkModule::OnRestartServerClicked()
{
	const UJackAudioLinkSettings* Settings = GetDefault<UJackAudioLinkSettings>();
	FJackInterface::Get().RestartServer(Settings->SampleRate, Settings->BufferSize);
	FJackInterface::Get().RegisterClient(Settings->ClientName.IsEmpty() ? FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName()) : Settings->ClientName,
	    Settings->InputChannels, Settings->OutputChannels);
	if (StatusTextBlock.IsValid())
	{
	    StatusTextBlock->SetText(GetStatusText());
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUEJackAudioLinkModule, UEJackAudioLink)
