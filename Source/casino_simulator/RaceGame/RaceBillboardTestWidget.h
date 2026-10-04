#pragma once

#include "CoreMinimal.h"
#include "RaceBillboardWidget.h"
#include "RaceBillboardTestWidget.generated.h"

UCLASS(Transient, NotBlueprintable)
class URaceBillboardTestWidget : public URaceBillboardWidget
{
    GENERATED_BODY()
public:
    int32 PhaseCount = 0;
    int32 DataCount = 0;
    virtual void ProcessEvent(UFunction* Function, void* Parameters) override
    {
        if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(URaceBillboardWidget, OnRacePhaseChanged)) ++PhaseCount;
        if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(URaceBillboardWidget, OnRaceDataUpdated)) ++DataCount;
        Super::ProcessEvent(Function, Parameters);
    }
};