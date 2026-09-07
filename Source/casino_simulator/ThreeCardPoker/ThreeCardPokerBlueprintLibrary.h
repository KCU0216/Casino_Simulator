// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThreeCardPoker/ThreeCardPokerTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThreeCardPokerBlueprintLibrary.generated.h"

class Acasino_simulatorCharacter;
class AThreeCardPokerTableActor;

/**
 * BP-facing helpers for Three Card Poker hand display. Card visuals (front/back textures) are
 * looked up via the existing UBlackjackBlueprintLibrary::FindBlackjackCardVisualData against
 * DT_BlackjackCardVisuals — this library only adds what's specific to poker hand display.
 */
UCLASS()
class CASINO_SIMULATOR_API UThreeCardPokerBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Human-readable name for a hand rank (e.g. "Straight Flush"), for UI text. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThreeCardPoker|Visual")
	static FText GetThreeCardPokerHandRankDisplayName(EThreeCardPokerHandRank Rank);

	/** Resolves the table a player is currently interacting with, in one call, so BP betting UI
	 * doesn't need to chain GetOwningPlayer -> Cast -> CurrentInteractionTarget -> Cast -> GetTable
	 * as separate graph nodes. Returns null if Player isn't currently interacting with a Three Card
	 * Poker dealer NPC. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThreeCardPoker")
	static AThreeCardPokerTableActor* GetThreeCardPokerTableForPlayer(Acasino_simulatorCharacter* Player);

	/** "Hand: Flush" once 3 cards are dealt, "Hand: -" otherwise (or if Table is null). For binding
	 * directly to a UMG text block's Text property. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThreeCardPoker|Visual")
	static FText GetThreeCardPokerHandRankText(AThreeCardPokerTableActor* Table);

	/** Round result text once the round is complete, empty otherwise (or if Table is null). For
	 * binding directly to a UMG text block's Text property. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="ThreeCardPoker|Visual")
	static FText GetThreeCardPokerResultText(AThreeCardPokerTableActor* Table);
};
