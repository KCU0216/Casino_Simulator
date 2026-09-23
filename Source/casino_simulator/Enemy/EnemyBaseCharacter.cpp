// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBaseCharacter.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Actor.h"
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


void AEnemyBaseCharacter::ApplyChaseSpeed()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = ChaseSpeed;
	}
}

void AEnemyBaseCharacter::ApplyPatrolSpeed()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = MoveSpeed;
	}
}

void AEnemyBaseCharacter::ApplyRandomPatrolSpeed()
{
	const float MinSpeed = FMath::Min(PatrolMinSpeed, MoveSpeed);
	const float MaxSpeed = FMath::Max(PatrolMinSpeed, MoveSpeed);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed =
			FMath::RandBool() ? MinSpeed : MaxSpeed;
	}
}

UAnimMontage* AEnemyBaseCharacter::SelectHitMontage(AActor* Attacker) const
{
	if (!IsValid(Attacker))
	{
		return nullptr;
	}

	FVector ToAttacker = Attacker -> GetActorLocation() - GetActorLocation();
	ToAttacker.Z = 0.0f;

	if (!ToAttacker.Normalize())
	{
		return nullptr;
	}
	const float ForwardDot =
		FVector::DotProduct(GetActorForwardVector(), ToAttacker);

	const float RightDot =
		FVector::DotProduct(GetActorRightVector(), ToAttacker);

	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		return ForwardDot >= 0.0f
			? HitFrontMontage.Get()
			: HitBackMontage.Get();
	}

	return RightDot >= 0.0f
		? HitRightMontage.Get()
		: HitLeftMontage.Get();
}


void AEnemyBaseCharacter::HandleEnemyHitReaction(AActor* Attacker)
{
	if (!HasAuthority())
	{
		return;
	}

	UAnimMontage* HitMontage = SelectHitMontage(Attacker);

	if (!IsValid(HitMontage))
	{
		return;
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}

	MulticastPlayHitReaction(HitMontage);
}

void AEnemyBaseCharacter::MulticastPlayHitReaction_Implementation(UAnimMontage* HitMontage)
{
	if (!IsValid(HitMontage))
	{
		return;
	}

	PlayAnimMontage(HitMontage);
}