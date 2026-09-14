// Copyright Epic Games, Inc. All Rights Reserved.

#include "Police/PoliceAIController.h"

#include "GameFramework/Pawn.h"

APoliceAIController::APoliceAIController()
{
	ChaseAcceptanceRadius = 0.0f;
	ChaseRetryInterval = 0.1f;
}

bool APoliceAIController::HasReachedChaseTarget() const
{
	const APawn* PolicePawn = GetPawn();
	const APawn* TargetPawn = GetChaseTarget();
	if (!IsValid(PolicePawn) || !IsValid(TargetPawn))
	{
		return false;
	}

	return PolicePawn->GetDistanceTo(TargetPawn) <= FMath::Max(0.0f, ArrestDistance);
}
