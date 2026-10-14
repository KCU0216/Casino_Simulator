// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackBlueprintLibrary.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Sound/CasinoSoundSubsystem.h"

void UBlackjackBlueprintLibrary::PlayCardPlacementSound(AActor* CardActor)
{
	UWorld* World = IsValid(CardActor) ? CardActor->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UGameInstance* GameInstance = World->GetGameInstance();
	if (UCasinoSoundSubsystem* Sound = GameInstance ? GameInstance->GetSubsystem<UCasinoSoundSubsystem>() : nullptr)
	{
		Sound->PlaySFX(TEXT("BlackjackCardPlace"), true, CardActor->GetActorLocation());
	}
}

FName UBlackjackBlueprintLibrary::GetBlackjackCardVisualRowName(EBlackjackDeckStyle DeckStyle, const FBlackjackCard& Card)
{
	const FName DeckStyleName = GetBlackjackDeckStyleName(DeckStyle);
	const FName SuitName = GetBlackjackSuitName(Card.Suit);
	const FName RankName = GetBlackjackRankName(Card.Rank);

	if (DeckStyleName.IsNone() || SuitName.IsNone() || RankName.IsNone())
	{
		return NAME_None;
	}

	return FName(FString::Printf(TEXT("%s_%s_%s"), *DeckStyleName.ToString(), *SuitName.ToString(), *RankName.ToString()));
}

bool UBlackjackBlueprintLibrary::FindBlackjackCardVisualData(
	UDataTable* CardVisualDataTable,
	EBlackjackDeckStyle DeckStyle,
	const FBlackjackCard& Card,
	FBlackjackCardVisualData& OutVisualData)
{
	OutVisualData = FBlackjackCardVisualData();
	if (!CardVisualDataTable)
	{
		return false;
	}

	// The public dealer hole card contains no rank until reveal. Its back still
	// needs the selected deck texture, without inventing or exposing a front.
	if (!Card.bFaceUp && Card.Rank == EBlackjackRank::None)
	{
		const FName DeckName = GetBlackjackDeckStyleName(DeckStyle);
		if (DeckName.IsNone()) return false;
		const FString Prefix = DeckName.ToString() + TEXT("_");
		TArray<FName> RowNames = CardVisualDataTable->GetRowNames();
		RowNames.Sort(FNameLexicalLess());
		for (const FName Name : RowNames)
		{
			if (!Name.ToString().StartsWith(Prefix)) continue;
			const FBlackjackCardVisualData* BackRow = CardVisualDataTable->FindRow<FBlackjackCardVisualData>(
				Name, TEXT("FindBlackjackCardBackVisualData"));
			if (BackRow && BackRow->BackTexture)
			{
				OutVisualData.DeckStyle = DeckStyle;
				OutVisualData.Rank = EBlackjackRank::None;
				OutVisualData.FrontTexture = nullptr;
				OutVisualData.BackTexture = BackRow->BackTexture;
				return true;
			}
		}
		return false;
	}

	const FName RowName = GetBlackjackCardVisualRowName(DeckStyle, Card);
	if (RowName.IsNone())
	{
		return false;
	}

	const FBlackjackCardVisualData* Row = CardVisualDataTable->FindRow<FBlackjackCardVisualData>(RowName, TEXT("FindBlackjackCardVisualData"));
	if (!Row)
	{
		return false;
	}

	OutVisualData = *Row;
	return true;
}

FName UBlackjackBlueprintLibrary::GetBlackjackDeckStyleName(EBlackjackDeckStyle DeckStyle)
{
	switch (DeckStyle)
	{
	case EBlackjackDeckStyle::Classic:
		return TEXT("Classic");
	case EBlackjackDeckStyle::CasinoRed:
		return TEXT("CasinoRed");
	case EBlackjackDeckStyle::CasinoBlue:
		return TEXT("CasinoBlue");
	case EBlackjackDeckStyle::Gold:
		return TEXT("Gold");
	default:
		return NAME_None;
	}
}

FName UBlackjackBlueprintLibrary::GetBlackjackSuitName(EBlackjackSuit Suit)
{
	switch (Suit)
	{
	case EBlackjackSuit::Clubs:
		return TEXT("Clubs");
	case EBlackjackSuit::Diamonds:
		return TEXT("Diamonds");
	case EBlackjackSuit::Hearts:
		return TEXT("Hearts");
	case EBlackjackSuit::Spades:
		return TEXT("Spades");
	default:
		return NAME_None;
	}
}

FName UBlackjackBlueprintLibrary::GetBlackjackRankName(EBlackjackRank Rank)
{
	switch (Rank)
	{
	case EBlackjackRank::Ace:
		return TEXT("Ace");
	case EBlackjackRank::Two:
		return TEXT("Two");
	case EBlackjackRank::Three:
		return TEXT("Three");
	case EBlackjackRank::Four:
		return TEXT("Four");
	case EBlackjackRank::Five:
		return TEXT("Five");
	case EBlackjackRank::Six:
		return TEXT("Six");
	case EBlackjackRank::Seven:
		return TEXT("Seven");
	case EBlackjackRank::Eight:
		return TEXT("Eight");
	case EBlackjackRank::Nine:
		return TEXT("Nine");
	case EBlackjackRank::Ten:
		return TEXT("Ten");
	case EBlackjackRank::Jack:
		return TEXT("Jack");
	case EBlackjackRank::Queen:
		return TEXT("Queen");
	case EBlackjackRank::King:
		return TEXT("King");
	case EBlackjackRank::None:
	default:
		return NAME_None;
	}
}
