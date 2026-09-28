#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTS_UpdateEscapePoint.generated.h"

UCLASS()
class CASINO_SIMULATOR_API UBTS_UpdateEscapePoint : public UBTService
{
    GENERATED_BODY()

public:
    UBTS_UpdateEscapePoint();

protected:
    virtual void TickNode(
        UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory,
        float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector EscapePointKey;

    // 도둑 주변에서 목적지를 찾는 범위입니다.
    UPROPERTY(EditAnywhere, Category = "Escape",
        meta = (ClampMin = "100.0"))
    float SearchRadius = 1500.0f;

    // 현재 목적지까지 이 거리 이내로 접근하면 갱신합니다.
    UPROPERTY(EditAnywhere, Category = "Escape",
        meta = (ClampMin = "100.0"))
    float RefreshDistance = 450.0f;

    // 새 목적지가 필요할 때 비교할 후보 수입니다.
    UPROPERTY(EditAnywhere, Category = "Escape",
        meta = (ClampMin = "1", ClampMax = "16"))
    int32 CandidateCount = 8;
};