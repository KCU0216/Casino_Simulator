// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyAIController_________.h"
#include "Kismet/GameplayStatics.h"
#include "casino_simulatorCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Enemy/EnemyBaseCharacter.h"

void AEnemyAIController_________::BeginPlay()
{
	Super::BeginPlay();

	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	SetPerceptionComponent(*AIPerceptionComp);

	/*if (Blackboard)
	{
		Acasino_simulatorCharacter* Player = Cast<Acasino_simulatorCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
		if (Player == nullptr)
		{
			return;
		}

		Blackboard->SetValueAsObject(TEXT("Target"), Player);
	}*/
}

void AEnemyAIController_________::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!BehaviorTreeAsset)
	{
		return;
	}

	RunBehaviorTree(BehaviorTreeAsset);

	AEnemyBaseCharacter* Enemy = Cast<AEnemyBaseCharacter>(InPawn);

	if (!Enemy || !Blackboard)
	{
			return;
	}

	UBlackboardComponent* BB = nullptr;

	if (!UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BB))
	{
		return;
	}

	BB->SetValueAsInt(TEXT("Job"), Enemy->Job);

	RunBehaviorTree(BehaviorTreeAsset);
		
}

void AEnemyAIController_________::OnUnPossess()
{
	UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent);
	if (BTComp)
	{
		BTComp->StopTree();
	}

	Super::OnUnPossess();
}
