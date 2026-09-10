// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/ChaseEnemyAIController.h"

#include "AI/Navigation/NavigationTypes.h"
#include "Engine/World.h"
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

	return RequestChaseMove(false);
}

void AChaseEnemyAIController::StopChase()
{
	bIsChasing = false;
	ClearChaseRetry();

	if (UPathFollowingComponent* ActivePathFollowing = GetPathFollowingComponent())
	{
		ActivePathFollowing->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
	}

	ChaseTarget = nullptr;
}

bool AChaseEnemyAIController::RequestChaseMove(bool bIsRetry)
{
	if (!bIsChasing || !IsValid(ChaseTarget) || !GetPawn())
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
	if (!HasAuthority() || !bIsChasing || !IsValid(ChaseTarget) || !GetPawn())
	{
		StopChase();
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

	if (!bIsChasing)
	{
		return;
	}

	const bool bReachedTarget = Result.IsSuccess() && IsValid(ChaseTarget);

	UE_LOG(
		Logcasino_simulator,
		Log,
		TEXT("Enemy '%s' move completed. Success=%s Code=%d Target='%s'."),
		*GetNameSafe(GetPawn()),
		Result.IsSuccess() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Result.Code),
		*GetNameSafe(ChaseTarget.Get())
	);

	if (bReachedTarget)
	{
		bIsChasing = false;
		ClearChaseRetry();
		OnChaseReachedTarget.Broadcast(ChaseTarget.Get());
		return;
	}

	ScheduleChaseRetry();
}

void AChaseEnemyAIController::OnUnPossess()
{
	StopChase();
	Super::OnUnPossess();
}
