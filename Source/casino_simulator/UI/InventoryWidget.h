// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"

/**
 *  Native parent for the inventory UI (e.g. WBP_Inventory). Empty for now - reparent the
 *  Blueprint to this class and add C++ functionality here as it's needed.
 */
UCLASS(abstract)
class CASINO_SIMULATOR_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Passes control to Blueprint to refresh every slot/element in the inventory UI. */
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory", meta = (DisplayName = "All Refresh"))
	void BP_AllRefresh();
};
