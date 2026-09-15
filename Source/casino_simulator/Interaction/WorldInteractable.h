// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WorldInteractable.generated.h"

class Acasino_simulatorCharacter;

UINTERFACE(BlueprintType)
class CASINO_SIMULATOR_API UWorldInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Shared contract for anything a player can walk up to, aim at, and press E on - world props/machines
 * (AWorldInteractableBase and its children: ASeatedMachineBase, AThreeCardPokerTableActor,
 * ABlackjackTableInteractionActor, ...) and NPCs (ANPC_Base and its children) alike.
 *
 * UWorldInteractionDetectorComponent drives candidate registration and line-trace focus resolution
 * through this interface, so both families share one detection pipeline instead of maintaining two
 * parallel ones. The player controller reads the focused target when E is pressed and routes it to
 * the matching interaction request. Each family still owns its own focus reaction (e.g.
 * AWorldInteractableBase and ANPC_Base both route to the player's HUD prompt flow) - only the
 * plumbing that discovers a target is shared.
 *
 * CanInteract/Interact/GetInteractionPromptText are plain virtual (not BlueprintNativeEvent, matching
 * what they were directly on AWorldInteractableBase before this interface existed) - callable directly
 * as normal C++ virtual dispatch through any pointer typed to this interface or a concrete
 * implementing class. OnLocalInteract/OnInteractionFocusStarted/OnInteractionFocusEnded stay
 * BlueprintNativeEvent (also unchanged from before) and are invoked via
 * IWorldInteractable::Execute_FunctionName(Object, Args) wherever the caller only has a generic
 * UObject*, since that's the only way to also reach a Blueprint-graph-only override.
 */
class CASINO_SIMULATOR_API IWorldInteractable
{
	GENERATED_BODY()

public:
	/** Whether InteractingCharacter is currently allowed to interact with this target (range, occupancy, etc.).
	 * Plain virtual, not a UFUNCTION - matches IShooterWeaponHolder's pattern elsewhere in this project:
	 * a C++-only interface function needs no UFUNCTION at all (on this declaration or any override), and
	 * is called via normal virtual dispatch (Target->CanInteract(...)), never Execute_. */
	virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const = 0;

	/** Server-authoritative interaction entry point. */
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) = 0;

	/** Prompt text shown in the shared PlayerHUDWidget prompt while this target is focused.
	 * Defaults to empty, which lets the player controller use its generic interaction text. */
	virtual FText GetInteractionPromptText() const { return FText::GetEmpty(); }

	/** Local-only cosmetic hook, run on the interacting player's own machine before the server call lands. */
	UFUNCTION(BlueprintNativeEvent, Category = "World Interaction")
	void OnLocalInteract(Acasino_simulatorCharacter* InteractingCharacter);
	virtual void OnLocalInteract_Implementation(Acasino_simulatorCharacter* InteractingCharacter);

	/** This target became what the player is currently aiming/focused at. */
	UFUNCTION(BlueprintNativeEvent, Category = "World Interaction")
	void OnInteractionFocusStarted(Acasino_simulatorCharacter* InteractingCharacter);
	virtual void OnInteractionFocusStarted_Implementation(Acasino_simulatorCharacter* InteractingCharacter);

	/** This target stopped being what the player is aiming/focused at. */
	UFUNCTION(BlueprintNativeEvent, Category = "World Interaction")
	void OnInteractionFocusEnded(Acasino_simulatorCharacter* InteractingCharacter);
	virtual void OnInteractionFocusEnded_Implementation(Acasino_simulatorCharacter* InteractingCharacter);
};
