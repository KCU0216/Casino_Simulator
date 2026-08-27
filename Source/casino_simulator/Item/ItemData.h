// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "ItemData.generated.h"

/** Broad category used for high-level branching (UI grouping, etc). Finer routing (e.g. cigarette vs alcohol) should use ItemCategory instead. */
//UENUM(BlueprintType)
//enum class EItemType : uint8
//{
//	Currency,
//	Consumable,
//	Chip,
//	Misc
//};

/**
 * Static definition of a single item, authored as one row of an item DataTable (RowName = ItemID).
 * Holds only data that's the same for every copy of the item - how many the player is
 * currently holding lives in the inventory's own slot data instead, not here.
 */
USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int UniqueID;

	/** Display name shown in inventory/interaction UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	/** Flavor text / tooltip description */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta = (MultiLine = "true"))
	FText Description;

	/** Icon shown in inventory UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Broad category, mainly for UI grouping/filtering */
	/*UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EItemType ItemType = EItemType::Misc;*/

	/** Finer-grained category (e.g. Item.Consumable.Cigarette, Item.Consumable.Alcohol) used to route use/interaction logic without hardcoding item IDs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FGameplayTag ItemCategory;

	/** Whether multiple copies of this item stack into a single inventory slot */
	/*UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	bool bStackable = true;*/

	/** Max quantity per stack when bStackable is true */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta = (EditCondition = "bStackable", ClampMin = "1"))
	int32 MaxStackSize = 99;

	/** Whether using this item consumes one copy of it (true for cigarettes/alcohol, false for e.g. key items) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Use")
	bool bConsumeOnUse = false;

	/** GameplayEffect applied to the owning AbilitySystemComponent when this item is used (e.g. a shared GE_ConsumeNicotine / GE_ConsumeAlcohol) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Use", meta = (EditCondition = "bConsumeOnUse"))
	TSubclassOf<UGameplayEffect> OnUseEffect;

	/** Magnitude passed into OnUseEffect via SetByCaller (e.g. how much Nicotine/Alcohol this specific item restores) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Use", meta = (EditCondition = "bConsumeOnUse"))
	float EffectMagnitude = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bConsumeOnUse"))
	float CoolTime = 0.0f;

	/** Shop price, for buying/selling at NPCs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	int32 Price = 0;
};
