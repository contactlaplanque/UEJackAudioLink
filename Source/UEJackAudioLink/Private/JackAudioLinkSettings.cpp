#include "JackAudioLinkSettings.h"
#include "Misc/App.h"
#include "UEJackAudioLinkLog.h"

UJackAudioLinkSettings::UJackAudioLinkSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    if (ClientName.IsEmpty())
    {
        ClientName = FString::Printf(TEXT("UnrealJackClient-%s"), FApp::GetProjectName());
    }
}

void UJackAudioLinkSettings::PostInitProperties()
{
    Super::PostInitProperties();
    // Backward-compat for old integer config values: try to parse and map
    int32 RawSR = 0, RawBS = 0;
    if (GConfig)
    {
        int32 Tmp;
        if (GConfig->GetInt(TEXT("/Script/UEJackAudioLink.JackAudioLinkSettings"), TEXT("SampleRate"), Tmp, GGameIni))
        {
            RawSR = Tmp;
        }
        if (GConfig->GetInt(TEXT("/Script/UEJackAudioLink.JackAudioLinkSettings"), TEXT("BufferSize"), Tmp, GGameIni))
        {
            RawBS = Tmp;
        }
    }

    auto MapSR = [](int32 V) -> EJackSampleRate
    {
        switch (V)
        {
            case 22050: return EJackSampleRate::SR_22050; 
            case 32000: return EJackSampleRate::SR_32000; 
            case 44100: return EJackSampleRate::SR_44100; 
            case 48000: return EJackSampleRate::SR_48000; 
            case 88200: return EJackSampleRate::SR_88200; 
            case 96000: return EJackSampleRate::SR_96000; 
            case 192000: return EJackSampleRate::SR_192000; 
            default: return EJackSampleRate::SR_48000;
        }
    };
    auto MapBS = [](int32 V) -> EJackBufferSize
    {
        switch (V)
        {
            case 16: return EJackBufferSize::BS_16; 
            case 32: return EJackBufferSize::BS_32; 
            case 64: return EJackBufferSize::BS_64; 
            case 128: return EJackBufferSize::BS_128; 
            case 256: return EJackBufferSize::BS_256; 
            case 512: return EJackBufferSize::BS_512; 
            case 1024: return EJackBufferSize::BS_1024; 
            case 2048: return EJackBufferSize::BS_2048; 
            case 4096: return EJackBufferSize::BS_4096; 
            default: return EJackBufferSize::BS_512;
        }
    };

    if (RawSR > 0)
    {
        SampleRateChoice = MapSR(RawSR);
    }
    if (RawBS > 0)
    {
        BufferSizeChoice = MapBS(RawBS);
    }
}

int32 UJackAudioLinkSettings::GetSampleRateValue() const
{
    switch (SampleRateChoice)
    {
        case EJackSampleRate::SR_22050:  return 22050;
        case EJackSampleRate::SR_32000:  return 32000;
        case EJackSampleRate::SR_44100:  return 44100;
        case EJackSampleRate::SR_48000:  return 48000;
        case EJackSampleRate::SR_88200:  return 88200;
        case EJackSampleRate::SR_96000:  return 96000;
        case EJackSampleRate::SR_192000: return 192000;
        default: return 48000;
    }
}

int32 UJackAudioLinkSettings::GetBufferSizeValue() const
{
    switch (BufferSizeChoice)
    {
        case EJackBufferSize::BS_16:   return 16;
        case EJackBufferSize::BS_32:   return 32;
        case EJackBufferSize::BS_64:   return 64;
        case EJackBufferSize::BS_128:  return 128;
        case EJackBufferSize::BS_256:  return 256;
        case EJackBufferSize::BS_512:  return 512;
        case EJackBufferSize::BS_1024: return 1024;
        case EJackBufferSize::BS_2048: return 2048;
        case EJackBufferSize::BS_4096: return 4096;
        default: return 512;
    }
}

#if WITH_EDITOR
void UJackAudioLinkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    UE_LOG(LogJackAudioLink, Display, TEXT("JackAudioLinkSettings changed"));
}
#endif 