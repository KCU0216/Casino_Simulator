// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/MachineInteractionComponent.h"
#include "Interaction/CasinoDayParticipant.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "casino_loop_gamestate.h"
#include "Engine/World.h"

UMachineInteractionComponent::UMachineInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMachineInteractionComponent::RequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[InteractDebug] RequestUseMachine Component=%s Owner=%s OwnerAuthority=%d Character=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		GetOwner() && GetOwner()->HasAuthority(),
		*GetNameSafe(RequestingCharacter));

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Server_RequestUseMachine_Implementation(RequestingCharacter);
		return;
	}

	Server_RequestUseMachine(RequestingCharacter);
}

void UMachineInteractionComponent::Server_RequestUseMachine_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	const ACasinoLoopGameState* CasinoGameState = GetWorld()
		? GetWorld()->GetGameState<ACasinoLoopGameState>()
		: nullptr;
	const int32 CasinoPhase = CasinoGameState
		? static_cast<int32>(CasinoGameState->LoopStatus.Phase)
		: INDEX_NONE;
	const bool bGameplayAllowed = IsCasinoGameplayAllowed(this);

	UE_LOG(LogTemp, Warning,
		TEXT("[InteractDebug] Server_RequestUseMachine Owner=%s Phase=%d GameplayAllowed=%d RequestDelegateBound=%d UseStartedDelegateBound=%d"),
		*GetNameSafe(GetOwner()),
		CasinoPhase,
		bGameplayAllowed,
		OnRequestUseMachine.IsBound(),
		OnUseStarted.IsBound());

	if (!bGameplayAllowed)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[InteractDebug] Machine interaction blocked by casino phase. Expected Playing=%d, Actual=%d"),
			static_cast<int32>(ECasinoLoopPhase::Playing),
			CasinoPhase);
		return;
	}

	OnRequestUseMachine.ExecuteIfBound(RequestingCharacter);
	Multicast_MachineUseStarted(RequestingCharacter);
	UE_LOG(LogTemp, Warning,
		TEXT("[InteractDebug] Machine use request completed for Owner=%s Character=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(RequestingCharacter));

	if (RequestingCharacter)
	{
		if (UCharacterMovementComponent* MovementComponent = RequestingCharacter->GetCharacterMovement())
		{
			MovementComponent->DisableMovement();
		}
	}
}

void UMachineInteractionComponent::Multicast_MachineUseStarted_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	OnUseStarted.ExecuteIfBound(RequestingCharacter);
}


bool UMachineInteractionComponent::CanRestoreMovement(const Acasino_simulatorCharacter* Character)
{
 if (!IsValid(Character) || !Character->GetWorld()) return false;
 const auto* PC = Cast<Acasino_simulatorPlayerController>(Character->GetController());
 if (PC && PC->IsDailyPaymentControlLocked()) return false;
 const auto* GS = Character->GetWorld()->GetGameState<ACasinoLoopGameState>();
 return !GS || GS->LoopStatus.Phase == ECasinoLoopPhase::Waiting || GS->LoopStatus.Phase == ECasinoLoopPhase::Playing;
}

void UMachineInteractionComponent::RestoreMovementAfterUse(Acasino_simulatorCharacter* Character)
{
 if (CanRestoreMovement(Character))
 {
  if (auto* Movement = Character->GetCharacterMovement())
   Movement->SetMovementMode(MOVE_Walking);
 }
}
