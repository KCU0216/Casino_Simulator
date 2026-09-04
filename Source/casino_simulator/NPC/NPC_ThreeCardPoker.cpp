// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/NPC_ThreeCardPoker.h"

#include "ThreeCardPoker/ThreeCardPokerTableActor.h"
#include "casino_simulatorCharacter.h"
#include "Net/UnrealNetwork.h"

void ANPC_ThreeCardPoker::BeginPlay()
{
	Super::BeginPlay();

	// Only the server spawns the table; it replicates down to clients via ThreeCardPokerTableInstance
	// (marked Replicated) once AThreeCardPokerTableActor itself is a replicated actor.
	if (!HasAuthority() || !ThreeCardPokerTableClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FTransform SpawnTransform = FTransform(GetActorRotation() + ThreeCardPokerTableRotationOffset, GetActorLocation() + ThreeCardPokerTableOffset);
	ThreeCardPokerTableInstance = GetWorld()->SpawnActor<AThreeCardPokerTableActor>(ThreeCardPokerTableClass, SpawnTransform, SpawnParams);
}

void ANPC_ThreeCardPoker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPC_ThreeCardPoker, ThreeCardPokerTableInstance);
}

void ANPC_ThreeCardPoker::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (ThreeCardPokerTableInstance)
	{
		ThreeCardPokerTableInstance->SetInteractingPlayer(InteractingCharacter);
	}

	Super::Interact(InteractingCharacter);
}

void ANPC_ThreeCardPoker::EndInteraction()
{
	if (ThreeCardPokerTableInstance)
	{
		ThreeCardPokerTableInstance->SetInteractingPlayer(nullptr);
	}
}
