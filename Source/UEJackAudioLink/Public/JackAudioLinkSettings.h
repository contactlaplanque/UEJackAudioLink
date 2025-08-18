#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "JackAudioLinkSettings.generated.h"

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Jack Audio Link"))
class UEJACKAUDIOLINK_API UJackAudioLinkSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UJackAudioLinkSettings(const FObjectInitializer& ObjectInitializer);

    // Server settings
    /** Sample rate for JACK server (Hz) */
    UPROPERTY(EditAnywhere, Config, Category="Server", meta=(ClampMin="8000", UIMin="8000"))
    int32 SampleRate = 48000;

    /** Buffer size (frames) */
    UPROPERTY(EditAnywhere, Config, Category="Server", meta=(ClampMin="32", UIMin="32"))
    int32 BufferSize = 512;

    /** Automatically start JACK server on editor start */
    UPROPERTY(EditAnywhere, Config, Category="Server")
    bool bAutoStartServer = true;

    /** Preferred backend driver name (per-platform default if empty). Examples: portaudio (Win), coreaudio (Mac), alsa (Linux) */
    UPROPERTY(EditAnywhere, Config, Category="Server")
    FString BackendDriver;

    /** Optional explicit path to jackd executable */
    UPROPERTY(EditAnywhere, Config, Category="Server")
    FString JackdPath;

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

    // Auto-connect
    /** Automatically connect new external client outputs to our inputs */
    UPROPERTY(EditAnywhere, Config, Category="Auto-Connect")
    bool bAutoConnectToNewClients = true;

    /** Interval for client monitoring when callbacks are unavailable (seconds) */
    UPROPERTY(EditAnywhere, Config, Category="Auto-Connect", meta=(ClampMin="0.1", UIMin="0.1"))
    float ClientMonitorInterval = 2.0f;

    // Ownership / shutdown policy
    /** Stop JACK server on editor shutdown if it was started by this plugin */
    UPROPERTY(EditAnywhere, Config, Category="Server")
    bool bKillServerOnShutdown = false;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
}; 