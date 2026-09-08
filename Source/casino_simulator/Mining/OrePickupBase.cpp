// Fill out your copyright notice in the Description page of Project Settings.


#include "Mining/OrePickupBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Ore_Pickup, "Event.Ore.Pickup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Equipment_Pickaxe_Equipped, "State.Equipment.Pickaxe.Equipped");

namespace
{
	constexpr float WeightForFullPenalty = 100.0f;
	constexpr float MinimumCarryMovementMultiplier = 0.1f;
	constexpr float OreThrowImpulse = 900.0f;
	constexpr float ThrowUpwardBias = 0.15f;
}

// Sets default values
AOrePickupBase::AOrePickupBase()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = false;

	OrePickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrePickupMesh"));
	SetRootComponent(OrePickupMesh);
	InteractionSphere->SetupAttachment(OrePickupMesh);
	OrePickupMesh->SetCollisionProfileName(TEXT("BlockAll"));
	OrePickupMesh->SetCollisionObjectType(ECC_PhysicsBody);
	OrePickupMesh->SetGenerateOverlapEvents(true);
	OrePickupMesh->SetSimulatePhysics(false);
	OrePickupMesh->SetEnableGravity(false);
}

bool AOrePickupBase::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	return Super::CanInteract(InteractingCharacter)
		&& Carrier == nullptr;
}

float AOrePickupBase::GetCarryMovementMultiplier() const
{
	const float WeightRatio = FMath::Clamp(Weight / WeightForFullPenalty, 0.0f, 1.0f);
	return FMath::Clamp(1.0f - WeightRatio, MinimumCarryMovementMultiplier, 1.0f);
}

void AOrePickupBase::BeginLocalInteraction(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (!InteractingCharacter)
	{
		return;
	}

	if (const UAbilitySystemComponent* AbilitySystem = InteractingCharacter->GetAbilitySystemComponent();
		AbilitySystem && AbilitySystem->HasMatchingGameplayTag(TAG_State_Equipment_Pickaxe_Equipped))
	{
		ReceivePickupBlocked(InteractingCharacter);
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = TAG_Event_Ore_Pickup;
	Payload.Instigator = InteractingCharacter;
	Payload.Target = this;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		InteractingCharacter,
		TAG_Event_Ore_Pickup,
		Payload
	);
}

bool AOrePickupBase::TryPickUp(Acasino_simulatorCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character) || Character->GetCarriedOre())
	{
		return false;
	}

	Carrier = Character;
	LastCarrier = Character;
	bUsePhysics = false;
	Character->SetCarriedOre(this);
	SetReplicateMovement(false);
	UpdatePickupCollision();
	UpdatePickupPhysics();
	AttachToCarrier(Character);
	ForceNetUpdate();
	return true;
}

bool AOrePickupBase::TryDrop(Acasino_simulatorCharacter* Character, FVector DropLocation)
{
	constexpr float MaxDropDistance = 300.0f;

	if (!HasAuthority()
		|| !Character
		|| Carrier != Character
		|| Character->GetCarriedOre() != this
		|| FVector::DistSquared(Character->GetActorLocation(), DropLocation) > FMath::Square(MaxDropDistance))
	{
		return false;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetReplicateMovement(true);
	SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);

	Carrier = nullptr;
	LastCarrier = Character;
	bUsePhysics = true;
	Character->SetCarriedOre(nullptr);
	UpdatePickupCollision();
	UpdatePickupPhysics();
	ForceNetUpdate();
	return true;
}

bool AOrePickupBase::TryThrow(Acasino_simulatorCharacter* Character, FVector ThrowDirection)
{
	if (!HasAuthority()
		|| !Character
		|| Carrier != Character
		|| Character->GetCarriedOre() != this)
	{
		return false;
	}

	FVector LaunchDirection = ThrowDirection.GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = Character->GetActorForwardVector();
	}

	LaunchDirection = (LaunchDirection + FVector::UpVector * ThrowUpwardBias).GetSafeNormal();

	FVector ViewLocation;
	FRotator ViewRotation;
	Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	const FVector ReleaseLocation = ViewLocation + LaunchDirection * 100.0f;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetReplicateMovement(true);
	SetActorLocation(ReleaseLocation, false, nullptr, ETeleportType::TeleportPhysics);

	Carrier = nullptr;
	LastCarrier = Character;
	bUsePhysics = true;
	Character->SetCarriedOre(nullptr);
	UpdatePickupCollision();
	UpdatePickupPhysics();

	OrePickupMesh->WakeRigidBody();
	OrePickupMesh->AddImpulse(LaunchDirection * OreThrowImpulse, NAME_None, false);

	ForceNetUpdate();
	return true;
}

// Called when the game starts or when spawned
void AOrePickupBase::BeginPlay()
{
	Super::BeginPlay();


	
}

void AOrePickupBase::UpdatePickupCollision()
{
	OrePickupMesh->SetCollisionEnabled(
		Carrier != nullptr
		? ECollisionEnabled::NoCollision
		: ECollisionEnabled::QueryAndPhysics
	);
}

void AOrePickupBase::UpdatePickupPhysics()
{
	const bool bShouldSimulatePhysics = Carrier == nullptr && bUsePhysics;
	OrePickupMesh->SetMassOverrideInKg(NAME_None, FMath::Max(Weight, 1.0f), true);
	OrePickupMesh->SetEnableGravity(bShouldSimulatePhysics);
	OrePickupMesh->SetSimulatePhysics(bShouldSimulatePhysics);
}

void AOrePickupBase::AttachToCarrier(Acasino_simulatorCharacter* Character)
{
	if (Character && Character->GetFirstPersonMesh())
	{
		const FAttachmentTransformRules AttachmentRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepRelative,
			false
		);

		AttachToComponent(
			Character->GetMesh(),
			AttachmentRules,
			TEXT("hand_r")
		);
	}
}

void AOrePickupBase::OnRep_Carrier()
{
	UpdatePickupCollision();
	UpdatePickupPhysics();

	if (Carrier)
	{
		SetReplicateMovement(false);
		AttachToCarrier(Carrier);
	}
	else
	{
		SetReplicateMovement(true);
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AOrePickupBase::OnRep_UsePhysics()
{
	UpdatePickupPhysics();
}

void AOrePickupBase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AOrePickupBase, Carrier);
	DOREPLIFETIME(AOrePickupBase, bUsePhysics);
}

