// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/BTT_ChooseEscapePoint.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/ThiefCharacter.h"
#include "NavigationSystem.h"

UBTT_ChooseEscapePoint::UBTT_ChooseEscapePoint()
{
	NodeName = TEXT("Choose Escape Point");

    EscapePointKey.AddVectorFilter(
        this,
        GET_MEMBER_NAME_CHECKED(
            UBTT_ChooseEscapePoint,
            EscapePointKey));
}

EBTNodeResult::Type UBTT_ChooseEscapePoint::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

    AThiefCharacter* Thief = AIController
        ? Cast<AThiefCharacter>(AIController->GetPawn())
        : nullptr;

    if (!IsValid(Thief) || !IsValid(Blackboard))
    {
        return EBTNodeResult::Failed;
    }

    if (!Thief->IsEscaping())
    {
        return EBTNodeResult::Failed;
    }

    // 도망칠 대상인 플레이어를 가져옵니다.
    AActor* Threat = Thief->GetEscapeFromActor();

    if (!IsValid(Threat))
    {
        return EBTNodeResult::Failed;
    }

    FVector AwayDirection =
        Thief->GetActorLocation() - Threat->GetActorLocation();

    AwayDirection.Z = 0.0f;

    if (!AwayDirection.Normalize())
    {
        AwayDirection = Thief->GetActorForwardVector();
        AwayDirection.Z = 0.0f;

        if (!AwayDirection.Normalize())
        {
            return EBTNodeResult::Failed;
        }
    }

    const FVector SearchCenter =
        Thief->GetActorLocation() + AwayDirection * EscapeDistance;

    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            Thief->GetWorld());

    FNavLocation EscapeLocation;

    if (!IsValid(NavigationSystem)
        || !NavigationSystem->GetRandomReachablePointInRadius(
            SearchCenter,
            SearchRadius,
            EscapeLocation))
    {
        return EBTNodeResult::Failed;
    }
    Blackboard->SetValueAsVector(
        EscapePointKey.SelectedKeyName,
        EscapeLocation.Location);

    return EBTNodeResult::Succeeded;
}
