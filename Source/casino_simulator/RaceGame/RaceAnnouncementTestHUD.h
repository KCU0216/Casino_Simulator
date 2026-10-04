#pragma once

#include "CoreMinimal.h"
#include "UI/casino_simulatorPlayerHUD.h"
#include "RaceAnnouncementTestHUD.generated.h"

// Records BP event delivery without requiring an announcement implementation in a HUD asset.
UCLASS(Transient, NotBlueprintable)
class URaceAnnouncementTestHUD : public Ucasino_simulatorPlayerHUD
{
    GENERATED_BODY()
public:
    int32 ShowCount = 0;
    int32 ClearCount = 0;

    virtual void ProcessEvent(UFunction* Function, void* Parameters) override
    {
        if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(Ucasino_simulatorPlayerHUD, BP_ShowWorldEventAnnouncement))
        {
            ++ShowCount;
        }
        else if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(Ucasino_simulatorPlayerHUD, BP_ClearWorldEventAnnouncement))
        {
            ++ClearCount;
        }
        Super::ProcessEvent(Function, Parameters);
    }
};
