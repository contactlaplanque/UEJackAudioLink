#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DeveloperSettings.h"
#include "JackAudioLinkSettings.generated.h"

UENUM(BlueprintType)
enum class EJackSampleRate : uint8
{
    SR_22050   UMETA(DisplayName = "22050"),
    SR_32000   UMETA(DisplayName = "32000"),
    SR_44100   UMETA(DisplayName = "44100"),
    SR_48000   UMETA(DisplayName = "48000"),
    SR_88200   UMETA(DisplayName = "88200"),
    SR_96000   UMETA(DisplayName = "96000"),
    SR_192000  UMETA(DisplayName = "192000"),
};

UENUM(BlueprintType)
enum class EJackBufferSize : uint8
{
    BS_16    UMETA(DisplayName = "16"),
    BS_32    UMETA(DisplayName = "32"),
    BS_64    UMETA(DisplayName = "64"),
    BS_128   UMETA(DisplayName = "128"),
    BS_256   UMETA(DisplayName = "256"),
    BS_512   UMETA(DisplayName = "512"),
    BS_1024  UMETA(DisplayName = "1024"),
    BS_2048  UMETA(DisplayName = "2048"),
    BS_4096  UMETA(DisplayName = "4096"),
};

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Jack Audio Link"))
class UEJACKAUDIOLINK_API UJackAudioLinkSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UJackAudioLinkSettings(const FObjectInitializer& ObjectInitializer);
    virtual void PostInitProperties() override;

    // Server settings
    /** Sample rate for JACK server (Hz) */
    UPROPERTY(EditAnywhere, Config, Category="Server")
    EJackSampleRate SampleRateChoice = EJackSampleRate::SR_48000;

    /** Buffer size (frames) */
    UPROPERTY(EditAnywhere, Config, Category="Server")
    EJackBufferSize BufferSizeChoice = EJackBufferSize::BS_512;

    /** Automatically start JACK server on editor start */
    UPROPERTY(EditAnywhere, Config, Category="Server")
    bool bAutoStartServer = true;

    // Client settings
    /** Client name that will appear in JACK graph */
    UPROPERTY(EditAnywhere, Config, Category="Client")
    FString ClientName;

    /** Number of audio input channels */
    UPROPERTY(EditAnywhere, Config, Category="Client", meta=(ClampMin="1", UIMin="1"))
    int32 InputChannels = 64;

    /** Number of audio output channels */
    UPROPERTY(EditAnywhere, Config, Category="Client", meta=(ClampMin="1", UIMin="1"))
    int32 OutputChannels = 64;

    // (Auto-connect and extra server options removed for simplicity)

    #if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
    #endif

public:
    int32 GetSampleRateValue() const;
    int32 GetBufferSizeValue() const;
}; 