// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController_________.generated.h"

/**
 * 
 */
UCLASS()
class CASINO_SIMULATOR_API AEnemyAIController_________ : public AAIController
{
	GENERATED_BODY()
public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackBoard")
	TObjectPtr<class UBehaviorTree> BehaviorTreeAsset;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	TObjectPtr<class UAIPerceptionComponent> AIPerceptionComp;

	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	TObjectPtr<class UAISenseConfig_Sight> SightConfig;

	


};
