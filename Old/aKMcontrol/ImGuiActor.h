// ImGuiActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImGuiModule.h"
#include "JackAudioInterface.h"

#include "ImGuiActor.generated.h"

UCLASS()
class AKMCONTROL_API AImGuiActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AImGuiActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	static void ToggleImGuiInput();

	// Actor Picking Variables
	TWeakObjectPtr<AActor> PickedActor;
	bool bIsPickingActor = false;

private:
	// Pointer to the Jack Audio Interface actor
	UPROPERTY()
	AJackAudioInterface* JackInterface;
	
	// Level meter state for all channels
	TArray<float> SmoothedRmsLevels;
	TArray<float> PeakLevels;
	TArray<float> TimesOfLastPeak;

	// Find JackAudioInterface in the scene
	AJackAudioInterface* FindJackAudioInterface();
	
	// Render Jack audio interface section
	void RenderJackAudioSection();
	
	// Render the audio level meter widget
	void RenderLevelMeter();

	// Render existing functionality section
	void RenderActorPickingSection();
	

};
