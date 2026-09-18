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
	InteractingPlayer = InteractingCharacter;
	InteractingCharacter->SetCurrentSeatedMachine(this);

	if (HasAuthority())
	{
		Server_RequestUseMachine_Implementation(InteractingCharacter);
		return;
	}

	Server_RequestUseMachine(InteractingCharacter);
}

void AWorldInteractableBase::BeginLocalInteraction(Acasino_simulatorCharacter* InteractingCharacter)
{
}

void AWorldInteractableBase::OnLocalInteract_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
}

void AWorldInteractableBase::OnInteractionFocusStarted_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (InteractingCharacter == nullptr)
	{
		return;
	}
	// 위젯이 떠있으면 막기
	Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController());
	if (PC == nullptr || PC->IsInteractionUIOpen())
	{
		return;
	}

	if (PC != nullptr && CanInteract(InteractingCharacter))
	{
		PC->SetWorldInteractionTargetFocused(true);
	}
}

void AWorldInteractableBase::OnInteractionFocusEnded_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (InteractingCharacter == nullptr)
	{
		return;
	}

	Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController());
	if (PlayerController != nullptr)
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
	return ToCharacter.SizeSquared() <= FMath::Square(MaxDistance) && InteractingPlayer == nullptr;
}

void AWorldInteractableBase::OnInteractionSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor);

	if (PlayerCharacter && !Players.Contains(PlayerCharacter))
	{
		Players.Add(PlayerCharacter);
	}

	if (InteractingPlayer == nullptr && PlayerCharacter != nullptr)
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
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor);

	if (PlayerCharacter)
	{
		Players.Remove(PlayerCharacter);
	}

	if (InteractingPlayer != nullptr && InteractingPlayer == PlayerCharacter)
	{
		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->UnregisterCandidate(this);
		}
	}
}

void AWorldInteractableBase::HandleMachineRequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter)
{
}

void AWorldInteractableBase::HandleMachineUseStarted(Acasino_simulatorCharacter* Character)
{
}

void AWorldInteractableBase::HandleMachineUseReleased(Acasino_simulatorCharacter* Character)
{
}

void AWorldInteractableBase::Multicast_MachineUseStarted_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	InteractingPlayer = RequestingCharacter;
	RequestingCharacter->SetCurrentSeatedMachine(this);

	HandleMachineUseStarted(RequestingCharacter);
}

void AWorldInteractableBase::Multicast_MachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter)
{
	InteractingPlayer = nullptr;
	ReleasingCharacter->SetCurrentSeatedMachine(nullptr);

	HandleMachineUseReleased(ReleasingCharacter);
}

void AWorldInteractableBase::Server_RequestUseMachine_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	HandleMachineRequestUseMachine(RequestingCharacter);
	Multicast_MachineUseStarted(RequestingCharacter);
}
