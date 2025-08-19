// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Input/Reply.h"
#include "Widgets/Input/SCheckBox.h"
#include "Containers/Ticker.h"

// Using lightweight includes for FReply and ECheckBoxState to satisfy linter

class FUEJackAudioLinkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Status text is obtained directly via binding, so no explicit refresh function.

	/** Slate Tab spawn function */
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** Returns the status text to display */
	FText GetStatusText() const;

	/** Callback when the restart button is clicked */
	FReply OnRestartServerClicked();

	/** Start and stop server buttons */
	FReply OnStartServerClicked();
	FReply OnStopServerClicked();

	/** Force stop any server (external or plugin-owned) */
	FReply OnStopAnyServerClicked();

	/** Open Project Settings at plugin section */
	FReply OnOpenSettingsClicked();

	/** Connect and disconnect client */
	FReply OnConnectClientClicked();
	FReply OnDisconnectClientClicked();

	/** Toggle auto-connect */
	void OnAutoConnectChanged(ECheckBoxState NewState);

	/** Apply settings and restart server/client */
	FReply OnApplyRestartClicked();

	/** Returns true and message if audio config mismatches settings */
	bool IsRestartRequired(FString& OutMessage) const;

	/** Update status display (called by timer) */
	void UpdateStatusDisplay();
	bool TickStatusUpdate(float DeltaTime);

private:
	/** Cached text block showing the status */
	TSharedPtr<class STextBlock> StatusTextBlock;

	/** Ticker handle for periodic status updates */
	FTSTicker::FDelegateHandle StatusUpdateTickHandle;
};
