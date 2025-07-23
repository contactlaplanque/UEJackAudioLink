// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

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

private:
	/** Cached text block showing the status */
	TSharedPtr<class STextBlock> StatusTextBlock;

	// No timer handle needed as Slate bindings update automatically.
};
