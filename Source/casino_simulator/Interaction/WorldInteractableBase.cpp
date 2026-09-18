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
	// 위젯이 떠있으면 막기
	Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController());
	if (PC == nullptr)
	{
		return;
	}
	if (InteractingCharacter == nullptr || PC->IsInteractionUIOpen())
	{
		return;
	}
	// NPCs use the same PlayerHUDWidget prompt flow as world interactables.
	Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController());

	if (PlayerController != nullptr && CanInteract(InteractingCharacter))
	{
		PlayerController->SetWorldInteractionTargetFocused(true);
		InteractingCharacter->SetCurrentSeatedMachine(this);
	}
}

void AWorldInteractableBase::OnInteractionFocusEnded_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (InteractingCharacter == nullptr)
	{
		return;
	}

	Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController());
	if (PlayerController != nullptr && InteractingPlayer != nullptr && InteractingPlayer == InteractingCharacter)
	{

		PlayerController->SetWorldInteractionTargetFocused(false);
		InteractingCharacter->SetCurrentSeatedMachine(nullptr);
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
	return  InteractingPlayer == InteractingCharacter && ToCharacter.SizeSquared() <= FMath::Square(MaxDistance);
}

void AWorldInteractableBase::OnInteractionSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("Enter"));
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor);

	if (InteractingPlayer == nullptr && PlayerCharacter != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player"));
		InteractingPlayer = PlayerCharacter;

		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			UE_LOG(LogTemp, Warning, TEXT("Detector"));
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
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor);
	if (InteractingPlayer != nullptr && InteractingPlayer == PlayerCharacter)
	{
		InteractingPlayer = nullptr;
		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->UnregisterCandidate(this);
		}
	}
}
