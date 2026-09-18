// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBaseCharacter.generated.h"

/** Shared physical base for all enemy characters. */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API AEnemyBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyBaseCharacter();

	/** Changes the movement speed at runtime. */
	UFUNCTION(BlueprintCallable, Category="Enemy|Movement")
	void SetEnemyMoveSpeed(float NewMoveSpeed);

	int Job;

protected:
	/** Each child Blueprint can override this value independently. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Movement", meta=(ClampMin="0.0"))
	float MoveSpeed = 350.0f;

	/** Each child Blueprint can override this value independently. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Movement", meta=(ClampMin="0.0"))
	float EnemyMaxStepHeight = 45.0f;

	virtual void BeginPlay() override;
};
