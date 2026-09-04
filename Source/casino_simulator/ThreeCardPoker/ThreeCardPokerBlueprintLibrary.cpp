// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreeCardPoker/ThreeCardPokerBlueprintLibrary.h"

#include "ThreeCardPoker/ThreeCardPokerTableActor.h"
#include "NPC/NPC_ThreeCardPoker.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"

AThreeCardPokerTableActor* UThreeCardPokerBlueprintLibrary::GetThreeCardPokerTableForPlayer(Acasino_simulatorCharacter* Player)
{
	if (!Player)
	{
		return nullptr;
	}

	Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(Player->GetController());
	if (!PC)
	{
		return nullptr;
	}

	ANPC_ThreeCardPoker* NPC = Cast<ANPC_ThreeCardPoker>(PC->GetCurrentInteractionTarget());
	return NPC ? NPC->GetThreeCardPokerTable() : nullptr;
}

FText UThreeCardPokerBlueprintLibrary::GetThreeCardPokerHandRankText(AThreeCardPokerTableActor* Table)
{
	if (!Table || Table->GetPlayerCards().Num() != 3)
	{
		return FText::FromString(TEXT("Hand: -"));
	}

	const FText RankName = GetThreeCardPokerHandRankDisplayName(Table->GetHandRank(Table->GetPlayerCards()));
	return FText::Format(FText::FromString(TEXT("Hand: {0}")), RankName);
}

FText UThreeCardPokerBlueprintLibrary::GetThreeCardPokerResultText(AThreeCardPokerTableActor* Table)
{
	if (!Table || Table->GetRoundState() != EThreeCardPokerRoundState::RoundComplete)
	{
		return FText::GetEmpty();
	}

	switch (Table->GetLastResult())
	{
	case EThreeCardPokerHandResult::PlayerWin:
		return FText::FromString(TEXT("승리!"));
	case EThreeCardPokerHandResult::DealerWin:
		return FText::FromString(TEXT("패배"));
	case EThreeCardPokerHandResult::Push:
		return FText::FromString(TEXT("푸시 (베팅 반환)"));
	case EThreeCardPokerHandResult::Folded:
		return FText::FromString(TEXT("폴드"));
	case EThreeCardPokerHandResult::DealerNotQualified:
		return FText::FromString(TEXT("딜러 미자격 - Ante 승리, Play 푸시"));
	default:
		return FText::GetEmpty();
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
