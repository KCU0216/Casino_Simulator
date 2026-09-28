#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTT_ChooseEscapePoint.generated.h"

UCLASS()
class CASINO_SIMULATOR_API UBTT_ChooseEscapePoint : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTT_ChooseEscapePoint();

    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector EscapePointKey;

    UPROPERTY(EditAnywhere, Category = "Escape",
        meta = (ClampMin = "100.0"))
    float EscapeDistance = 1200.0f;

    UPROPERTY(EditAnywhere, Category = "Escape",
        meta = (ClampMin = "0.0"))
    float SearchRadius = 400.0f;
};