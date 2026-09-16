// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BT_NormalEnemy.generated.h"

/**
 * 
 */
UCLASS()
class CASINO_SIMULATOR_API UBT_NormalEnemy : public UBTService_BlackboardBase
{
	GENERATED_BODY()
	
public :
	UBT_NormalEnemy();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

};
