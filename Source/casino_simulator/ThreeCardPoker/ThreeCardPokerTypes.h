// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blackjack/BlackjackTypes.h"
#include "ThreeCardPokerTypes.generated.h"

// Card representation (EBlackjackSuit / EBlackjackRank / FBlackjackCard) is reused as-is from the
// Blackjack module — same physical deck, same DT_BlackjackCardVisuals lookup — so this module never
// duplicates card enums, textures, or a visual data table. See BlackjackTypes.h.

UENUM(BlueprintType)
enum class EThreeCardPokerHandRank : uint8
{
	HighCard UMETA(DisplayName="High Card"),
	Pair UMETA(DisplayName="Pair"),
	Flush UMETA(DisplayName="Flush"),
	Straight UMETA(DisplayName="Straight"),
	ThreeOfAKind UMETA(DisplayName="Three of a Kind"),
	StraightFlush UMETA(DisplayName="Straight Flush")
};

/** 1:1 vs. the dealer (see ANPC_ThreeCardPoker) — no seat-waiting state, since there's never more
 * than one player at a time. */
UENUM(BlueprintType)
enum class EThreeCardPokerRoundState : uint8
{
	WaitingForBet UMETA(DisplayName="Waiting For Bet"),
	Dealing UMETA(DisplayName="Dealing"),
	PlayerDecision UMETA(DisplayName="Player Decision"),
	Resolving UMETA(DisplayName="Resolving"),
	RoundComplete UMETA(DisplayName="Round Complete")
};

UENUM(BlueprintType)
enum class EThreeCardPokerHandResult : uint8
{
	None UMETA(DisplayName="None"),
	PlayerWin UMETA(DisplayName="Player Win"),
	DealerWin UMETA(DisplayName="Dealer Win"),
	Push UMETA(DisplayName="Push"),
	Folded UMETA(DisplayName="Folded"),
	DealerNotQualified UMETA(DisplayName="Dealer Not Qualified")
};

/** Pair Plus side-bet payout multipliers ("X" in "X:1"), keyed by the player's own 3-card hand rank. */
USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FThreeCardPokerPairPlusPayouts
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ThreeCardPoker|Payout")
	int32 PairMultiplier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ThreeCardPoker|Payout")
	int32 FlushMultiplier = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ThreeCardPoker|Payout")
	int32 StraightMultiplier = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ThreeCardPoker|Payout")
	int32 ThreeOfAKindMultiplier = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ThreeCardPoker|Payout")
	int32 StraightFlushMultiplier = 40;
};

/** Ante Bonus payout multipliers, paid on top of the Ante's own result whenever the player's hand
 * qualifies (Straight or better), regardless of Play/Fold or the dealer's hand. */
USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FThreeCardPokerAnteBonusPayouts
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ThreeCardPoker|Payout")
	int32 StraightMultiplier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ThreeCardPoker|Payout")
	int32 ThreeOfAKindMultiplier = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ThreeCardPoker|Payout")
	int32 StraightFlushMultiplier = 5;
};
