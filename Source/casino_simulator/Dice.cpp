// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dice.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulator.h"

ADice::ADice()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicateMovement(true);

	DiceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiceMesh"));
	SetRootComponent(DiceMesh);
	DiceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DiceMesh->SetCollisionObjectType(ECC_PhysicsBody);
	DiceMesh->SetSimulatePhysics(false);
}

void ADice::Roll(int32 DesiredFace)
{
	if (!HasAuthority())
	{
		return;
	}

	if (DesiredFace < 1 || DesiredFace > 6)
	{
		DesiredFace = FMath::RandRange(1, 6);
	}

	PendingFace = DesiredFace;
	bSettling = false;
	ResultFace = 0;

	DiceMesh->SetSimulatePhysics(true);
	DiceMesh->WakeAllRigidBodies();

	// Spin in place - no throw/hop, just an angular impulse so it looks like it's naturally tumbling
	// to a stop where it's sitting.
	const FVector AngularImpulse = FMath::VRand() * FMath::FRandRange(AngularImpulseRange.X, AngularImpulseRange.Y);
	DiceMesh->AddAngularImpulseInDegrees(AngularImpulse, NAME_None, /*bVelChange=*/true);

	SetActorTickEnabled(true);
}

void ADice::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Only the server drives the roll/settle; clients just receive the resulting transform via
	// normal actor movement replication and the ResultFace OnRep.
	if (!HasAuthority() || PendingFace == 0)
	{
		return;
	}

	if (!bSettling)
	{
		const FVector LinearVelocity = DiceMesh->GetPhysicsLinearVelocity();
		const FVector AngularVelocity = DiceMesh->GetPhysicsAngularVelocityInDegrees();

		if (LinearVelocity.SizeSquared() < FMath::Square(SettleLinearSpeedThreshold)
			&& AngularVelocity.SizeSquared() < FMath::Square(SettleAngularSpeedThreshold))
		{
			// Stop simulating and take over the last stretch by hand so the die is guaranteed to
			// land on PendingFace instead of whatever it physically happened to roll to.
			DiceMesh->SetSimulatePhysics(false);

			SettleStartRotation = GetActorQuat();

			const FRotator RandomSpin(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
			SettleTargetRotation = RandomSpin.Quaternion() * GetFaceUpRotation(PendingFace).Quaternion();

			SettleElapsed = 0.0f;
			bSettling = true;
		}
	}
	else
	{
		SettleElapsed += DeltaSeconds;
		const float Alpha = SettleDuration > 0.0f ? FMath::Clamp(SettleElapsed / SettleDuration, 0.0f, 1.0f) : 1.0f;

		SetActorRotation(FQuat::Slerp(SettleStartRotation, SettleTargetRotation, Alpha));

		if (Alpha >= 1.0f)
		{
			const int32 FinishedFace = PendingFace;

			bSettling = false;
			PendingFace = 0;
			SetActorTickEnabled(false);

			// Setting this (rather than relying solely on the delegate) is what clients actually
			// see replicate in via OnRep_ResultFace.
			ResultFace = FinishedFace;
			OnDiceRolled.Broadcast(ResultFace);
		}
	}
}

void ADice::OnRep_ResultFace()
{
	OnDiceRolled.Broadcast(ResultFace);
}

FRotator ADice::GetFaceUpRotation(int32 Face) const
{
	if (const FRotator* FoundRotation = FaceUpRotations.Find(Face))
	{
		return *FoundRotation;
	}

	UE_LOG(Logcasino_simulator, Warning, TEXT("'%s' has no FaceUpRotations entry for face %d - falling back to identity rotation. Calibrate this die's FaceUpRotations map."), *GetNameSafe(this), Face);
	return FRotator::ZeroRotator;
}

void ADice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADice, ResultFace);
}
