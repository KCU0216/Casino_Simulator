// Copyright Epic Games, Inc. All Rights Reserved.

#include "DiceGame.h"
#include "Dice.h"
#include "NPC/NPC_Dice.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

ADiceGame::ADiceGame()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	ResultText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ResultText"));
	//ResultText->SetupAttachment(RootComponent);

	ResultText->SetText(FText::GetEmpty());
	/*ResultText->SetHorizontalAlignment(EHTA_Center);
	ResultText->SetVerticalAlignment(EVRTA_TextCenter);
	ResultText->SetWorldSize(50.f);*/
}

void ADiceGame::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// Spawn the two dice a little apart so they don't overlap on top of each other.
	const FTransform Dice1Transform = FTransform(GetActorRotation(), GetActorLocation() + StartPos);
	const FTransform Dice2Transform = FTransform(GetActorRotation(), GetActorLocation() + StartPos + DistancePos);

	SpawnedDice1 = SpawnDice(BP_Dice_1, Dice1Transform);
	if (SpawnedDice1)
	{
		SpawnedDice1->SetActorScale3D(DiceScale);
		SpawnedDice1->SetActorHiddenInGame(true);
	}

	SpawnedDice2 = SpawnDice(BP_Dice_2, Dice2Transform);
	if (SpawnedDice2)
	{
		SpawnedDice2->SetActorScale3D(DiceScale);
		SpawnedDice2->SetActorHiddenInGame(true);
	}

	ResultText->SetVisibility(false);
}

void ADiceGame::SetDice(bool bVisible, int32 ResultValue)
{
	if (SpawnedDice1)
	{
		SpawnedDice1->SetActorHiddenInGame(!bVisible);
	}

	if (SpawnedDice2)
	{
		SpawnedDice2->SetActorHiddenInGame(!bVisible);
	}

	GetWorldTimerManager().ClearTimer(ResultTextTimerHandle);

	if (bVisible)
	{
		int MaxDice = ResultValue > 6 ? 6 : ResultValue - 1;
		int RandValue = FMath::RandRange(ResultValue - MaxDice, MaxDice);

		SpawnedDice1->Roll(RandValue);
		SpawnedDice2->Roll(ResultValue - RandValue);

		// Hide the text for now; it's revealed after ResultTextRevealDelay so it doesn't pop in before the dice roll.
		ResultText->SetVisibility(false);
		ResultText->SetText(FText::GetEmpty());

		FTimerDelegate RevealDelegate = FTimerDelegate::CreateUObject(this, &ADiceGame::ShowResultText, ResultValue);
		GetWorldTimerManager().SetTimer(ResultTextTimerHandle, RevealDelegate, ResultTextRevealDelay, false);
	}
	else
	{
		ResultText->SetVisibility(false);
		ResultText->SetText(FText::GetEmpty());
	}
}

void ADiceGame::ShowResultText(int32 ResultValue)
{
	ResultText->SetVisibility(true);
	ResultText->SetText(FText::AsNumber(ResultValue));
	ANPC_Dice* NPC = Cast<ANPC_Dice>(Owner);
	if (NPC)
	{
		NPC->ShowResult(ResultValue);
	}
}

ADice* ADiceGame::SpawnDice(TSubclassOf<ADice> DiceClass, const FTransform& SpawnTransform) const
{
	if (!DiceClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = const_cast<ADiceGame*>(this);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	return GetWorld()->SpawnActor<ADice>(DiceClass, SpawnTransform, SpawnParams);
}
