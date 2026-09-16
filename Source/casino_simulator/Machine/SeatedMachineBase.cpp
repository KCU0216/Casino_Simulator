#include "Machine/SeatedMachineBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"
#include "AbilitySystemComponent.h"

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
	if (!RequestingCharacter)
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
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Sit")));
	
		RequestingCharacter->GetAbilitySystemComponent()->TryActivateAbilitiesByTag(TagContainer, true);

		FVector SeatLocation = SeatPoint->GetComponentLocation();
		SeatLocation.Z += SeatHeightOffset;
		SeatLocation.Y += SeatHeightOffset/2;
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

	// 착석 해제 시 "State.Walk" 게임플레이 이벤트를 보낸다. 실행 중인 어빌리티가
	// Wait Gameplay Event 노드로 이 태그를 리슨하고 있으면 그쪽에서 받아 처리한다.
	FGameplayEventData EventData;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		ReleasingCharacter,
		FGameplayTag::RequestGameplayTag(FName("State.Walk")),
		EventData);

	ReleasingCharacter->ClearCurrentSeatedMachine(this);
}
