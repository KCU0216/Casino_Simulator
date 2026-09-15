#include "Interaction/WorldInteractableBase.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Interaction/WorldInteractionDetectorComponent.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"

AWorldInteractableBase::AWorldInteractableBase()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->InitSphereRadius(500.0f);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	InteractionPromptText = FText::FromString(TEXT("E Use"));
}

void AWorldInteractableBase::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AWorldInteractableBase::OnInteractionSphereBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AWorldInteractableBase::OnInteractionSphereEndOverlap);
	}
}

void AWorldInteractableBase::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
}

void AWorldInteractableBase::BeginLocalInteraction(Acasino_simulatorCharacter* InteractingCharacter)
{
}

void AWorldInteractableBase::OnLocalInteract_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
}

void AWorldInteractableBase::OnInteractionFocusStarted_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	// Same PlayerHUDWidget prompt flow ANPC_Base uses.
	if (Acasino_simulatorPlayerController* PlayerController = InteractingCharacter
		? Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController())
		: nullptr)
	{
		PlayerController->SetWorldInteractionTargetFocused(true);
	}
}

void AWorldInteractableBase::OnInteractionFocusEnded_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (Acasino_simulatorPlayerController* PlayerController = InteractingCharacter
		? Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController())
		: nullptr)
	{
		PlayerController->SetWorldInteractionTargetFocused(false);
	}
}

bool AWorldInteractableBase::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	if (!InteractingCharacter)
	{
		return false;
	}

	const float MaxDistance = InteractionSphere ? InteractionSphere->GetScaledSphereRadius() + 150.0f : 0.0f;
	if (MaxDistance <= 0.0f)
	{
		return false;
	}

	const FVector ToCharacter = InteractingCharacter->GetActorLocation() - GetActorLocation();
	return ToCharacter.SizeSquared() <= FMath::Square(MaxDistance);
}

void AWorldInteractableBase::OnInteractionSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor))
	{
		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->RegisterCandidate(this);
		}
	}
}

void AWorldInteractableBase::OnInteractionSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor))
	{
		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->UnregisterCandidate(this);
		}
	}
}
