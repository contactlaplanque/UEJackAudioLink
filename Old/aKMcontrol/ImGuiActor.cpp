// ImGuiActor.cpp

#include "ImGuiActor.h"
#include "SourceActor.h"
#include "Kismet/GameplayStatics.h"
#include "ImGuiModule.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

AImGuiActor::AImGuiActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AImGuiActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Find the JackAudioInterface actor in the world once
	JackInterface = FindJackAudioInterface();
	
	// Ensure ImGui input is disabled by default so camera controls work
	FImGuiModule::Get().GetProperties().SetInputEnabled(false);
	
	// Scale ImGui style by 2x (this should be done once)
	ImGui::GetStyle().ScaleAllSizes(2.0f);
}

void AImGuiActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

	// --- Update Level Meter State ---
	if (JackInterface)
	{
		const int32 NumChannels = JackInterface->GetNumRegisteredInputPorts();

		// Ensure our state arrays are the correct size
		if (SmoothedRmsLevels.Num() != NumChannels)
		{
			SmoothedRmsLevels.Init(0.0f, NumChannels);
			PeakLevels.Init(0.0f, NumChannels);
			TimesOfLastPeak.Init(0.0f, NumChannels);
		}
		
		for (int32 i = 0; i < NumChannels; ++i)
		{
			// Get the raw RMS level for the current channel
			const float RawRms = JackInterface->GetInputChannelLevel(i);

			// Apply smoothing (exponential moving average)
			const float SmoothingFactor = 0.6f;
			SmoothedRmsLevels[i] = (RawRms * (1.0f - SmoothingFactor)) + (SmoothedRmsLevels[i] * SmoothingFactor);

			// Update peak level
			if (SmoothedRmsLevels[i] > PeakLevels[i])
			{
				PeakLevels[i] = SmoothedRmsLevels[i];
				TimesOfLastPeak[i] = GetWorld()->GetTimeSeconds();
			}

			// Let peak level fall off after a short time
			const float PeakHoldTime = 1.5f;
			if (GetWorld()->GetTimeSeconds() - TimesOfLastPeak[i] > PeakHoldTime)
			{
				// Decay peak level smoothly
				PeakLevels[i] = FMath::Max(0.0f, PeakLevels[i] - (DeltaTime * 0.5f));
			}
		}
	}
	// --- End Update ---
	
    // Scale ImGui font by 2x (apply every frame to ensure it takes effect)
    ImGui::GetIO().FontGlobalScale = 2.0f;

    ImGui::Begin("Main Window");

    // Show ImGui input capture status
    bool bImGuiInputEnabled = FImGuiModule::Get().GetProperties().IsInputEnabled();
    ImGui::TextColored(bImGuiInputEnabled ? ImVec4(1,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1),
        bImGuiInputEnabled ? "ImGui Input: ENABLED" : "ImGui Input: DISABLED");
    ImGui::Separator();

    // FPS Monitor
    static float FPS = 0.0f;
    static float FrameTime = 0.0f;
    static float FPSUpdateTimer = 0.0f;
    
    FPSUpdateTimer += DeltaTime;
    if (FPSUpdateTimer >= 0.5f) // Update every 0.5 seconds
    {
        FPS = 1.0f / DeltaTime;
        FrameTime = DeltaTime * 1000.0f; // Convert to milliseconds
        FPSUpdateTimer = 0.0f;
    }
    
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "FPS Monitor");
    ImGui::Text("FPS: %.1f", FPS);
    ImGui::Text("Frame Time: %.2f ms", FrameTime);
    
    ImGui::Separator();

    // Render Jack Audio Interface section
    RenderJackAudioSection();
    
    ImGui::Separator();
    
    // Render existing Actor Picking section
    RenderActorPickingSection();

    ImGui::End();

	// Render the level meter in its own window if the interface is active
	if (JackInterface && JackInterface->IsConnectedToJack())
	{
		// Set a default position and size for the meter window so it doesn't overlap the main one
		ImGui::SetNextWindowPos(ImVec2(600, 50), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(450, 550), ImGuiCond_FirstUseEver);

		ImGui::Begin("Input Level Meter");
		RenderLevelMeter();
		ImGui::End();
	}
}

void AImGuiActor::RenderJackAudioSection()
{
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Jack Audio Interface");
    
    if (!JackInterface)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No JackAudioInterface found in scene");
        return;
    }
    
    // Connection Status
    EJackConnectionStatus Status = JackInterface->GetJackConnectionStatus();
    ImVec4 StatusColor;
    const char* StatusText;
    
    switch (Status)
    {
    case EJackConnectionStatus::Connected:
        StatusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        StatusText = "CONNECTED";
        break;
    case EJackConnectionStatus::Connecting:
        StatusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        StatusText = "CONNECTING";
        break;
    case EJackConnectionStatus::Disconnected:
        StatusColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
        StatusText = "DISCONNECTED";
        break;
    case EJackConnectionStatus::Failed:
        StatusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        StatusText = "FAILED";
        break;
    default:
        StatusColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        StatusText = "UNKNOWN";
        break;
    }
    
    ImGui::Text("Status: ");
    ImGui::SameLine();
    ImGui::TextColored(StatusColor, "%s", StatusText);
    
    if (JackInterface->IsConnectedToJack())
    {
        // Client Information - get values safely
        FString ClientName = JackInterface->GetJackClientName();
        int32 SampleRate = JackInterface->GetJackSampleRate();
        int32 BufferSize = JackInterface->GetJackBufferSize();
        float CPUUsage = JackInterface->GetJackCPUUsage();
        int32 NumInputs = JackInterface->GetNumRegisteredInputPorts();
        int32 NumOutputs = JackInterface->GetNumRegisteredOutputPorts();
        
        // Only display if we got valid values (prevents displaying zeros when server is dead)
        if (!ClientName.IsEmpty() && SampleRate > 0 && BufferSize > 0)
        {
            ImGui::Text("Client: %s", TCHAR_TO_UTF8(*ClientName));
            ImGui::Text("Sample Rate: %d Hz", SampleRate);
            ImGui::Text("Buffer Size: %d frames", BufferSize);
            ImGui::Text("CPU Usage: %.2f%%", CPUUsage);
            ImGui::Text("Ports: %d in, %d out", NumInputs, NumOutputs);
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Connection appears to be dead");
        }
        
        // Connection info displayed above
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Not connected to Jack server");
    }
}

void AImGuiActor::RenderLevelMeter()
{
	if (SmoothedRmsLevels.IsEmpty())
	{
		ImGui::Text("No input channels to display.");
		return;
	}

	// --- Adaptive Layout ---
	const int32 ChannelsPerGroup = 8;
	const float MeterWidth = 12.0f;
	const float MeterSpacing = 4.0f; // Spacing between individual meters for clarity
	const float MeterHeight = 240.0f; // Increased height for better scale readability
	const float ScaleWidth = 30.0f;
	const float GroupSpacing = 20.0f; // Increased spacing between groups
	// Account for the spacing between meters in the total group width
	const float GroupWidth = ScaleWidth + (MeterWidth * ChannelsPerGroup) + (MeterSpacing * (ChannelsPerGroup - 1)) + GroupSpacing;

	int numColumns = FMath::FloorToInt(ImGui::GetContentRegionAvail().x / GroupWidth);
	numColumns = FMath::Max(1, numColumns);

	for (int32 groupIdx = 0; groupIdx * ChannelsPerGroup < SmoothedRmsLevels.Num(); ++groupIdx)
	{
		ImGui::BeginGroup(); // Group the scale, meters, and label together

		// --- Get base positions and draw list ---
		const ImVec2 GroupCursorStart = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		// --- 1. Draw the dB Scale on the left of the group ---
		{
			const ImVec2 ScalePos = GroupCursorStart;
			const float MinDB = -60.0f;
			const float TickLevels[] = {6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f};

			for (float TickDB : TickLevels)
			{
				float NormalizedTick = (FMath::Clamp(TickDB, -60.f, 6.f) - MinDB) / (6.f - MinDB);
				float TickY = ScalePos.y + MeterHeight - (MeterHeight * NormalizedTick);
				
				// Draw tick mark
				DrawList->AddLine(ImVec2(ScalePos.x + ScaleWidth - 5, TickY), ImVec2(ScalePos.x + ScaleWidth, TickY), IM_COL32(180, 180, 180, 255));

				// Draw text
				FString TickLabel = FString::Printf(TEXT("%.0f"), TickDB);
				ImVec2 TextSize = ImGui::CalcTextSize(TCHAR_TO_UTF8(*TickLabel));
				DrawList->AddText(ImVec2(ScalePos.x + ScaleWidth - 7 - TextSize.x, TickY - TextSize.y / 2), IM_COL32(180, 180, 180, 255), TCHAR_TO_UTF8(*TickLabel));
			}
		}

		// --- 2. Draw the block of 8 meters ---
		ImGui::SetCursorScreenPos(ImVec2(GroupCursorStart.x + ScaleWidth, GroupCursorStart.y)); // Position cursor for meters
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(MeterSpacing, 0.0f));
		for (int32 i = 0; i < ChannelsPerGroup; ++i)
		{
			int32 channelIdx = groupIdx * ChannelsPerGroup + i;
			if (channelIdx >= SmoothedRmsLevels.Num()) break;

			const float MinDB = -60.0f, MaxDB = 6.0f;
			const float RmsDB = 20.0f * FMath::LogX(10.0f, FMath::Max(SmoothedRmsLevels[channelIdx], 1e-9f));
			const float PeakDB = 20.0f * FMath::LogX(10.0f, FMath::Max(PeakLevels[channelIdx], 1e-9f));
			const float NormalizedRms = (FMath::Clamp(RmsDB, MinDB, MaxDB) - MinDB) / (MaxDB - MinDB);
			const float NormalizedPeak = (FMath::Clamp(PeakDB, MinDB, MaxDB) - MinDB) / (MaxDB - MinDB);
			
			const ImVec2 MeterSize(MeterWidth, MeterHeight);
			const ImVec2 CursorPos = ImGui::GetCursorScreenPos();
			const ImVec2 BarStart = ImVec2(CursorPos.x, CursorPos.y + MeterSize.y);

			DrawList->AddRectFilled(CursorPos, ImVec2(CursorPos.x + MeterSize.x, CursorPos.y + MeterSize.y), IM_COL32(30, 30, 30, 255), 2.0f);
			
			if (NormalizedRms > 0) {
				const ImU32 GreenColor=IM_COL32(0,200,0,255), YellowColor=IM_COL32(255,255,0,255), RedColor=IM_COL32(255,0,0,255);
				const float YellowT=0.75f, RedT=0.90f;
				float GreenHeight = FMath::Min(MeterSize.y * NormalizedRms, MeterSize.y * YellowT);
				DrawList->AddRectFilled(ImVec2(BarStart.x, BarStart.y-GreenHeight), ImVec2(BarStart.x+MeterSize.x, BarStart.y), GreenColor, 2.0f, ImDrawFlags_RoundCornersBottom);
				if(NormalizedRms > YellowT){
					float YellowHeight = FMath::Min(MeterSize.y*NormalizedRms, MeterSize.y*RedT) - GreenHeight;
					DrawList->AddRectFilled(ImVec2(BarStart.x, BarStart.y-GreenHeight-YellowHeight), ImVec2(BarStart.x+MeterSize.x, BarStart.y-GreenHeight), YellowColor);
				}
				if(NormalizedRms > RedT){
					float RedHeight = (MeterSize.y*NormalizedRms) - (MeterSize.y*RedT);
					DrawList->AddRectFilled(ImVec2(BarStart.x, BarStart.y-(MeterSize.y*RedT)-RedHeight), ImVec2(BarStart.x+MeterSize.x, BarStart.y-(MeterSize.y*RedT)), RedColor);
				}
			}

			if (NormalizedPeak > 0) {
				const float PeakLineY = BarStart.y - (MeterSize.y * NormalizedPeak);
				DrawList->AddLine(ImVec2(CursorPos.x, PeakLineY), ImVec2(CursorPos.x + MeterSize.x, PeakLineY), IM_COL32(255, 255, 255, 180), 1.0f);
			}

			// The explicit divider line is no longer needed, as ItemSpacing handles it.

			ImGui::Dummy(MeterSize);
			ImGui::SameLine();
		}
		ImGui::PopStyleVar();
		
		// --- 3. Draw the group label, properly centered under the meters ---
		ImGui::SetCursorScreenPos(ImVec2(GroupCursorStart.x + ScaleWidth, GroupCursorStart.y + MeterHeight + 5));
		const int32 StartChannel = groupIdx * ChannelsPerGroup + 1;
		const int32 EndChannel = FMath::Min(StartChannel + ChannelsPerGroup - 1, SmoothedRmsLevels.Num());
		const FString ChannelLabel = FString::Printf(TEXT("%d-%d"), StartChannel, EndChannel);
		const ImVec2 TextSize = ImGui::CalcTextSize(TCHAR_TO_UTF8(*ChannelLabel));
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((MeterWidth * ChannelsPerGroup) - TextSize.x) * 0.5f);
		ImGui::Text("%s", TCHAR_TO_UTF8(*ChannelLabel));

		ImGui::EndGroup(); // End the main group for scale, meters, and label

		// --- Handle wrapping for the next group ---
		if ((groupIdx + 1) % numColumns != 0)
		{
			ImGui::SameLine(0, GroupSpacing);
		}
	}
}

void AImGuiActor::RenderActorPickingSection()
{
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Actor Picking");

    if (ImGui::Button("Toggle ImGui Input"))
    {
        ToggleImGuiInput();
    }

    // Actor Picking
    const char* ButtonTitle = bIsPickingActor ? "Stop Picking" : "Start Picking";
    if (ImGui::Button(ButtonTitle))
    {
        bIsPickingActor = !bIsPickingActor;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull);
    ULocalPlayer* LP = World ? World->GetFirstLocalPlayerFromController() : nullptr;
    if (bIsPickingActor)
    {
        if (LP && LP->ViewportClient)
        {
            // Get the projection data from world to viewport
            FSceneViewProjectionData ProjectionData;
            if (LP->GetProjectionData(LP->ViewportClient->Viewport, ProjectionData))
            {
                ImVec2 ScreenPosImGui = ImGui::GetMousePos();
                FVector2D ScreenPos = { ScreenPosImGui.x, ScreenPosImGui.y };

                FMatrix const InvViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix().InverseFast();

                FVector WorldPosition, WorldDirection;
                FSceneView::DeprojectScreenToWorld(ScreenPos, ProjectionData.GetConstrainedViewRect(), InvViewProjectionMatrix, WorldPosition, WorldDirection);

                FCollisionQueryParams Params("DevGuiActorPickerTrace", SCENE_QUERY_STAT_ONLY(UDevGuiSubsystem), true);

                FCollisionObjectQueryParams ObjectParams(
                    ECC_TO_BITFIELD(ECC_WorldStatic)
                    | ECC_TO_BITFIELD(ECC_WorldDynamic)
                    | ECC_TO_BITFIELD(ECC_Pawn)
                    | ECC_TO_BITFIELD(ECC_PhysicsBody)
                );

                FHitResult OutHit;
                if (World->LineTraceSingleByObjectType(
                    OutHit,
                    WorldPosition + WorldDirection * 100.0,
                    WorldPosition + WorldDirection * 10000.0,
                    ObjectParams,
                    Params))
                {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse)
                    {
                        PickedActor = OutHit.GetActor()->GetClass()->GetName() == "BP_Source_C" ? OutHit.GetActor() : nullptr;
                        bIsPickingActor = false;
                    }
                }
                else
                {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse)
                    {
                        PickedActor = nullptr;
                        bIsPickingActor = false;
                    }
                }
            }
        }

        // Exiting Actor Picking mode if we click the right button !
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            bIsPickingActor = false;
        }
    }

    if (AActor* Actor = PickedActor.Get())
    {
        if (ASourceActor* SourceActor = Cast<ASourceActor>(Actor))
        {
            ImGui::Text("Picked source:");
            ImGui::SameLine(); 
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), " %ls", *SourceActor->GetName());
        }
    }
}

void AImGuiActor::ToggleImGuiInput()
{
	FImGuiModule::Get().GetProperties().ToggleInput();
}

AJackAudioInterface* AImGuiActor::FindJackAudioInterface()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    // Find the first JackAudioInterface in the scene
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, AJackAudioInterface::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        return Cast<AJackAudioInterface>(FoundActors[0]);
    }
    
    return nullptr;
}

