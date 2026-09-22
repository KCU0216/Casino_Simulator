// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/MachineInteractionComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "casino_simulatorCharacter.h"

UMachineInteractionComponent::UMachineInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMachineInteractionComponent::RequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Server_RequestUseMachine_Implementation(RequestingCharacter);
		return;
	}

	Server_RequestUseMachine(RequestingCharacter);
}

void UMachineInteractionComponent::Server_RequestUseMachine_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	OnRequestUseMachine.ExecuteIfBound(RequestingCharacter);
	Multicast_MachineUseStarted(RequestingCharacter);

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
