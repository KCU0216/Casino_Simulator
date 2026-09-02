// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/WorldInteractableBase.h"
#include "BlackjackTableInteractionActor.generated.h"

class ABlackjackTableActor;
class UBlackjackPlayerComponent;

UENUM(BlueprintType)
enum class EBlackjackTableInteractionAction : uint8
{
	OpenBetting UMETA(DisplayName="Open Betting"),
	BetFixedAmount UMETA(DisplayName="Bet Fixed Amount"),
	SitOut UMETA(DisplayName="Sit Out"),
	Hit UMETA(DisplayName="Hit"),
	Stand UMETA(DisplayName="Stand"),
	DoubleDown UMETA(DisplayName="Double Down"),
	Split UMETA(DisplayName="Split"),
	ExitSeat UMETA(DisplayName="Exit Seat"),
	InspectCards UMETA(DisplayName="Inspect Cards")
};

/**
 * Line-trace target for blackjack table controls.
 *
 * Drop BP children of this actor onto chips, table tokens, and card inspect zones.
 * The existing world interaction detector handles camera focus and E-click routing.
 */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API ABlackjackTableInteractionActor : public AWorldInteractableBase
{
	GENERATED_BODY()

public:
	ABlackjackTableInteractionActor();

	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) override;
	virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const override;
	virtual void OnLocalInteract_Implementation(Acasino_simulatorCharacter* InteractingCharacter) override;

	UFUNCTION(BlueprintPure, Category="Blackjack|Interaction")
	ABlackjackTableActor* GetBlackjackTable() const;

	UFUNCTION(BlueprintPure, Category="Blackjack|Interaction")
	EBlackjackTableInteractionAction GetInteractionAction() const { return InteractionAction; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Interaction")
	TObjectPtr<ABlackjackTableActor> BlackjackTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Interaction")
	EBlackjackTableInteractionAction InteractionAction = EBlackjackTableInteractionAction::OpenBetting;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Interaction", meta=(ClampMin="1"))
	int32 FixedBetAmount = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blackjack|Interaction")
	bool bUseActionPromptText = true;

	UFUNCTION(BlueprintImplementableEvent, Category="Blackjack|Interaction", meta=(DisplayName="On Local Blackjack Interact"))
	void BP_OnLocalBlackjackInteract(Acasino_simulatorCharacter* InteractingCharacter, ABlackjackTableActor* Table, EBlackjackTableInteractionAction Action);

	UFUNCTION(BlueprintImplementableEvent, Category="Blackjack|Interaction", meta=(DisplayName="On Blackjack Interaction Performed"))
	void BP_OnBlackjackInteractionPerformed(Acasino_simulatorCharacter* InteractingCharacter, ABlackjackTableActor* Table, EBlackjackTableInteractionAction Action);

	UFUNCTION(BlueprintImplementableEvent, Category="Blackjack|Interaction", meta=(DisplayName="On Blackjack Interaction Rejected"))
	void BP_OnBlackjackInteractionRejected(Acasino_simulatorCharacter* InteractingCharacter, ABlackjackTableActor* Table, EBlackjackTableInteractionAction Action);

private:
	ABlackjackTableActor* ResolveBlackjackTable() const;
	UBlackjackPlayerComponent* GetPlayerBlackjackComponent(Acasino_simulatorCharacter* InteractingCharacter) const;
	FText GetActionPromptText() const;
};
