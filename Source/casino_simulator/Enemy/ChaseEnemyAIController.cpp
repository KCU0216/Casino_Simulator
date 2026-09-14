// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/ChaseEnemyAIController.h"

#include "AI/Navigation/NavigationTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "casino_simulator.h"

AChaseEnemyAIController::AChaseEnemyAIController()
{
}

bool AChaseEnemyAIController::StartChase(APawn* InTarget)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy chase request ignored on a non-authority instance."));
		return false;
	}

	if (!IsValid(InTarget) || InTarget == GetPawn())
	{
		return false;
	}

	StopChase();
	ChaseTarget = InTarget;
	bIsChasing = true;
	ScheduleChaseRetry();

	if (TryCompleteChase())
	{
		return true;
	}

	return RequestChaseMove(false);
}

void AChaseEnemyAIController::StopChase()
{
	bIsChasing = false;
	ClearChaseRetry();
	ChaseTarget = nullptr;

	// AbortMove may synchronously call OnMoveCompleted, so clear chase state first.
	if (UPathFollowingComponent* ActivePathFollowing = GetPathFollowingComponent())
	{
		ActivePathFollowing->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
	}
}

bool AChaseEnemyAIController::HasReachedChaseTarget() const
{
	const APawn* ControlledPawn = GetPawn();
	const APawn* TargetPawn = GetChaseTarget();
	if (!IsValid(ControlledPawn) || !IsValid(TargetPawn))
	{
		return false;
	}

	const UPathFollowingComponent* ActivePathFollowing = GetPathFollowingComponent();
	return ActivePathFollowing && ActivePathFollowing->HasReached(
		*TargetPawn,
		EPathFollowingReachMode::OverlapAgentAndGoal,
		ChaseAcceptanceRadius
	);
}

bool AChaseEnemyAIController::TryCompleteChase()
{
	if (!HasAuthority() || !bIsChasing || !IsValid(GetPawn()) || !IsValid(ChaseTarget)
		|| !HasReachedChaseTarget())
	{
		return false;
	}

	APawn* ReachedTarget = ChaseTarget.Get();
	UE_LOG(
		Logcasino_simulator,
		Log,
		TEXT("Enemy '%s' reached chase target '%s'. Distance=%.1f."),
		*GetNameSafe(GetPawn()),
		*GetNameSafe(ReachedTarget),
		GetPawn()->GetDistanceTo(ReachedTarget)
	);

	StopChase();
	if (IsValid(ReachedTarget))
	{
		OnChaseReachedTarget.Broadcast(ReachedTarget);
	}
	return true;
}

bool AChaseEnemyAIController::RequestChaseMove(bool bIsRetry)
{
	if (!bIsChasing || !IsValid(ChaseTarget) || !IsValid(GetPawn()))
	{
		return false;
	}

	const EPathFollowingRequestResult::Type RequestResult = MoveToActor(
		ChaseTarget.Get(),
		ChaseAcceptanceRadius,
		true,
		true,
		true,
		nullptr,
		true
	);

	if (bIsRetry)
	{
		UE_LOG(
			Logcasino_simulator,
			Verbose,
			TEXT("Enemy '%s' retry chase request for '%s' returned %d."),
			*GetNameSafe(GetPawn()),
			*GetNameSafe(ChaseTarget.Get()),
			static_cast<int32>(RequestResult)
		);
	}
	else
	{
		UE_LOG(
			Logcasino_simulator,
			Log,
			TEXT("Enemy '%s' initial chase request for '%s' returned %d."),
			*GetNameSafe(GetPawn()),
			*GetNameSafe(ChaseTarget.Get()),
			static_cast<int32>(RequestResult)
		);
	}

	return RequestResult != EPathFollowingRequestResult::Failed;
}

void AChaseEnemyAIController::ScheduleChaseRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChaseRetryTimer,
			this,
			&AChaseEnemyAIController::RetryChase,
			ChaseRetryInterval,
			true
		);
	}
}

void AChaseEnemyAIController::RetryChase()
{
	if (!HasAuthority() || !bIsChasing || !IsValid(ChaseTarget) || !IsValid(GetPawn()))
	{
		StopChase();
		return;
	}

	// Check the reach condition even while the existing movement is still active.
	if (TryCompleteChase())
	{
		return;
	}

	if (UPathFollowingComponent* ActivePathFollowing = GetPathFollowingComponent())
	{
		if (ActivePathFollowing->GetStatus() != EPathFollowingStatus::Idle)
		{
			return;
		}
	}

	RequestChaseMove(true);
}

void AChaseEnemyAIController::ClearChaseRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChaseRetryTimer);
	}
}

void AChaseEnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (!HasAuthority() || !bIsChasing)
	{
		return;
	}

	if (!IsValid(ChaseTarget) || !IsValid(GetPawn()))
	{
		StopChase();
		return;
	}

	UE_LOG(
		Logcasino_simulator,
		Log,
		TEXT("Enemy '%s' move completed. Success=%s Code=%d Target='%s'."),
		*GetNameSafe(GetPawn()),
		Result.IsSuccess() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Result.Code),
		*GetNameSafe(ChaseTarget.Get())
	);

	// A finished path alone does not prove that the actual target was reached.
	if (TryCompleteChase())
	{
		return;
	}

	ScheduleChaseRetry();
}

void AChaseEnemyAIController::OnUnPossess()
{
	StopChase();
	Super::OnUnPossess();
}
