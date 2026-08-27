// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Item/InventoryEntry.h"
#include "casino_simulatorPlayerState.generated.h"

class UDataTable;
struct FItemData;

/** Broadcast on both server and clients whenever the inventory contents change, so UI (e.g. WBP_Inventory) can refresh. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

/**
 * Owns the items a player is currently holding (inventory), independent of round/level.
 * Storage is a variable list of stacks (FInventoryEntry), keyed by FItemData::UniqueID.
 * Add/RemoveItem are server-authoritative; the array replicates (owner-only) so the owning
 * client's UI can read it directly and react to OnInventoryChanged.
 *
 * Using items (applying OnUseEffect via the owning Ability System Component) is intentionally
 * out of scope here and will be wired up separately.
 */
UCLASS()
class CASINO_SIMULATOR_API Acasino_simulatorPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	Acasino_simulatorPlayerState();

	/** DataTable of FItemData rows this inventory resolves ItemIDs against (RowName is arbitrary; FItemData::UniqueID is the actual key) */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	/** Read-only access to the current inventory entries (for UI) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	const TArray<FInventoryEntry>& GetInventory() const { return Inventory; }

	/**
	 * Adds Quantity of ItemID to the inventory: fills existing stacks up to each item's
	 * MaxStackSize first, then creates new stacks for the remainder. Server-only.
	 * @return the amount actually added (always equal to Quantity; kept as a return value for future partial-add cases, e.g. inventory caps).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(int32 ItemID, int32 Quantity);

	/**
	 * Removes up to Quantity of ItemID from the inventory, draining stacks until satisfied
	 * or none are left. Server-only.
	 * @return the amount actually removed (may be less than Quantity if not enough was held).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItem(int32 ItemID, int32 Quantity);

	/** Total quantity of ItemID currently held, summed across all stacks */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemQuantity(int32 ItemID) const;

	/** Looks up the static FItemData row whose UniqueID matches ItemID in ItemDataTable */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool FindItemData(int32 ItemID, FItemData& OutItemData) const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Inventory, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryEntry> Inventory;

	UFUNCTION()
	void OnRep_Inventory();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NumberSlots")
	TArray<int> NumberSlots = {0,1};

	//~ Begin AActor interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor interface
};
