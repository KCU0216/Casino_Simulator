// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlackjackUIBlueprintLibrary.generated.h"

class ABlackjackTableActor;
class UBorder;
class UUserWidget;
class UVerticalBox;

/** Presentation helpers for the replicated blackjack table snapshot. */
UCLASS()
class CASINO_SIMULATOR_API UBlackjackUIBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Rebuilds the other occupied seats using the HUD's existing RenderCards function. */
	UFUNCTION(BlueprintCallable, Category="Blackjack|UI")
	static bool RefreshOtherPlayers(UUserWidget* BlackjackHUD, ABlackjackTableActor* Table,
		AActor* LocalPlayer, UVerticalBox* OtherPlayersList, UBorder* OtherPlayersPanel);
};
