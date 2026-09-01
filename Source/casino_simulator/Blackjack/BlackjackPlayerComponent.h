// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blackjack/BlackjackTypes.h"
#include "BlackjackPlayerComponent.generated.h"

class ABlackjackTableActor;
class Acasino_simulatorCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlackjackSeatModeChanged, ABlackjackTableActor*, Table, int32, SeatIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlackjackSeatExitRequested, ABlackjackTableActor*, Table, int32, SeatIndex);

/**
 * Player-side blackjack seat state.
 *
 * The table still owns authoritative blackjack rules and seat occupancy. This component only keeps
 * the local player in a seated blackjack mode: movement locked, look input kept alive, and Q exits.
 */
UCLASS(ClassGroup=(Blackjack), meta=(BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UBlackjackPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlackjackPlayerComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Blackjack|Seat")
	void EnterBlackjackSeatMode(ABlackjackTableActor* Table, int32 SeatIndex);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Seat")
	void RequestExitBlackjackSeat();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Seat")
	void CompleteExitBlackjackSeat();

	UFUNCTION(BlueprintPure, Category="Blackjack|Seat")
	bool IsInBlackjackSeat() const { return CurrentBlackjackTable != nullptr && CurrentSeatIndex != INDEX_NONE; }

	UFUNCTION(BlueprintPure, Category="Blackjack|Seat")
	ABlackjackTableActor* GetCurrentBlackjackTable() const { return CurrentBlackjackTable; }

	UFUNCTION(BlueprintPure, Category="Blackjack|Seat")
	int32 GetCurrentSeatIndex() const { return CurrentSeatIndex; }

	UFUNCTION(BlueprintCallable, Category="Blackjack|Actions")
	bool PlaceBet(int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Actions")
	bool StartRound();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Actions")
	bool Hit();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Actions")
	bool Stand();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Actions")
	bool DoubleDown();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Actions")
	bool Split();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Actions")
	bool PlaceInsurance(int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Actions")
	bool SkipInsurance();

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Seat")
	FBlackjackSeatModeChanged OnBlackjackSeatModeStarted;

	UPROPERTY(BlueprintAssignable, Category="Blackjack|Seat")
	FBlackjackSeatModeChanged OnBlackjackSeatModeEnded;

	/**
	 * Fired when Q asks to leave the table. If nothing is bound, the component exits immediately.
	 * Bind this from BP when a stand-up montage should play before calling CompleteExitBlackjackSeat.
	 */
	UPROPERTY(BlueprintAssignable, Category="Blackjack|Seat")
	FBlackjackSeatExitRequested OnBlackjackSeatExitRequested;

protected:
	UFUNCTION(Server, Reliable)
	void ServerEnterBlackjackSeatMode(ABlackjackTableActor* Table, int32 SeatIndex);

	UFUNCTION(Server, Reliable)
	void ServerCompleteExitBlackjackSeat();

	UFUNCTION(Server, Reliable)
	void ServerPlaceBet(int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerStartRound();

	UFUNCTION(Server, Reliable)
	void ServerHit();

	UFUNCTION(Server, Reliable)
	void ServerStand();

	UFUNCTION(Server, Reliable)
	void ServerDoubleDown();

	UFUNCTION(Server, Reliable)
	void ServerSplit();

	UFUNCTION(Server, Reliable)
	void ServerPlaceInsurance(int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerSkipInsurance();

	UFUNCTION()
	void OnRep_BlackjackSeatMode();

private:
	UPROPERTY(ReplicatedUsing=OnRep_BlackjackSeatMode)
	TObjectPtr<ABlackjackTableActor> CurrentBlackjackTable;

	UPROPERTY(ReplicatedUsing=OnRep_BlackjackSeatMode)
	int32 CurrentSeatIndex = INDEX_NONE;

	bool bMovementLockApplied = false;

	Acasino_simulatorCharacter* GetOwnerCharacter() const;
	void SetBlackjackSeatMode(ABlackjackTableActor* Table, int32 SeatIndex);
	void ClearBlackjackSeatMode();
	void ApplyMovementLock();
	void ClearMovementLock();
	bool CanLeaveCurrentSeat() const;
	bool ExecutePlaceBet(int32 Amount);
	bool ExecuteStartRound();
	bool ExecuteHit();
	bool ExecuteStand();
	bool ExecuteDoubleDown();
	bool ExecuteSplit();
	bool ExecutePlaceInsurance(int32 Amount);
	bool ExecuteSkipInsurance();
};
