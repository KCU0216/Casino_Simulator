// Copyright Epic Games, Inc. All Rights Reserved.

#include "DiceGame.h"
#include "Dice.h"
#include "NPC/NPC_Dice.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ADiceGame::ADiceGame()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	ResultText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ResultText"));
	//ResultText->SetupAttachment(RootComponent);

	/*ResultText->SetHorizontalAlignment(EHTA_Center);
	ResultText->SetVerticalAlignment(EVRTA_TextCenter);
	ResultText->SetWorldSize(50.f);*/
}

void ADiceGame::BeginPlay()
{
	Super::BeginPlay();

	// Applies the (default, empty) DisplayedResult state; runs on every machine, server and clients alike.
	OnRep_DisplayedResult();

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
}

void ADiceGame::SetDice(bool bVisible, int32 ResultValue)
{
	// Only the server decides/drives a roll; the reveal below replicates to clients via DisplayedResult.
	if (!HasAuthority())
	{
		return;
	}

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
		DisplayedResult = 0;
		OnRep_DisplayedResult();

		FTimerDelegate RevealDelegate = FTimerDelegate::CreateUObject(this, &ADiceGame::ShowResultText, ResultValue);
		GetWorldTimerManager().SetTimer(ResultTextTimerHandle, RevealDelegate, ResultTextRevealDelay, false);
	}
	else
	{
		DisplayedResult = 0;
		OnRep_DisplayedResult();
	}
}

void ADiceGame::ShowResultText(int32 ResultValue)
{
	// Setting the replicated property (and applying it locally right away, since the server/host
	// doesn't get its own OnRep call) is what makes the reveal show up on every client, not just
	// whichever machine happened to run this timer (the server, or a listen server's own client).
	DisplayedResult = ResultValue;
	OnRep_DisplayedResult();

	ANPC_Dice* NPC = Cast<ANPC_Dice>(Owner);
	if (NPC)
	{
		NPC->ShowResult(ResultValue);
	}
}

void ADiceGame::OnRep_DisplayedResult()
{
	if (DisplayedResult != 0)
	{
		ResultText->SetVisibility(true);
		ResultText->SetText(FText::AsNumber(DisplayedResult));
	}
	else
	{
		ResultText->SetVisibility(false);
		ResultText->SetText(FText::GetEmpty());
	}
}

void ADiceGame::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADiceGame, DisplayedResult);
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
