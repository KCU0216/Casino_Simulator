// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InventoryEntry.generated.h"

/**
 * One stack of items inside a PlayerState's inventory.
 * ItemID matches FItemData::UniqueID (not the DataTable RowName).
 */
USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FInventoryEntry
{
	GENERATED_BODY()

	/** Matches FItemData::UniqueID in the item DataTable */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 ItemID = INDEX_NONE;

	/** How many copies of ItemID this stack holds */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;

	FInventoryEntry() {}
	FInventoryEntry(int32 InItemID, int32 InQuantity)
		: ItemID(InItemID)
		, Quantity(InQuantity)
	{}
};
