// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blackjack/BlackjackTypes.h"
#include "NPC/NPC_Base.h"
#include "BlackjackSeatInteractionActor.generated.h"

class ABlackjackTableActor;
class Acasino_simulatorCharacter;

/**
 * Interaction entry for one blackjack seat.
 *
 * The table owns rules and seat state. This actor only gives the player an E-interaction
 * target for a specific seat index, then reports the claim result back to Blueprint.
 */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API ABlackjackSeatInteractionActor : public ANPC_Base
{
	GENERATED_BODY()

public:
	ABlackjackSeatInteractionActor();

	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) override;

	UFUNCTION(BlueprintPure, Category="Blackjack|Seat")
	ABlackjackTableActor* GetBlackjackTable() const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Seat")
	int32 GetSeatIndex() const { return SeatIndex; }

	UFUNCTION(BlueprintPure, Category="Blackjack|Seat")
	EBlackjackSeatClaimResult GetCurrentClaimResult(Acasino_simulatorCharacter* InteractingCharacter) const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Seat", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ABlackjackTableActor> BlackjackTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Seat", meta=(ClampMin="0", ClampMax="3", AllowPrivateAccess="true"))
	int32 SeatIndex = 0;

	UFUNCTION(BlueprintImplementableEvent, Category="Blackjack|Seat", meta=(DisplayName="On Seat Claim Succeeded"))
	void BP_OnSeatClaimSucceeded(Acasino_simulatorCharacter* InteractingCharacter, ABlackjackTableActor* Table, int32 ClaimedSeatIndex);

	UFUNCTION(BlueprintImplementableEvent, Category="Blackjack|Seat", meta=(DisplayName="On Seat Claim Failed"))
	void BP_OnSeatClaimFailed(Acasino_simulatorCharacter* InteractingCharacter, EBlackjackSeatClaimResult Result);

private:
	ABlackjackTableActor* ResolveBlackjackTable() const;
};
