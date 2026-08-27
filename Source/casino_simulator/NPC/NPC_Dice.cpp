// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/NPC_Dice.h"
#include "Components/StaticMeshComponent.h"
#include "DiceGame.h"
#include "casino_simulatorCharacter.h"

ANPC_Dice::ANPC_Dice()
{
	// Decorative prop only - no collision, just follows the character mesh (e.g. a hand socket).
	CupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CupMesh"));
	CupMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
	CupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANPC_Dice::BeginPlay()
{
	Super::BeginPlay();

	// Only the server spawns the dice table; it replicates down like any other actor.
	if (!HasAuthority() || !DiceGameClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FTransform SpawnTransform = FTransform(GetActorRotation(), GetActorLocation() + DiceGameOffset);
	DiceGameInstance = GetWorld()->SpawnActor<ADiceGame>(DiceGameClass, SpawnTransform, SpawnParams);
	DiceGameInstance->SetOwner(this);
}

void ANPC_Dice::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	InteractingPlayer = InteractingCharacter;
	Super::Interact(InteractingCharacter);
}

void ANPC_Dice::SetBetValue(int32 Select, int32 Betting)
{
	SelectedValue = Select;
	BettingAmount = Betting;
}

bool ANPC_Dice::ShowResult(int32 ResultValue)
{
	bool bResult = ResultValue % 2 == SelectedValue;
	if (bResult)
	{
		if (Acasino_simulatorCharacter* Player = InteractingPlayer.Get())
		{
			Player->AddCurrency(static_cast<float>(BettingAmount * 2));
		}
	}
	else
	{

	}
	return bResult;
}
