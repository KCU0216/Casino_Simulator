// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/InventoryEntry.h"
#include "casino_simulatorPlayerHUD.generated.h"

/**
 *  Player HUD widget: displays the player's nicotine/alcohol levels.
 *  Kept passive - it doesn't source its own data. Acasino_simulatorPlayerController pushes
 *  Current/Max updates into it whenever the underlying attributes change; Blueprint is
 *  responsible for turning that into a ProgressBar Percent (CurrentValue / MaxValue).
 */
UCLASS(abstract)
class CASINO_SIMULATOR_API Ucasino_simulatorPlayerHUD : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Passes control to Blueprint to update the nicotine progress bar */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD", meta = (DisplayName = "Nicotine Updated"))
	void BP_NicotineUpdated(float CurrentValue, float MaxValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD", meta = (DisplayName = "Nicotine Count"))
	void BP_Slot_1Count(int Value);

	/** Passes control to Blueprint to update the alcohol progress bar */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD", meta = (DisplayName = "Alcohol Updated"))
	void BP_AlcoholUpdated(float CurrentValue, float MaxValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD", meta = (DisplayName = "Alcohol Count"))
	void BP_Slot_2Count(int Value);

	/** Passes control to Blueprint to update the currency text */
	UFUNCTION(BlueprintImplementableEvent, Category="HUD", meta = (DisplayName = "Currency Updated"))
	void BP_CurrencyUpdated(float NewValue);
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD", meta = (DisplayName = "Open Interection"))
	void BP_OpenInterection();
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD", meta = (DisplayName = "Close Interection"))
	void BP_CloseInterection();

	/** Passes the full current inventory to Blueprint whenever PlayerState's inventory changes, so item slot widgets (e.g. WBP_ItemSlot) can be rebuilt/refreshed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD", meta = (DisplayName = "Inventory Updated"))
	void BP_InventoryUpdated(const TArray<FInventoryEntry>& Inventory);
};
