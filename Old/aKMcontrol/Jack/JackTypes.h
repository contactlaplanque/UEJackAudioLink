#pragma once

#include "CoreMinimal.h"
#include "JackTypes.generated.h"

/**
 * Jack connection status
 */
UENUM(BlueprintType)
enum class EJackConnectionStatus : uint8
{
	Disconnected UMETA(DisplayName = "Disconnected"),
	Connecting UMETA(DisplayName = "Connecting"),
	Connected UMETA(DisplayName = "Connected"),
	Failed UMETA(DisplayName = "Failed")
};

/**
 * Jack port flags
 */
UENUM(BlueprintType)
enum class EJackPortFlags : uint8
{
	IsInput UMETA(DisplayName = "Input Port"),
	IsOutput UMETA(DisplayName = "Output Port"),
	IsPhysical UMETA(DisplayName = "Physical Port"),
	CanMonitor UMETA(DisplayName = "Can Monitor"),
	IsTerminal UMETA(DisplayName = "Terminal Port")
};

/**
 * Jack performance metrics structure
 */
USTRUCT(BlueprintType)
struct FJackPerformanceMetrics
{
	GENERATED_BODY()

	// Audio processing time in milliseconds
	UPROPERTY(BlueprintReadOnly, Category = "Jack Performance")
	float AudioCallbackTime = 0.0f;

	// CPU usage percentage
	UPROPERTY(BlueprintReadOnly, Category = "Jack Performance")
	float CPUUsage = 0.0f;

	// Number of xruns (buffer underruns/overruns)
	UPROPERTY(BlueprintReadOnly, Category = "Jack Performance")
	int32 XRuns = 0;

	// Current buffer latency in frames
	UPROPERTY(BlueprintReadOnly, Category = "Jack Performance")
	float BufferLatency = 0.0f;

	// Sample rate accuracy
	UPROPERTY(BlueprintReadOnly, Category = "Jack Performance")
	float SampleRateAccuracy = 0.0f;

	// Number of dropped frames
	UPROPERTY(BlueprintReadOnly, Category = "Jack Performance")
	int32 DroppedFrames = 0;

	// Timing jitter
	UPROPERTY(BlueprintReadOnly, Category = "Jack Performance")
	float Jitter = 0.0f;
}; 