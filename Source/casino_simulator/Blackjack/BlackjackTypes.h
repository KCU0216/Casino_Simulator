// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlackjackTypes.generated.h"

UENUM(BlueprintType)
enum class EBlackjackSuit : uint8
{
	Clubs UMETA(DisplayName="Clubs"),
	Diamonds UMETA(DisplayName="Diamonds"),
	Hearts UMETA(DisplayName="Hearts"),
	Spades UMETA(DisplayName="Spades")
};

UENUM(BlueprintType)
enum class EBlackjackRank : uint8
{
	None = 0 UMETA(Hidden),
	Ace = 1 UMETA(DisplayName="Ace"),
	Two UMETA(DisplayName="2"),
	Three UMETA(DisplayName="3"),
	Four UMETA(DisplayName="4"),
	Five UMETA(DisplayName="5"),
	Six UMETA(DisplayName="6"),
	Seven UMETA(DisplayName="7"),
	Eight UMETA(DisplayName="8"),
	Nine UMETA(DisplayName="9"),
	Ten UMETA(DisplayName="10"),
	Jack UMETA(DisplayName="Jack"),
	Queen UMETA(DisplayName="Queen"),
	King UMETA(DisplayName="King")
};

UENUM(BlueprintType)
enum class EBlackjackRoundState : uint8
{
	WaitingForPlayers UMETA(DisplayName="Waiting For Players"),
	Betting UMETA(DisplayName="Betting"),
	Dealing UMETA(DisplayName="Dealing"),
	PlayerTurns UMETA(DisplayName="Player Turns"),
	DealerTurn UMETA(DisplayName="Dealer Turn"),
	Resolving UMETA(DisplayName="Resolving"),
	RoundComplete UMETA(DisplayName="Round Complete")
};

UENUM(BlueprintType)
enum class EBlackjackSeatResult : uint8
{
	None UMETA(DisplayName="None"),
	PlayerWin UMETA(DisplayName="Player Win"),
	DealerWin UMETA(DisplayName="Dealer Win"),
	Push UMETA(DisplayName="Push"),
	PlayerBlackjack UMETA(DisplayName="Player Blackjack"),
	PlayerBust UMETA(DisplayName="Player Bust")
};

UENUM(BlueprintType)
enum class EBlackjackSeatClaimResult : uint8
{
	Accepted UMETA(DisplayName="Accepted"),
	InvalidPlayer UMETA(DisplayName="Invalid Player"),
	InvalidSeat UMETA(DisplayName="Invalid Seat"),
	SeatOccupied UMETA(DisplayName="Seat Occupied"),
	PlayerAlreadySeated UMETA(DisplayName="Player Already Seated"),
	RequestFailed UMETA(DisplayName="Request Failed")
};

USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FBlackjackCard
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	EBlackjackSuit Suit = EBlackjackSuit::Clubs;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	EBlackjackRank Rank = EBlackjackRank::Ace;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	bool bFaceUp = true;

	FBlackjackCard() = default;

	FBlackjackCard(EBlackjackSuit InSuit, EBlackjackRank InRank, bool bInFaceUp = true)
		: Suit(InSuit)
		, Rank(InRank)
		, bFaceUp(bInFaceUp)
	{
	}
};

USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FBlackjackHand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	TArray<FBlackjackCard> Cards;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	bool bStood = false;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	bool bDoubledDown = false;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	bool bFromSplit = false;
};

USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FBlackjackSeatState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	int32 SeatIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	TObjectPtr<AActor> Occupant = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	TArray<FBlackjackHand> Hands;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	int32 ActiveHandIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	int32 BetAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	int32 InsuranceBetAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	EBlackjackSeatResult LastResult = EBlackjackSeatResult::None;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	bool bReadyForRound = false;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack")
	bool bHasSplitThisRound = false;

	bool IsOccupied() const { return Occupant != nullptr; }
};
