// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Blackjack/BlackjackTypes.h"
#include "BlackjackTableActor.generated.h"

class Acasino_simulatorCharacter;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBlackjackTableChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBlackjackRoundCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlackjackSeatChanged, int32, SeatIndex, const FBlackjackSeatState&, SeatState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlackjackCardDealt, int32, SeatIndex, const FBlackjackCard&, Card);

/**
 * Server-owned blackjack table state for a 4-seat, mostly-3D blackjack setup.
 *
 * BP owns presentation: seat/stand positioning, 3D cards/chips, and minimal controls.
 * This actor owns rules/state: shoe, seats, hands, bets, hit/stand/dealer resolve.
 */
UCLASS()
class CASINO_SIMULATOR_API ABlackjackTableActor : public AActor
{
	GENERATED_BODY()

public:
	ABlackjackTableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Blackjack|Seats")
	bool TryClaimSeat(Acasino_simulatorCharacter* Player, int32 SeatIndex);

	UFUNCTION(BlueprintPure, Category="Blackjack|Seats")
	EBlackjackSeatClaimResult GetSeatClaimResult(Acasino_simulatorCharacter* Player, int32 SeatIndex) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Seats")
	bool CanClaimSeat(Acasino_simulatorCharacter* Player, int32 SeatIndex) const;

	UFUNCTION(BlueprintCallable, Category="Blackjack|Seats")
	void LeaveSeat(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintPure, Category="Blackjack|Seats")
	bool CanLeaveSeat(Acasino_simulatorCharacter* Player) const;

	UFUNCTION(BlueprintCallable, Category="Blackjack|Seats")
	bool ToggleLeaveAfterRound(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Seats")
	int32 ReleaseSeatsLeavingAfterRound();

	UFUNCTION(BlueprintPure, Category="Blackjack|Seats")
	int32 GetSeatIndexForPlayer(Acasino_simulatorCharacter* Player) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Seats")
	bool IsLeaveAfterRoundRequested(Acasino_simulatorCharacter* Player) const;

	UFUNCTION(BlueprintCallable, Category="Blackjack|Seats")
	bool ToggleSitOut(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintPure, Category="Blackjack|Seats")
	bool IsSitOutRequested(Acasino_simulatorCharacter* Player) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Seats")
	bool IsSeatAvailable(int32 SeatIndex) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Seats")
	const TArray<FBlackjackSeatState>& GetSeats() const { return Seats; }

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	bool PlaceBet(Acasino_simulatorCharacter* Player, int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	bool StartRound();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	bool PlayerHit(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	bool PlayerStand(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	bool PlayerDoubleDown(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	bool PlayerSplit(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	bool PlaceInsurance(Acasino_simulatorCharacter* Player, int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	bool SkipInsurance(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Round")
	void ResetRound();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Betting")
	bool StartBettingWindow(float DurationSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Betting")
	void FinishBettingWindow();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Betting")
	bool ExtendBettingWindow(float MinRemainingSeconds);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Betting")
	bool NotifyBettingInteractionStarted(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintPure, Category="Blackjack|Betting")
	bool HasAnyBettingPlayer() const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Betting")
	bool AreAllSeatedPlayersDecided() const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Betting")
	bool IsBettingWindowOpen() const { return bBettingWindowOpen; }

	UFUNCTION(BlueprintPure, Category="Blackjack|Betting")
	float GetBettingRemainingTime() const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Round")
	EBlackjackRoundState GetRoundState() const { return RoundState; }

	UFUNCTION(BlueprintPure, Category="Blackjack|Round")
	const FBlackjackHand& GetDealerHand() const { return DealerHand; }

	UFUNCTION(BlueprintPure, Category="Blackjack|Rules")
	int32 GetHandBestValue(const FBlackjackHand& Hand) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Rules")
	bool IsHandBust(const FBlackjackHand& Hand) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Rules")
	bool IsNaturalBlackjack(const FBlackjackHand& Hand) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Rules")
	bool CanSplitSeat(int32 SeatIndex) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Rules")
	bool CanOfferInsurance() const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Rules")
	bool IsPlayerTurn(Acasino_simulatorCharacter* Player) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Layout")
	USceneComponent* GetSeatPoint(int32 SeatIndex) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Layout")
	USceneComponent* GetStandBackPoint() const { return StandBackPoint; }

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackTableChanged OnTableChanged;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackSeatChanged OnSeatChanged;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackCardDealt OnPlayerCardDealt;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackCardDealt OnDealerCardDealt;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackCardDealt OnDealerHoleCardRevealed;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackRoundCompleted OnRoundCompleted;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> TableRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Layout")
	TObjectPtr<USceneComponent> StandBackPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Layout")
	TObjectPtr<USceneComponent> SeatPoint0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Layout")
	TObjectPtr<USceneComponent> SeatPoint1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Layout")
	TObjectPtr<USceneComponent> SeatPoint2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Layout")
	TObjectPtr<USceneComponent> SeatPoint3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Layout")
	TObjectPtr<USceneComponent> DealerCardRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Layout")
	TObjectPtr<USceneComponent> DeckPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Rules", meta=(ClampMin="1", ClampMax="8"))
	int32 DeckCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Rules", meta=(ClampMin="1"))
	int32 ShuffleThreshold = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Rules")
	bool bDealerStandsOnSoft17 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Betting", meta=(ClampMin="1.0"))
	float DefaultBettingWindowSeconds = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Betting", meta=(ClampMin="0.0"))
	float MinBettingInteractionSeconds = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Betting", meta=(ClampMin="0.0"))
	float MinAfterBetSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Betting", meta=(ClampMin="1.0"))
	float MaxBettingWindowSeconds = 25.0f;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|State")
	EBlackjackRoundState RoundState = EBlackjackRoundState::WaitingForPlayers;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|State")
	TArray<FBlackjackSeatState> Seats;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|State")
	FBlackjackHand DealerHand;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|State")
	int32 ActiveSeatIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|Betting")
	bool bBettingWindowOpen = false;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|Betting")
	float BettingWindowEndsAtServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|Betting")
	float BettingWindowMaxEndsAtServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack|State")
	TArray<FBlackjackCard> Shoe;

	UPROPERTY(BlueprintReadOnly, Category="Blackjack|State")
	TArray<FBlackjackCard> DiscardPile;

	UFUNCTION()
	void OnRep_TableState();

private:
	void InitializeSeats();
	void BuildAndShuffleShoe();
	bool ShouldShuffleBeforeRound() const;
	FBlackjackCard DrawCard(bool bFaceUp = true);
	void DealCardToSeat(int32 SeatIndex, bool bFaceUp = true);
	void DealCardToDealer(bool bFaceUp = true);
	void AdvanceTurnAfterSeat(int32 SeatIndex);
	void RunDealerAndResolve();
	void ResolveSeats();
	void FinishInsuranceIfReady();
	bool AllInsuranceDecisionsComplete() const;
	bool MoveToNextPlayableHand(int32 CurrentSeatIndex);
	bool IsHandComplete(const FBlackjackHand& Hand) const;
	bool HasAnyNonBustPlayerHand() const;
	bool TryStartRoundFromBettingWindow();
	void ClearBettingWindowTimer();
	void ScheduleBettingWindowTimer();
	void BroadcastSeat(int32 SeatIndex);
	FBlackjackSeatState* FindSeatForPlayer(Acasino_simulatorCharacter* Player);
	const FBlackjackSeatState* FindSeatForPlayer(Acasino_simulatorCharacter* Player) const;
	bool IsValidSeatIndex(int32 SeatIndex) const;
	bool IsRoundActive() const;
	bool IsSeatLockedForCurrentRound(const FBlackjackSeatState& Seat) const;
	float GetServerWorldTimeSeconds() const;
	bool AllActiveSeatsComplete() const;
	int32 GetCardValue(const FBlackjackCard& Card) const;
	bool IsSoft17(const FBlackjackHand& Hand) const;

	FBlackjackHand ServerDealerHand;
	FTimerHandle BettingWindowTimerHandle;
};
