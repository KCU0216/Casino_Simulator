#pragma once

#include "CoreMinimal.h"
#include "RaceBillboardWidget.h"
#include "RaceRunner.h"
#include "UObject/UnrealType.h"
#include "RaceBillboardTestWidget.generated.h"

UCLASS(Transient, NotBlueprintable)
class URaceBillboardTestWidget : public URaceBillboardWidget
{
    GENERATED_BODY()
public:
    int32 PhaseCount = 0;
    int32 DataCount = 0;
    int32 AwakenCount = 0;
    TWeakObjectPtr<ARaceRunner> LastAwakenRunner;
    virtual void ProcessEvent(UFunction* Function, void* Parameters) override
    {
        if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(URaceBillboardWidget, OnRacePhaseChanged)) ++PhaseCount;
        if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(URaceBillboardWidget, OnRaceDataUpdated)) ++DataCount;
        if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(URaceBillboardWidget, BP_ShowAwakenRunner))
        {
            ++AwakenCount;
            const auto* RunnerProperty = FindFProperty<FObjectPropertyBase>(Function, TEXT("Runner"));
            LastAwakenRunner = Cast<ARaceRunner>(RunnerProperty->GetObjectPropertyValue_InContainer(Parameters));
        }
        Super::ProcessEvent(Function, Parameters);
    }
};
