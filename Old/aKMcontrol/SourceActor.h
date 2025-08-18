#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SourceActor.generated.h"

UCLASS()
class AKMCONTROL_API ASourceActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ASourceActor();

protected:
	virtual void BeginPlay() override;

public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString Name;

	UFUNCTION(BlueprintCallable, Category = "Getters")
	FString GetName();
};
