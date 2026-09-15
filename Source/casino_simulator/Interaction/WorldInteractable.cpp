// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/WorldInteractable.h"

// Default (no-op) bodies for the BlueprintNativeEvent functions, used by any implementer that doesn't
// need to react (e.g. AWorldInteractableBase doesn't override OnLocalInteract by default; ANPC_Base
// doesn't override OnLocalInteract at all). CanInteract/Interact/GetInteractionPromptText are plain
// virtual (see the class comment in WorldInteractable.h) and need no definition here.

void IWorldInteractable::OnLocalInteract_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
}

void IWorldInteractable::OnInteractionFocusStarted_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
}

void IWorldInteractable::OnInteractionFocusEnded_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
}
