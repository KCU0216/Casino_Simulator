// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/ChaseEnemyAIController.h"
#include "PoliceAIController.generated.h"

/**
 * Police-specific controller entry point.
 *
 * The reusable chase implementation lives in AChaseEnemyAIController. Target selection, day
 * rules, arrest and presentation remain outside this class.
 */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API APoliceAIController : public AChaseEnemyAIController
{
	GENERATED_BODY()

public:
	APoliceAIController();
};
