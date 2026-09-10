// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBaseCharacter.h"
#include "PoliceCharacter.generated.h"

/**
 * Base character for the police AI.
 *
 * The character owns the physical representation and movement component. Target selection and
 * navigation requests belong to APoliceAIController so that the day system can control them later.
 */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API APoliceCharacter : public AEnemyBaseCharacter
{
	GENERATED_BODY()

public:
	APoliceCharacter();
};
