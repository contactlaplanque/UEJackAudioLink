#include "JackAudioLinkSettings.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "JackInterface.h"
#include "UEJackAudioLinkLog.h"

UJackAudioLinkSettings::UJackAudioLinkSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    if (ClientName.IsEmpty())
    {
        ClientName = FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName());
    }
}

#if WITH_EDITOR
void UJackAudioLinkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    UE_LOG(LogJackAudioLink, Display, TEXT("JackAudioLinkSettings changed – restarting server to apply new settings"));

    FJackInterface::Get().RestartServer(SampleRate, BufferSize);
}
#endif 