#include "Interaction/WorldInteractionDetectorComponent.h"

#include "casino_simulatorCharacter.h"
#include "Camera/CameraComponent.h"

UWorldInteractionDetectorComponent::UWorldInteractionDetectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWorldInteractionDetectorComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<Acasino_simulatorCharacter>(GetOwner());
}

void UWorldInteractionDetectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Routes through SetFocusedTarget (rather than just clearing FocusedTarget directly) so whichever
	// panel is currently open - NPC's or a world target's, both drive the same PlayerHUDWidget now -
	// gets its OnInteractionFocusEnded and closes properly.
	SetFocusedTarget(TScriptInterface<IWorldInteractable>());
	NearbyTargets.Reset();

	Super::EndPlay(EndPlayReason);
}

void UWorldInteractionDetectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		return;
	}

	UpdateFocusedTarget();
}

void UWorldInteractionDetectorComponent::RegisterCandidate(TScriptInterface<IWorldInteractable> Candidate)
{
	UObject* CandidateObject = Candidate.GetObject();
	if (!CandidateObject)
	{
		return;
	}

	const bool bAlreadyRegistered = NearbyTargets.ContainsByPredicate([CandidateObject](const TScriptInterface<IWorldInteractable>& Existing)
	{
		return Existing.GetObject() == CandidateObject;
	});

	if (!bAlreadyRegistered)
	{
		NearbyTargets.Add(Candidate);
	}
}

void UWorldInteractionDetectorComponent::UnregisterCandidate(TScriptInterface<IWorldInteractable> Candidate)
{
	UObject* CandidateObject = Candidate.GetObject();
	if (!CandidateObject)
	{
		return;
	}

	NearbyTargets.RemoveAll([CandidateObject](const TScriptInterface<IWorldInteractable>& Existing)
	{
		return Existing.GetObject() == CandidateObject;
	});

	if (FocusedTarget.GetObject() == CandidateObject)
	{
		SetFocusedTarget(TScriptInterface<IWorldInteractable>());
	}
}

void UWorldInteractionDetectorComponent::UpdateFocusedTarget()
{
	if (!OwnerCharacter)
	{
		SetFocusedTarget(TScriptInterface<IWorldInteractable>());
		return;
	}

	if (OwnerCharacter->GetCarriedOre())
	{
		SetFocusedTarget(TScriptInterface<IWorldInteractable>());
		return;
	}

	NearbyTargets.RemoveAll([](const TScriptInterface<IWorldInteractable>& Candidate)
	{
		return !IsValid(Candidate.GetObject());
	});

	TScriptInterface<IWorldInteractable> BestTarget;

	FVector TraceStart = OwnerCharacter->GetActorLocation();
	FVector TraceDirection = OwnerCharacter->GetActorForwardVector();

	if (UCameraComponent* FirstPersonCamera = OwnerCharacter->GetFirstPersonCameraComponent())
	{
		TraceStart = FirstPersonCamera->GetComponentLocation();
		TraceDirection = FirstPersonCamera->GetForwardVector();
	}

	constexpr float TraceDistance = 1000.0f;
	const FVector TraceEnd = TraceStart + TraceDirection * TraceDistance;

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WorldInteractionTrace), false);
	QueryParams.AddIgnoredActor(OwnerCharacter);

	if (UWorld* World = OwnerCharacter->GetWorld())
	{
		World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	}

	AActor* HitActor = Hit.GetActor();

	for (const TScriptInterface<IWorldInteractable>& Candidate : NearbyTargets)
	{
		UObject* CandidateObject = Candidate.GetObject();
		if (!CandidateObject || !Candidate->CanInteract(OwnerCharacter))
		{
			continue;
		}

		if (HitActor == CandidateObject)
		{
			BestTarget = Candidate;
			break;
		}
	}

	SetFocusedTarget(BestTarget);
}

void UWorldInteractionDetectorComponent::SetFocusedTarget(const TScriptInterface<IWorldInteractable>& NewFocusedTarget)
{
	UObject* CurrentObject = FocusedTarget.GetObject();
	UObject* NewObject = NewFocusedTarget.GetObject();

	if (CurrentObject == NewObject)
	{
		return;
	}

	// OnInteractionFocusStarted/Ended stay BlueprintNativeEvent (unchanged from before this pipeline
	// was shared with NPCs), so they're invoked via Execute_ rather than a direct call - see
	// IWorldInteractable's class comment. Both AWorldInteractableBase and ANPC_Base now react by
	// opening/closing the same PlayerHUDWidget panel (via SetWorldInteractionTargetFocused /
	// SetInteractionTarget-ClearInteractionTarget respectively) - the detector itself no longer needs
	// to know which family it's looking at.
	if (CurrentObject)
	{
		IWorldInteractable::Execute_OnInteractionFocusEnded(CurrentObject, OwnerCharacter);
	}

	FocusedTarget = NewFocusedTarget;

	if (NewObject)
	{
		IWorldInteractable::Execute_OnInteractionFocusStarted(NewObject, OwnerCharacter);
	}
}
