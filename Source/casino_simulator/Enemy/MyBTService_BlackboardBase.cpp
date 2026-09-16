// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/MyBTService_BlackboardBase.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MyBTService_BlackboardBase.h"



void UMyBTService_BlackboardBase::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!AIController || !Blackboard)
	{
		return;
	}

	APawn* Enemy = AIController->GetPawn();

	if (!Enemy)
	{
		return;
	}

	UNavigationSystemV1* NavSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(
			Enemy->GetWorld());

	if (!NavSystem)
	{
		return;
	}

	FNavLocation RandomLocation;
	const float SearchRadius = 3000.0f;

	if (NavSystem->GetRandomReachablePointInRadius(
		Enemy->GetActorLocation(),
		SearchRadius,
		RandomLocation))
	{
		Blackboard->SetValueAsVector(
			BlackboardKey.SelectedKeyName,
			RandomLocation.Location);
	}

}
