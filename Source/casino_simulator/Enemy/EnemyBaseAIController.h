// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyBaseAIController.generated.h"

/** Common server-side base for enemy AI controllers. */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API AEnemyBaseAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyBaseAIController();
};
