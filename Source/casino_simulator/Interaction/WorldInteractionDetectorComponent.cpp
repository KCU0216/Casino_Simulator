#include "Interaction/WorldInteractionDetectorComponent.h"

#include "Interaction/WorldInteractableBase.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
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
	CloseWorldPrompt();
	NearbyTargets.Reset();
	FocusedTarget = nullptr;

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

void UWorldInteractionDetectorComponent::RegisterCandidate(AWorldInteractableBase* Candidate)
{
	if (!Candidate)
	{
		return;
	}

	NearbyTargets.AddUnique(Candidate);
}

void UWorldInteractionDetectorComponent::UnregisterCandidate(AWorldInteractableBase* Candidate)
{
	if (!Candidate)
	{
		return;
	}

	NearbyTargets.Remove(Candidate);

	if (FocusedTarget == Candidate)
	{
		SetFocusedTarget(nullptr);
	}
}

bool UWorldInteractionDetectorComponent::TryInteract()
{
	if (!OwnerCharacter || !FocusedTarget || !FocusedTarget->CanInteract(OwnerCharacter))
	{
		return false;
	}

	if (Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(OwnerCharacter->GetController()))
	{
		PlayerController->RequestWorldInteraction(FocusedTarget);
	}

	return true;
}

void UWorldInteractionDetectorComponent::UpdateFocusedTarget()
{
	if (!OwnerCharacter)
	{
		SetFocusedTarget(nullptr);
		return;
	}

	NearbyTargets.RemoveAll([](const AWorldInteractableBase* Candidate)
	{
		return !IsValid(Candidate);
	});

	AWorldInteractableBase* BestTarget = nullptr;

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

	for (AWorldInteractableBase* Candidate : NearbyTargets)
	{
		if (!Candidate || !Candidate->CanInteract(OwnerCharacter))
		{
			continue;
		}

		if (HitActor == Candidate)
		{
			BestTarget = Candidate;
			break;
		}
	}

	SetFocusedTarget(BestTarget);
}

void UWorldInteractionDetectorComponent::SetFocusedTarget(AWorldInteractableBase* NewFocusedTarget)
{
	if (FocusedTarget == NewFocusedTarget)
	{
		if (FocusedTarget && !bWorldPromptOpen)
		{
			if (Acasino_simulatorPlayerController* PlayerController = OwnerCharacter
				? Cast<Acasino_simulatorPlayerController>(OwnerCharacter->GetController())
				: nullptr)
			{
				bWorldPromptOpen = PlayerController->OpenWorldInteraction(FocusedTarget->GetInteractionPromptText());
			}
		}
		return;
	}

	FocusedTarget = NewFocusedTarget;

	Acasino_simulatorPlayerController* PlayerController = OwnerCharacter
		? Cast<Acasino_simulatorPlayerController>(OwnerCharacter->GetController())
		: nullptr;
	if (!PlayerController)
	{
		return;
	}

	if (FocusedTarget)
	{
		bWorldPromptOpen = PlayerController->OpenWorldInteraction(FocusedTarget->GetInteractionPromptText());
	}
	else
	{
		CloseWorldPrompt();
	}
}

void UWorldInteractionDetectorComponent::CloseWorldPrompt()
{
	if (!bWorldPromptOpen || !OwnerCharacter)
	{
		return;
	}

	if (Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(OwnerCharacter->GetController()))
	{
		PlayerController->CloseWorldInteraction();
	}

	bWorldPromptOpen = false;
}
