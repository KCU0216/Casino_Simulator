// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/NPC_Game.h"
#include "NPC_ThreeCardPoker.generated.h"

class AThreeCardPokerTableActor;

/**
 * NPC variant hosting the Three Card Poker minigame (dealer). Mirrors ANPC_Dice: spawns and owns
 * the table actor that holds the actual rules, and forwards Interact/EndInteraction to it so the
 * table knows who it's currently playing with.
 */
UCLASS()
class CASINO_SIMULATOR_API ANPC_ThreeCardPoker : public ANPC_Game
{
	GENERATED_BODY()

public:
	/** Returns the spawned Three Card Poker table this NPC hosts, if any. */
	UFUNCTION(BlueprintPure, Category = "Three Card Poker")
	AThreeCardPokerTableActor* GetThreeCardPokerTable() const { return ThreeCardPokerTableInstance; }

	/** Call when the player leaves/closes the betting UI (walks away, cancels, round fully wraps up).
	 * Clears the table's interacting player and hands ownership back to DefaultOwner. Server-only. */
	UFUNCTION(BlueprintCallable, Category = "Three Card Poker")
	void EndInteraction();

protected:
	/** ThreeCardPokerTableActor class to spawn for this NPC's table (e.g. BP_ThreeCardPokerTable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Three Card Poker")
	TSubclassOf<AThreeCardPokerTableActor> ThreeCardPokerTableClass;

	/** Instance spawned from ThreeCardPokerTableClass on BeginPlay. Replicated so clients can resolve it too. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Three Card Poker", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AThreeCardPokerTableActor> ThreeCardPokerTableInstance;

	/** Offset (relative to this NPC's location) the table is spawned at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Three Card Poker")
	FVector ThreeCardPokerTableOffset = FVector(60.0f, 0.0f, -90.0f);

	/** Added to this NPC's rotation for the table's spawn rotation, so the table can face a
	 * different way than the dealer without having to rotate the dealer itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Three Card Poker")
	FRotator ThreeCardPokerTableRotationOffset = FRotator(0.0f, 90.0f, 0.0f);

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor interface

	//~ Begin ANPC_Base interface
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) override;
	//~ End ANPC_Base interface
};
