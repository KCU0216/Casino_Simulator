// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blackjack/BlackjackTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlackjackBlueprintLibrary.generated.h"

class UDataTable;

UCLASS()
class CASINO_SIMULATOR_API UBlackjackBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blackjack|Visual")
	static FName GetBlackjackCardVisualRowName(EBlackjackDeckStyle DeckStyle, const FBlackjackCard& Card);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blackjack|Visual")
	static bool FindBlackjackCardVisualData(UDataTable* CardVisualDataTable, EBlackjackDeckStyle DeckStyle, const FBlackjackCard& Card, FBlackjackCardVisualData& OutVisualData);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blackjack|Visual")
	static FName GetBlackjackDeckStyleName(EBlackjackDeckStyle DeckStyle);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blackjack|Visual")
	static FName GetBlackjackSuitName(EBlackjackSuit Suit);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blackjack|Visual")
	static FName GetBlackjackRankName(EBlackjackRank Rank);
};
