// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreeCardPoker/ThreeCardPokerBlueprintLibrary.h"

#include "ThreeCardPoker/ThreeCardPokerTableActor.h"
#include "casino_simulatorCharacter.h"

AThreeCardPokerTableActor* UThreeCardPokerBlueprintLibrary::GetThreeCardPokerTableForPlayer(Acasino_simulatorCharacter* Player)
{
	// AThreeCardPokerTableActor now handles its own world interaction directly (no more separate
	// dealer NPC to resolve through CurrentInteractionTarget) - see its class comment.
	return Player ? Player->GetCurrentThreeCardPokerTable() : nullptr;
}

FText UThreeCardPokerBlueprintLibrary::GetThreeCardPokerHandRankText(AThreeCardPokerTableActor* Table)
{
	if (!Table || Table->GetPlayerCards().Num() != 3)
	{
		return FText::FromString(TEXT(""));
	}

	const FText RankName = GetThreeCardPokerHandRankDisplayName(Table->GetHandRank(Table->GetPlayerCards()));
	return FText::Format(FText::FromString(TEXT("{0}")), RankName);
}

FText UThreeCardPokerBlueprintLibrary::GetThreeCardPokerResultText(AThreeCardPokerTableActor* Table)
{
	//|| Table->GetRoundState() != EThreeCardPokerRoundState::RoundComplete
	if (!Table)
	{
		return FText::GetEmpty();
	}

	switch (Table->GetLastResult())
	{
	case EThreeCardPokerHandResult::PlayerWin:
		return FText::FromString(TEXT("Win!"));
	case EThreeCardPokerHandResult::DealerWin:
		return FText::FromString(TEXT("Lose"));
	case EThreeCardPokerHandResult::Push:
		return FText::FromString(TEXT("Push"));
	case EThreeCardPokerHandResult::Folded:
		return FText::FromString(TEXT("Fold"));
	case EThreeCardPokerHandResult::DealerNotQualified:
		return FText::FromString(TEXT("Dealer Miss"));
	default:
		// GetHandRank indexes its Cards argument assuming exactly 3 (see its declaration comment), and
		// this default case is also hit before any cards are dealt (LastResult still None), so guard
		// it the same way GetThreeCardPokerHandRankText does above.
		if (Table->GetPlayerCards().Num() != 3)
		{
			return FText::GetEmpty();
		}
		return GetThreeCardPokerHandRankDisplayName(Table->GetHandRank(Table->GetPlayerCards()));
	}
}

FText UThreeCardPokerBlueprintLibrary::GetThreeCardPokerHandRankDisplayName(EThreeCardPokerHandRank Rank)
{
	switch (Rank)
	{
	case EThreeCardPokerHandRank::HighCard:
		return FText::FromString(TEXT("High Card"));
	case EThreeCardPokerHandRank::Pair:
		return FText::FromString(TEXT("Pair"));
	case EThreeCardPokerHandRank::Flush:
		return FText::FromString(TEXT("Flush"));
	case EThreeCardPokerHandRank::Straight:
		return FText::FromString(TEXT("Straight"));
	case EThreeCardPokerHandRank::ThreeOfAKind:
		return FText::FromString(TEXT("Three of a Kind"));
	case EThreeCardPokerHandRank::StraightFlush:
		return FText::FromString(TEXT("Straight Flush"));
	default:
		return FText::GetEmpty();
	}
}
