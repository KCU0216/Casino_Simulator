#include "Machine/SeatedMachineBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Interaction/InteractionSessionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"

ASeatedMachineBase::ASeatedMachineBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // �� ���Ͱ� ��Ʈ��ũ ���� ����̶�� ��
	InteractionSessionComponent = CreateDefaultSubobject<UInteractionSessionComponent>(TEXT("InteractionSessionComponent"));

	// ���ڿ� StaticMeshComponent�� �����ϴ� ��
	ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh"));
	// ChairMesh�� SceneRoot �ؿ� ���̴� ��
	ChairMesh->SetupAttachment(SceneRoot);
	// ������ �浹 ������ ���ϴ� ��
	ChairMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	SeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint"));
	SeatPoint->SetupAttachment(ChairMesh);

	CameraPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPoint"));
	CameraPoint->SetupAttachment(SceneRoot);

	MachineCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MachineCamera"));
	MachineCamera->SetupAttachment(CameraPoint);
	MachineCamera->SetAutoActivate(false);
}

void ASeatedMachineBase::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSessionComponent)
	{
		InteractionSessionComponent->OnUserJoined.AddUObject(this, &ASeatedMachineBase::HandleSessionUserJoined);
		InteractionSessionComponent->OnUserLeft.AddUObject(this, &ASeatedMachineBase::HandleSessionUserLeft);
	}
}

void ASeatedMachineBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeatedMachineBase, CurrentUser);
	DOREPLIFETIME(ASeatedMachineBase, bCanOperate);
	DOREPLIFETIME(ASeatedMachineBase, bCanExitMachine);
}

void ASeatedMachineBase::Interact(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!IsValid(RequestingCharacter))
	{
		OnMachineUseRejected(
			RequestingCharacter,
			ESeatedMachineUseResult::InvalidUser);
		return;
	}

	if (!IsValid(InteractionSessionComponent) ||
		!InteractionSessionComponent->TryJoin(RequestingCharacter))
	{
		OnMachineUseRejected(
			RequestingCharacter,
			ESeatedMachineUseResult::AlreadyOccupied);
	}
}

bool ASeatedMachineBase::CanInteract(
	Acasino_simulatorCharacter* RequestingCharacter) const
{
	USphereComponent* Sphere = GetInteractionSphere();

	if (!IsValid(RequestingCharacter) ||
		!IsValid(Sphere) ||
		!IsValid(InteractionSessionComponent))
	{
		return false;
	}

	const float MaxDistance =
		Sphere->GetScaledSphereRadius() + 150.0f;

	const float DistanceSquared = FVector::DistSquared(
		RequestingCharacter->GetActorLocation(),
		GetActorLocation());

	return DistanceSquared <= FMath::Square(MaxDistance) &&
		InteractionSessionComponent->HasCapacity();
}

void ASeatedMachineBase::RequestReleaseMachine(
	Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!HasAuthority() ||
		!IsValid(RequestingCharacter) ||
		!IsValid(InteractionSessionComponent) ||
		!InteractionSessionComponent->ContainsUser(RequestingCharacter))
	{
		return;
	}

	if (!bCanExitMachine)
	{
		OnMachineExitRejected(RequestingCharacter);
		return;
	}

	InteractionSessionComponent->TryLeave(RequestingCharacter);
}

void ASeatedMachineBase::HandleMachinePrimaryInput(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!RequestingCharacter)
	{
		return;
	}

	if (HasAuthority())
	{
		Server_HandleMachinePrimaryInput_Implementation(RequestingCharacter);
		return;
	}

	Server_HandleMachinePrimaryInput(RequestingCharacter);
}

void ASeatedMachineBase::SetCanExitMachine(bool bCanExit)
{
	bCanExitMachine = bCanExit;

	if (!HasAuthority())
	{
		Server_SetCanExitMachine(bCanExit);
	}
}

bool ASeatedMachineBase::IsOccupied() const
{
	return IsValid(InteractionSessionComponent) &&
		InteractionSessionComponent->IsSessionActive();
}

Acasino_simulatorCharacter* ASeatedMachineBase::GetCurrentUser() const
{
	if (!IsValid(InteractionSessionComponent))
	{
		return nullptr;
	}

	const TArray<TObjectPtr<Acasino_simulatorCharacter>>& Users =
		InteractionSessionComponent->GetUsers();

	return Users.IsEmpty() ? nullptr : Users[0].Get();
}


void ASeatedMachineBase::Server_HandleMachinePrimaryInput_Implementation(
	Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!IsValid(RequestingCharacter) ||
		!IsValid(InteractionSessionComponent) ||
		!InteractionSessionComponent->ContainsUser(RequestingCharacter))
	{
		return;
	}

	OnMachinePrimaryInput(RequestingCharacter);
}

void ASeatedMachineBase::Server_SetCanExitMachine_Implementation(bool bCanExit)
{
	bCanExitMachine = bCanExit;
}

void ASeatedMachineBase::HandleMachineUseStarted(Acasino_simulatorCharacter* RequestingCharacter)
{
	Super::HandleMachineUseStarted(RequestingCharacter);

	EnterMachineUseView(RequestingCharacter);
	OnMachineReady(RequestingCharacter);
}

void ASeatedMachineBase::HandleMachineUseReleased(Acasino_simulatorCharacter* ReleasingCharacter)
{
	Super::HandleMachineUseReleased(ReleasingCharacter);

	OnMachineReleased(ReleasingCharacter);
	ExitMachineUseView(ReleasingCharacter);
}

void ASeatedMachineBase::HandleSessionUserJoined(Acasino_simulatorCharacter* JoinedUser)
{
	if (!IsValid(JoinedUser))
	{
		return;
	}

	InteractingPlayer = JoinedUser;
	CurrentUser = JoinedUser;
	bCanOperate = true;
	bCanExitMachine = true;

	HandleMachineUseStarted(JoinedUser);
}

void ASeatedMachineBase::HandleSessionUserLeft(Acasino_simulatorCharacter* LeftUser)
{
	if (!IsValid(LeftUser))
	{
		return;
	}

	if (CurrentUser == LeftUser)
	{
		CurrentUser = nullptr;
	}

	if (InteractingPlayer == LeftUser)
	{
		InteractingPlayer = nullptr;
	}

	bCanOperate = InteractionSessionComponent &&
		InteractionSessionComponent->IsSessionActive();

	if (!bCanOperate)
	{
		bCanExitMachine = true;
	}

	LeftUser->SetCurrentSeatedMachine(nullptr);
	HandleMachineUseReleased(LeftUser);
}

void ASeatedMachineBase::OnRep_CurrentUser()
{
	bCanOperate = CurrentUser != nullptr;
}

void ASeatedMachineBase::OnMachineReady_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
}

void ASeatedMachineBase::OnMachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter)
{
}

void ASeatedMachineBase::OnMachineUseRejected_Implementation(
	Acasino_simulatorCharacter* RequestingCharacter,
	ESeatedMachineUseResult Result)
{
}

void ASeatedMachineBase::OnMachinePrimaryInput_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
}

void ASeatedMachineBase::OnMachineExitRejected_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
}

void ASeatedMachineBase::EnterMachineUseView(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!RequestingCharacter)
	{
		return;
	}

	if (bMoveUserToSeatOnUse && SeatPoint)
	{
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Sit")));

		RequestingCharacter->GetAbilitySystemComponent()->TryActivateAbilitiesByTag(TagContainer, true);

		FVector SeatLocation = SeatPoint->GetComponentLocation();
		SeatLocation.Z += SeatHeightOffset;

		// Apply the horizontal seat offset along the machine's local Y axis.
		SeatLocation += GetActorRightVector() * (SeatHeightOffset / 2.0f);

		FRotator SeatRoator = SeatPoint->GetComponentRotation();
		RequestingCharacter->SetActorLocationAndRotation(
			SeatLocation,
			SeatRoator,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		// bUseControllerRotationYaw가 켜져 있으면 다음 틱에 Actor 회전이
		// Controller의 ControlRotation으로 덮어써지므로, 여기서도 같이 맞춰준다.
		if (AController* SeatController = RequestingCharacter->GetController())
		{
			SeatController->SetControlRotation(SeatRoator);
		}
	}

	if (bDisableUserMovementOnUse)
	{
		if (UCharacterMovementComponent* MovementComponent = RequestingCharacter->GetCharacterMovement())
		{
			MovementComponent->DisableMovement();
		}
	}

	if (MachineCamera)
	{
		MachineCamera->SetActive(true);
	}

	APlayerController* PlayerController = Cast<APlayerController>(RequestingCharacter->GetController());
	if (PlayerController && PlayerController->IsLocalController())
	{
		PlayerController->SetViewTargetWithBlend(this, MachineCameraBlendTime);
	}

	RequestingCharacter->SetCurrentSeatedMachine(this);
}

void ASeatedMachineBase::ExitMachineUseView(Acasino_simulatorCharacter* ReleasingCharacter)
{
	if (!ReleasingCharacter)
	{
		return;
	}

 if (ReleasingCharacter->HasAuthority())
 {
  FGameplayEventData EndSitEvent;
  UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(ReleasingCharacter,
   FGameplayTag::RequestGameplayTag(FName("State.Walk")), EndSitEvent);
 }
 // Day-end payment/result flow owns the camera and movement after forced relocation.
 if (!UInteractionSessionComponent::CanRestoreMovement(ReleasingCharacter))
 {
  if (MachineCamera) MachineCamera->SetActive(false);
  ReleasingCharacter->ClearCurrentSeatedMachine(this);
  return;
 }

	APlayerController* PlayerController = Cast<APlayerController>(ReleasingCharacter->GetController());
	if (PlayerController && PlayerController->IsLocalController())
	{
		PlayerController->SetViewTargetWithBlend(ReleasingCharacter, ReleaseCameraBlendTime);
	}

	if (bDisableUserMovementOnUse)
	{
		if (UCharacterMovementComponent* MovementComponent = ReleasingCharacter->GetCharacterMovement())
		{
			UInteractionSessionComponent::RestoreMovementAfterUse(ReleasingCharacter);
		}
	}

	if (MachineCamera)
	{
		MachineCamera->SetActive(false);
	}

	ReleasingCharacter->ClearCurrentSeatedMachine(this);
}
