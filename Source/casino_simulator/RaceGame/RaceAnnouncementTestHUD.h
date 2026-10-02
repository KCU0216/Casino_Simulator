#pragma once

#include "CoreMinimal.h"
#include "UI/casino_simulatorPlayerHUD.h"
#include "UObject/UnrealType.h"
#include "RaceAnnouncementTestHUD.generated.h"

// Records BP event delivery without requiring an announcement implementation in a HUD asset.
UCLASS(Transient, NotBlueprintable)
class URaceAnnouncementTestHUD : public Ucasino_simulatorPlayerHUD
{
    GENERATED_BODY()
public:
    int32 ShowCount = 0;
    int32 ClearCount = 0;
    FText LastMessage;
    float LastDuration = 0.f;

    virtual void ProcessEvent(UFunction* Function, void* Parameters) override
    {
        if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(Ucasino_simulatorPlayerHUD, BP_ShowWorldEventAnnouncement))
        {
            ++ShowCount;
            LastMessage = FindFProperty<FTextProperty>(Function, TEXT("Message"))->GetPropertyValue_InContainer(Parameters);
            LastDuration = FindFProperty<FFloatProperty>(Function, TEXT("Duration"))->GetPropertyValue_InContainer(Parameters);
        }
        else if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(Ucasino_simulatorPlayerHUD, BP_ClearWorldEventAnnouncement))
        {
            ++ClearCount;
        }
        Super::ProcessEvent(Function, Parameters);
    }
};
