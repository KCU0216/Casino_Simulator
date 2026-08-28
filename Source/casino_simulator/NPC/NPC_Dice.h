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

	/** Betting UI entry point: spends the bet from Player's currency and records the selection, on
	 * both server and client. NPC_Dice isn't owned by any player's connection, so a client can't
	 * call a Server RPC declared here directly - on a client this forwards through Player's own
	 * ServerPlaceDiceBet (Player's Character IS owned by that client's connection). */
	UFUNCTION(BlueprintCallable, Category = "Dice Game")
	bool PlaceBet(Acasino_simulatorCharacter* Player, int32 Select, int32 Betting);

	/** Authoritative half of PlaceBet: validates Player is the one currently interacting with this
	 * NPC, spends their currency, and records the selection. Server-only. Called directly by
	 * PlaceBet on the server, and by Acasino_simulatorCharacter::ServerPlaceDiceBet on behalf of a
	 * client's request. */
	bool ExecutePlaceBet(Acasino_simulatorCharacter* Player, int32 Select, int32 Betting);

	UFUNCTION(BlueprintCallable, Category = "Dice Game")
	bool ShowResult(int32 ResultValue);

	/** Call when the player leaves/closes the betting UI (walks away, cancels, round fully wraps up).
	 * Clears InteractingPlayer and hands ownership of this NPC back to DefaultOwner. Server-only. */
	UFUNCTION(BlueprintCallable, Category = "Dice Game")
	void EndInteraction();

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

	/** Instance spawned from DiceGameClass on BeginPlay. Replicated so clients can resolve it via GetDiceGame() too. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dice Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ADiceGame> DiceGameInstance;

	/** Offset (relative to this NPC's location) the dice table is spawned at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice Game")
	FVector DiceGameOffset = FVector(-90.0f, 0.0f, -90.0f);

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor interface

	//~ Begin ANPC_Base interface
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) override;
	//~ End ANPC_Base interface

	/** Player currently playing this round, cached from Interact so ShowResult knows who to pay out. */
	TWeakObjectPtr<Acasino_simulatorCharacter> InteractingPlayer;

private:
	/** Sets/clears InteractingPlayer and hands this NPC's ownership to Player (or back to DefaultOwner
	 * when Player is null). Owning the NPC gives that specific client's connection ROLE_AutonomousProxy
	 * for it, which is what lets a Server RPC declared directly on ANPC_Dice actually reach the server
	 * from that client. Server-only. */
	void SetInteractingPlayer(Acasino_simulatorCharacter* Player);

	/** This NPC's owner before any player starts interacting with it (captured in BeginPlay); restored by EndInteraction. */
	TWeakObjectPtr<AActor> DefaultOwner;

	/** Stores the player's current selection and bet amount for this round. Server-only, called from ExecutePlaceBet. */
	void SetBetValue(int32 Select, int32 Betting);

	/** Dice cup prop held/attached to this NPC. Assign the mesh asset (e.g. the red plastic cup) per-Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CupMesh;
};
