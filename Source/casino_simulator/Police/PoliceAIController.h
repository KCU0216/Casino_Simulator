// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/ChaseEnemyAIController.h"
#include "PoliceAIController.generated.h"

/**
 * Police-specific controller entry point.
 *
 * The reusable chase implementation lives in AChaseEnemyAIController. This class checks the
 * arrest distance; target selection, day rules and arrest results remain outside this class.
 */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API APoliceAIController : public AChaseEnemyAIController
{
	GENERATED_BODY()

public:
	APoliceAIController();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Police|Arrest", meta=(ClampMin="0.0", Units="cm"))
	float ArrestDistance = 150.0f;

	virtual bool HasReachedChaseTarget() const override;
};
