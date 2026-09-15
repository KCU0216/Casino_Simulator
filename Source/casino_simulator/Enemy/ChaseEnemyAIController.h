// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBaseAIController.h"
#include "ChaseEnemyAIController.generated.h"

class APawn;
struct FPathFollowingResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FChaseEnemyReachedTarget, APawn*, Target);

/** Reusable moving-target chase behavior for enemy AI controllers. */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API AChaseEnemyAIController : public AEnemyBaseAIController
{
	GENERATED_BODY()

public:
	AChaseEnemyAIController();

	/** False can still leave a valid chase waiting for a movement retry. */
	UFUNCTION(BlueprintCallable, Category="Enemy|Chase")
	bool StartChase(APawn* InTarget);

	UFUNCTION(BlueprintCallable, Category="Enemy|Chase")
	void StopChase();

	UFUNCTION(BlueprintPure, Category="Enemy|Chase")
	APawn* GetChaseTarget() const { return ChaseTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Enemy|Chase")
	bool IsChasing() const { return bIsChasing; }

	UPROPERTY(BlueprintAssignable, Category="Enemy|Chase")
	FChaseEnemyReachedTarget OnChaseReachedTarget;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Chase", meta=(ClampMin="0.0"))
	float ChaseAcceptanceRadius = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Chase")
	TObjectPtr<APawn> ChaseTarget;

	/** True while the controller is pursuing a target, including retry waits. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Chase")
	bool bIsChasing = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Chase", meta=(ClampMin="0.05"))
	float ChaseRetryInterval = 0.25f;

	/** Children can specialize the reach condition without duplicating movement. */
	virtual bool HasReachedChaseTarget() const;

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	virtual void OnUnPossess() override;

private:
	bool TryCompleteChase();
	bool RequestChaseMove(bool bIsRetry);
	void ScheduleChaseRetry();
	void RetryChase();
	void ClearChaseRetry();

	FTimerHandle ChaseRetryTimer;
};
