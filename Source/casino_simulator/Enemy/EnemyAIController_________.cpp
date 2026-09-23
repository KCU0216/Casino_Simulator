// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyAIController_________.h"
#include "Kismet/GameplayStatics.h"
#include "casino_simulatorCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Enemy/EnemyBaseCharacter.h"

AEnemyAIController_________::AEnemyAIController_________()
{

	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	SightConfig->SightRadius = 600.0f;
	SightConfig->LoseSightRadius = 800.0f;
	SightConfig->PeripheralVisionAngleDegrees = 30.0f;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	SetPerceptionComponent(*AIPerceptionComp);
}

void AEnemyAIController_________::BeginPlay()
{
	Super::BeginPlay();
}

void AEnemyAIController_________::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!BehaviorTreeAsset)
	{
		return;
	}

	AEnemyBaseCharacter* Enemy = Cast<AEnemyBaseCharacter>(InPawn);

	if (!Enemy)
	{
			return;
	}

	UBlackboardComponent* BB = nullptr;

	if (!UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BB) || !BB)
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
