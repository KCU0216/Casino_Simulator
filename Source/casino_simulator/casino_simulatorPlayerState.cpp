// Copyright Epic Games, Inc. All Rights Reserved.

#include "casino_simulatorPlayerState.h"
#include "Item/ItemData.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"

Acasino_simulatorPlayerState::Acasino_simulatorPlayerState()
{
}

void Acasino_simulatorPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Owner-only: a player's inventory contents aren't relevant to other clients.
	DOREPLIFETIME_CONDITION(Acasino_simulatorPlayerState, Inventory, COND_OwnerOnly);
}

bool Acasino_simulatorPlayerState::FindItemData(int32 ItemID, FItemData& OutItemData) const
{
	if (!ItemDataTable)
	{
		return false;
	}

	for (const TPair<FName, uint8*>& Row : ItemDataTable->GetRowMap())
	{
		const FItemData* RowData = reinterpret_cast<FItemData*>(Row.Value);
		if (RowData && RowData->UniqueID == ItemID)
		{
			OutItemData = *RowData;
			return true;
		}
	}

	return false;
}

int32 Acasino_simulatorPlayerState::GetItemQuantity(int32 ItemID) const
{
	int32 Total = 0;
	for (const FInventoryEntry& Entry : Inventory)
	{
		if (Entry.ItemID == ItemID)
		{
			Total += Entry.Quantity;
		}
	}
	return Total;
}

int32 Acasino_simulatorPlayerState::AddItem(int32 ItemID, int32 Quantity)
{
	if (!HasAuthority() || Quantity <= 0)
	{
		return 0;
	}

	FItemData ItemData;
	const int32 MaxStackSize = FindItemData(ItemID, ItemData) ? FMath::Max(1, ItemData.MaxStackSize) : TNumericLimits<int32>::Max();

	int32 Remaining = Quantity;

	// Top up existing stacks of this item first.
	for (FInventoryEntry& Entry : Inventory)
	{
		if (Remaining <= 0)
		{
			break;
		}

		if (Entry.ItemID == ItemID && Entry.Quantity < MaxStackSize)
		{
			const int32 SpaceInStack = MaxStackSize - Entry.Quantity;
			const int32 AmountToAdd = FMath::Min(SpaceInStack, Remaining);
			Entry.Quantity += AmountToAdd;
			Remaining -= AmountToAdd;
		}
	}

	// Anything left over starts new stacks.
	while (Remaining > 0)
	{
		const int32 AmountToAdd = FMath::Min(MaxStackSize, Remaining);
		Inventory.Add(FInventoryEntry(ItemID, AmountToAdd));
		Remaining -= AmountToAdd;
	}

	// ReplicatedUsing only fires on clients; broadcast here too so the server's own listeners (e.g. its own UI) see the change.
	OnRep_Inventory();

	return Quantity - Remaining;
}

int32 Acasino_simulatorPlayerState::RemoveItem(int32 ItemID, int32 Quantity)
{
	if (!HasAuthority() || Quantity <= 0)
	{
		return 0;
	}

	int32 Remaining = Quantity;

	for (int32 Index = Inventory.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FInventoryEntry& Entry = Inventory[Index];
		if (Entry.ItemID != ItemID)
		{
			continue;
		}

		const int32 AmountToRemove = FMath::Min(Entry.Quantity, Remaining);
		Entry.Quantity -= AmountToRemove;
		Remaining -= AmountToRemove;

		if (Entry.Quantity <= 0)
		{
			Inventory.RemoveAt(Index);
		}
	}

	const int32 AmountRemoved = Quantity - Remaining;
	if (AmountRemoved > 0)
	{
		OnRep_Inventory();
	}

	return AmountRemoved;
}

void Acasino_simulatorPlayerState::OnRep_Inventory()
{
	OnInventoryChanged.Broadcast();
}
