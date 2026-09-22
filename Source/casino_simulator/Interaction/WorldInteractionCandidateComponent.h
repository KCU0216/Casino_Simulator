// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldInteractionCandidateComponent.generated.h"

class Acasino_simulatorCharacter;

/**
 * Shared helper for the three IWorldInteractable implementers (AWorldInteractableBase, ANPC_Base,
 * Acasino_simulatorCharacter): registers/unregisters this component's owner as an interaction
 * candidate on another character's UWorldInteractionDetectorComponent.
 *
 * Each owner still decides *when* to call these - its own overlap-gating logic (occupancy, NPCType,
 * "not self", etc.) stays exactly where it was in OnInteractionSphereBeginOverlap/EndOverlap. This
 * component only replaces the copy-pasted "find the other character's detector and call
 * RegisterCandidate/UnregisterCandidate" body that used to be duplicated in all three classes.
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UWorldInteractionCandidateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldInteractionCandidateComponent();

	/** Registers this component's owner (which must implement IWorldInteractable) as a candidate on
	 * OverlappingCharacter's detector. No-ops if OverlappingCharacter or its detector is missing. */
	void RegisterOwnerAsCandidate(Acasino_simulatorCharacter* OverlappingCharacter) const;

	/** Inverse of RegisterOwnerAsCandidate. */
	void UnregisterOwnerAsCandidate(Acasino_simulatorCharacter* OverlappingCharacter) const;
};
