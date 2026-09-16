// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/BT_NormalEnemy.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "GameFramework/Character.h"
#include "EnemyAIController_________.h"	

UBT_NormalEnemy::UBT_NormalEnemy()
{
		NodeName = TEXT("Update Player Target");

		bNotifyTick = true;
		bCallTickOnSearchStart = true;

		Interval = 0.2f;
		RandomDeviation = 0.0f;
}

void UBT_NormalEnemy::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	
	AEnemyAIController_________* AIController = Cast<AEnemyAIController_________>(OwnerComp.GetAIOwner());
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();

	if (!AIController || !BB)
	{
		return;
	}
	

	

	switch (BB->GetValueAsInt(TEXT("Job")))
	{
	case 0:
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(
			AIController->GetWorld(), 0);

		if (IsValid(Player))
		{
			BB->SetValueAsObject(TEXT("Target"), Player);
		}
		else
		{
			BB->ClearValue(TEXT("Target"));
		}
	}
			break;
	case 1:
	{
		UAIPerceptionComponent* Perception = AIController->GetPerceptionComponent();
		if (!Perception)
		{
			return;
		}
		TArray<AActor*> SightedActors;
		Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), SightedActors);

		AActor* FoundCharacter = nullptr;
		for (AActor* Actor : SightedActors)
		{
			if (Actor && Actor->IsA<ACharacter>())
			{
				FoundCharacter = Actor;
				break;
			}
		}
		if (FoundCharacter)
		{
			BB->SetValueAsObject(TEXT("Target"), FoundCharacter);
		}
		else
		{
			BB->ClearValue(GetSelectedBlackboardKey());
		}
	}
			break;
	case 2:
	{
		BB->ClearValue(TEXT("Target"));
	}
			break;
	}
}
