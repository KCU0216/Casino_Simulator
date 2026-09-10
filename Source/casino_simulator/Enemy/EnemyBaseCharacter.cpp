// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBaseCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

AEnemyBaseCharacter::AEnemyBaseCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bOrientRotationToMovement = true;
		MovementComponent->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	}
}

void AEnemyBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = MoveSpeed;
		MovementComponent->MaxStepHeight = EnemyMaxStepHeight;
	}
}

void AEnemyBaseCharacter::SetEnemyMoveSpeed(float NewMoveSpeed)
{
	MoveSpeed = FMath::Max(0.0f, NewMoveSpeed);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = MoveSpeed;
	}
}
