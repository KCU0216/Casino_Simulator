// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/WorldInteractionCandidateComponent.h"

#include "Interaction/WorldInteractable.h"
#include "Interaction/WorldInteractionDetectorComponent.h"
#include "casino_simulatorCharacter.h"

UWorldInteractionCandidateComponent::UWorldInteractionCandidateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldInteractionCandidateComponent::RegisterOwnerAsCandidate(Acasino_simulatorCharacter* OverlappingCharacter) const
{
	if (!OverlappingCharacter)
	{
		return;
	}

	if (UWorldInteractionDetectorComponent* Detector = OverlappingCharacter->GetWorldInteractionDetector())
	{
		Detector->RegisterCandidate(TScriptInterface<IWorldInteractable>(GetOwner()));
	}
}

void UWorldInteractionCandidateComponent::UnregisterOwnerAsCandidate(Acasino_simulatorCharacter* OverlappingCharacter) const
{
	if (!OverlappingCharacter)
	{
		return;
	}

	if (UWorldInteractionDetectorComponent* Detector = OverlappingCharacter->GetWorldInteractionDetector())
	{
		Detector->UnregisterCandidate(TScriptInterface<IWorldInteractable>(GetOwner()));
	}
}
