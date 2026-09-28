// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBaseCharacter.generated.h"

class AActor;
class UAnimMontage;

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

	UFUNCTION(BlueprintCallable, Category = "Enemy|Movement")
	void ApplyChaseSpeed();

	
	UFUNCTION(BlueprintCallable, Category = "Enemy|Movement")
	void ApplyPatrolSpeed();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Movement")
	void ApplyRandomPatrolSpeed();

	UFUNCTION(BlueprintCallable, Category = "Enemy|HitReaction")
	virtual void HandleEnemyHitReaction(AActor* Attacker);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayHitReaction(UAnimMontage* HitMontage);
protected:
	/** Each child Blueprint can override this value independently. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Movement", meta=(ClampMin="0.0"))
	float MoveSpeed = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement",
		meta = (ClampMin = "0.0"))
	float PatrolMinSpeed = 80.0f;

	/** Each child Blueprint can override this value independently. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Movement", meta=(ClampMin="0.0"))
	float EnemyMaxStepHeight = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitReaction")
	TObjectPtr<UAnimMontage> HitFrontMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitReaction")
	TObjectPtr<UAnimMontage> HitBackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitReaction")
	TObjectPtr<UAnimMontage> HitLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitReaction")
	TObjectPtr<UAnimMontage> HitRightMontage;

	UAnimMontage* SelectHitMontage(AActor* Attacker) const;

	virtual void BeginPlay() override;
};
