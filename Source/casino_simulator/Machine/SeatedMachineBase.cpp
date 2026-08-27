#include "Machine/SeatedMachineBase.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"

ASeatedMachineBase::ASeatedMachineBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // �� ���Ͱ� ��Ʈ��ũ ���� ����̶�� ��

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

void ASeatedMachineBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeatedMachineBase, CurrentUser);
	DOREPLIFETIME(ASeatedMachineBase, bCanOperate);
	DOREPLIFETIME(ASeatedMachineBase, bCanExitMachine);
}

void ASeatedMachineBase::Interact(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (HasAuthority())
	{
		Server_RequestUseMachine_Implementation(RequestingCharacter);
		return;
	}

	Server_RequestUseMachine(RequestingCharacter);
}

void ASeatedMachineBase::RequestReleaseMachine(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (HasAuthority())
	{
		Server_ReleaseMachine_Implementation(RequestingCharacter);
		return;
	}

	Server_ReleaseMachine(RequestingCharacter);
}

void ASeatedMachineBase::HandleMachinePrimaryInput(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!RequestingCharacter || !CanOperate(RequestingCharacter))
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

bool ASeatedMachineBase::CanOperate(Acasino_simulatorCharacter* RequestingCharacter) const
{
	return bCanOperate && CurrentUser && CurrentUser == RequestingCharacter;
}

bool ASeatedMachineBase::CanInteract(Acasino_simulatorCharacter* RequestingCharacter) const
{
	return Super::CanInteract(RequestingCharacter)
		&& (!CurrentUser || CurrentUser == RequestingCharacter);
}

void ASeatedMachineBase::Server_RequestUseMachine_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	const ESeatedMachineUseResult Result = CanAcceptUser(RequestingCharacter);
	if (Result != ESeatedMachineUseResult::Accepted)
	{
		OnMachineUseRejected(RequestingCharacter, Result);
		return;
	}

	CurrentUser = RequestingCharacter;
	bCanOperate = true;
	bCanExitMachine = true;

	Multicast_MachineUseStarted(RequestingCharacter);
}

void ASeatedMachineBase::Server_ReleaseMachine_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!RequestingCharacter || CurrentUser != RequestingCharacter)
	{
		return;
	}

	if (!bCanExitMachine)
	{
		OnMachineExitRejected(RequestingCharacter);
		return;
	}

	Acasino_simulatorCharacter* ReleasingCharacter = CurrentUser;
	CurrentUser = nullptr;
	bCanOperate = false;
	bCanExitMachine = true;

	Multicast_MachineReleased(ReleasingCharacter);
}

void ASeatedMachineBase::Server_HandleMachinePrimaryInput_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!RequestingCharacter || !CanOperate(RequestingCharacter))
	{
		return;
	}

	OnMachinePrimaryInput(RequestingCharacter);
}

void ASeatedMachineBase::Server_SetCanExitMachine_Implementation(bool bCanExit)
{
	bCanExitMachine = bCanExit;
}

void ASeatedMachineBase::Multicast_MachineUseStarted_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	EnterMachineUseView(RequestingCharacter);
	OnMachineReady(RequestingCharacter);
}

void ASeatedMachineBase::Multicast_MachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter)
{
	OnMachineReleased(ReleasingCharacter);
	ExitMachineUseView(ReleasingCharacter);
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

ESeatedMachineUseResult ASeatedMachineBase::CanAcceptUser(Acasino_simulatorCharacter* RequestingCharacter) const
{
	if (!RequestingCharacter)
	{
		return ESeatedMachineUseResult::InvalidUser;
	}

	if (CurrentUser && CurrentUser != RequestingCharacter)
	{
		return ESeatedMachineUseResult::AlreadyOccupied;
	}

	return ESeatedMachineUseResult::Accepted;
}

void ASeatedMachineBase::EnterMachineUseView(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (!RequestingCharacter)
	{
		return;
	}

	if (bMoveUserToSeatOnUse && SeatPoint)
	{
		RequestingCharacter->SetActorLocationAndRotation(
			SeatPoint->GetComponentLocation(),
			SeatPoint->GetComponentRotation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
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

	APlayerController* PlayerController = Cast<APlayerController>(ReleasingCharacter->GetController());
	if (PlayerController && PlayerController->IsLocalController())
	{
		PlayerController->SetViewTargetWithBlend(ReleasingCharacter, ReleaseCameraBlendTime);
	}

	if (bDisableUserMovementOnUse)
	{
		if (UCharacterMovementComponent* MovementComponent = ReleasingCharacter->GetCharacterMovement())
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}

	if (MachineCamera)
	{
		MachineCamera->SetActive(false);
	}

	ReleasingCharacter->ClearCurrentSeatedMachine(this);
}
