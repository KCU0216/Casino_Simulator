// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/NPC_Game.h"
#include "NPC_Dice.generated.h"

class UStaticMeshComponent;
class ADiceGame;

/**
 * NPC variant hosting the dice minigame (shakes/holds the dice cup).
 */
UCLASS()
class CASINO_SIMULATOR_API ANPC_Dice : public ANPC_Game
{
	GENERATED_BODY()

public:
	ANPC_Dice();

	/** Returns the dice cup prop mesh component. */
	UStaticMeshComponent* GetCupMesh() const { return CupMesh; }

	/** Returns the spawned dice game table this NPC hosts, if any. */
	UFUNCTION(BlueprintPure, Category = "Dice Game")
	ADiceGame* GetDiceGame() const { return DiceGameInstance; }

	/** Stores the player's current selection and bet amount for this round. */
	UFUNCTION(BlueprintCallable, Category = "Dice Game")
	void SetBetValue(int32 Select, int32 Betting);

	UFUNCTION(BlueprintCallable, Category = "Dice Game")
	bool ShowResult(int32 ResultValue);

protected:
	/** Player's currently selected value, set via SetBetValue. */
	UPROPERTY(BlueprintReadOnly, Category = "Dice Game", meta = (AllowPrivateAccess = "true"))
	int32 SelectedValue = 0;

	/** Player's currently staked bet amount, set via SetBetValue. */
	UPROPERTY(BlueprintReadOnly, Category = "Dice Game", meta = (AllowPrivateAccess = "true"))
	int32 BettingAmount = 0;

	/** DiceGame class to spawn for this NPC's table (e.g. BP_DiceGame). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice Game")
	TSubclassOf<ADiceGame> DiceGameClass;

	/** Instance spawned from DiceGameClass on BeginPlay. */
	UPROPERTY(BlueprintReadOnly, Category = "Dice Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ADiceGame> DiceGameInstance;

	/** Offset (relative to this NPC's location) the dice table is spawned at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice Game")
	FVector DiceGameOffset = FVector(-90.0f, 0.0f, -90.0f);

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	//~ End AActor interface

	//~ Begin ANPC_Base interface
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) override;
	//~ End ANPC_Base interface

	/** Player currently playing this round, cached from Interact so ShowResult knows who to pay out. */
	TWeakObjectPtr<Acasino_simulatorCharacter> InteractingPlayer;

private:
	/** Dice cup prop held/attached to this NPC. Assign the mesh asset (e.g. the red plastic cup) per-Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CupMesh;
};
