// SourceActor.cpp

#include "SourceActor.h"

ASourceActor::ASourceActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASourceActor::BeginPlay()
{
	Super::BeginPlay();
}

FString ASourceActor::GetName()
{
	return Name;
}


