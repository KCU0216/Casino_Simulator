// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blackjack/BlackjackTypes.h"
#include "BlackjackTableActor.generated.h"

class Acasino_simulatorCharacter;
class UCameraComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBlackjackTableChanged);
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
	int32 GetSeatIndexForPlayer(Acasino_simulatorCharacter* Player) const;

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
	void ResetRound();

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

	UFUNCTION(BlueprintPure, Category="Blackjack|Layout")
	USceneComponent* GetSeatPoint(int32 SeatIndex) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Layout")
	USceneComponent* GetSeatCameraPoint(int32 SeatIndex) const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Layout")
	USceneComponent* GetStandBackPoint() const { return StandBackPoint; }

	UFUNCTION(BlueprintPure, Category="Blackjack|Camera")
	UCameraComponent* GetSeatSelectCamera() const { return nullptr; }

	UFUNCTION(BlueprintPure, Category="Blackjack|Camera")
	UCameraComponent* GetSeatCamera(int32 SeatIndex) const;

	UFUNCTION(BlueprintCallable, Category="Blackjack|Camera")
	void ActivateSeatSelectCamera();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Camera")
	bool ActivateSeatCamera(int32 SeatIndex);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Camera")
	void DeactivateTableCameras();

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackTableChanged OnTableChanged;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackSeatChanged OnSeatChanged;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackCardDealt OnPlayerCardDealt;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Events")
	FBlackjackCardDealt OnDealerCardDealt;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> TableRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Layout")
	TObjectPtr<USceneComponent> SeatSelectCameraPoint;

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

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|State")
	EBlackjackRoundState RoundState = EBlackjackRoundState::WaitingForPlayers;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|State")
	TArray<FBlackjackSeatState> Seats;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|State")
	FBlackjackHand DealerHand;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="Blackjack|State")
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
	void BroadcastSeat(int32 SeatIndex);
	FBlackjackSeatState* FindSeatForPlayer(Acasino_simulatorCharacter* Player);
	const FBlackjackSeatState* FindSeatForPlayer(Acasino_simulatorCharacter* Player) const;
	bool IsValidSeatIndex(int32 SeatIndex) const;
	bool IsRoundActive() const;
	bool AllActiveSeatsComplete() const;
	int32 GetCardValue(const FBlackjackCard& Card) const;
	bool IsSoft17(const FBlackjackHand& Hand) const;
};
