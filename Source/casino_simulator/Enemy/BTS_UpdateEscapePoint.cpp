#include "Enemy/BTS_UpdateEscapePoint.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/ThiefCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

UBTS_UpdateEscapePoint::UBTS_UpdateEscapePoint()
{
    NodeName = TEXT("Update Escape Point");

    bNotifyTick = true;
    bCallTickOnSearchStart = true;

    Interval = 0.3f;
    RandomDeviation = 0.0f;

    EscapePointKey.AddVectorFilter(
        this,
        GET_MEMBER_NAME_CHECKED(
            UBTS_UpdateEscapePoint,
            EscapePointKey));
}

void UBTS_UpdateEscapePoint::TickNode(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory,
    float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIController = OwnerComp.GetAIOwner();
    UBlackboardComponent* Blackboard =
        OwnerComp.GetBlackboardComponent();

    AThiefCharacter* Thief = IsValid(AIController)
        ? Cast<AThiefCharacter>(AIController->GetPawn())
        : nullptr;

    if (!IsValid(Thief) || !IsValid(Blackboard))
    {
        return;
    }

    if (!Thief->IsEscaping())
    {
        return;
    }

    AActor* Threat = Thief->GetEscapeFromActor();

    if (!IsValid(Threat)
        || EscapePointKey.SelectedKeyName.IsNone())
    {
        return;
    }

    const FVector Start = Thief->GetActorLocation();
    const FVector ThreatLocation = Threat->GetActorLocation();

    // 목적지가 아직 멀고 정상 이동 중이면 그대로 유지합니다.
    if (Blackboard->IsVectorValueSet(
        EscapePointKey.SelectedKeyName))
    {
        const FVector CurrentGoal =
            Blackboard->GetValueAsVector(
                EscapePointKey.SelectedKeyName);

        const bool bGoalIsFar =
            FVector::Dist2D(Start, CurrentGoal) > RefreshDistance;

        const bool bMoving =
            AIController->GetMoveStatus()
            == EPathFollowingStatus::Moving;

        if (bGoalIsFar && bMoving)
        {
            return;
        }
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            Thief->GetWorld());

    if (!IsValid(Navigation))
    {
        return;
    }

    FVector AwayDirection = Start - ThreatLocation;
    AwayDirection.Z = 0.0f;

    if (!AwayDirection.Normalize())
    {
        AwayDirection = Thief->GetActorForwardVector();
        AwayDirection.Z = 0.0f;
        AwayDirection.Normalize();
    }

    const double CurrentThreatDistance =
        FVector::Dist2D(Start, ThreatLocation);

    bool bFound = false;
    double BestScore = -1.0e30;
    FVector BestLocation = Start;

    for (int32 Index = 0; Index < CandidateCount; ++Index)
    {
        FNavLocation Candidate;

        // 멀리 떨어진 가상 중심점이 아니라
        // 도둑의 현재 위치에서 도달 가능한 후보를 찾습니다.
        if (!Navigation->GetRandomReachablePointInRadius(
            Start, SearchRadius, Candidate))
        {
            continue;
        }

        const double TravelDistance =
            FVector::Dist2D(Start, Candidate.Location);

        // 너무 가까운 목적지는 피합니다.
        if (TravelDistance < RefreshDistance + 150.0f)
        {
            continue;
        }

        UNavigationPath* Path =
            UNavigationSystemV1::FindPathToLocationSynchronously(
                Thief->GetWorld(),
                Start,
                Candidate.Location,
                Thief);

        // 목적지까지 완전히 도달할 수 있는 경로만 사용합니다.
        if (!IsValid(Path)
            || !Path->IsValid()
            || Path->IsPartial()
            || Path->PathPoints.Num() < 2)
        {
            continue;
        }

        double PathLength = 0.0;
        double ClosestThreatDistance = CurrentThreatDistance;

        for (int32 PointIndex = 1;
            PointIndex < Path->PathPoints.Num();
            ++PointIndex)
        {
            const FVector& Previous =
                Path->PathPoints[PointIndex - 1];

            const FVector& Next =
                Path->PathPoints[PointIndex];

            PathLength += FVector::Dist(Previous, Next);

            // 경로가 플레이어 가까이를 지나가는지도 평가합니다.
            const FVector ClosestPoint =
                FMath::ClosestPointOnSegment(
                    ThreatLocation, Previous, Next);

            ClosestThreatDistance = FMath::Min(
                ClosestThreatDistance,
                FVector::Dist2D(ClosestPoint, ThreatLocation));
        }

        FVector InitialDirection =
            Path->PathPoints[1] - Path->PathPoints[0];

        InitialDirection.Z = 0.0f;
        InitialDirection.Normalize();

        const double EndDistance =
            FVector::Dist2D(Candidate.Location, ThreatLocation);

        const double DirectionScore =
            FVector::DotProduct(InitialDirection, AwayDirection);

        const double ApproachPenalty = FMath::Max(
            0.0,
            CurrentThreatDistance - ClosestThreatDistance);

        // 멀어지는 목적지, 반대 방향 출발, 짧은 경로를 선호합니다.
        // 플레이어에게 접근하는 경로에는 감점을 줍니다.
        const double Score =
            (EndDistance - CurrentThreatDistance)
            + DirectionScore * 300.0
            - PathLength * 0.15
            - ApproachPenalty * 2.0;

        if (!bFound || Score > BestScore)
        {
            bFound = true;
            BestScore = Score;
            BestLocation = Candidate.Location;
        }
    }

    // 실패했다고 현재 목적지를 지우거나 이동을 중단하지 않습니다.
    if (bFound)
    {
        Blackboard->SetValueAsVector(
            EscapePointKey.SelectedKeyName,
            BestLocation);
    }
}