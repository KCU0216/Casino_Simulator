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
	OrePickupMesh->SetGenerateOverlapEvents(false);
	OrePickupMesh->SetSimulatePhysics(false);
	OrePickupMesh->SetEnableGravity(false);
}

bool AOrePickupBase::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	return Super::CanInteract(InteractingCharacter)
		&& Carrier == nullptr;
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
	bUsePhysics = false;
	Character->SetCarriedOre(this);
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
	SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);

	Carrier = nullptr;
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

	FVector ReleaseLocation = GetActorLocation();
	if (const USkeletalMeshComponent* FirstPersonMesh = Character->GetFirstPersonMesh())
	{
		ReleaseLocation = FirstPersonMesh->GetSocketLocation(TEXT("hand_r"));
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocation(ReleaseLocation, false, nullptr, ETeleportType::TeleportPhysics);

	Carrier = nullptr;
	bUsePhysics = true;
	Character->SetCarriedOre(nullptr);
	UpdatePickupCollision();
	UpdatePickupPhysics();

	OrePickupMesh->WakeRigidBody();
	OrePickupMesh->AddImpulse(LaunchDirection * ThrowImpulse, NAME_None, true);

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
			Character->GetFirstPersonMesh(),
			AttachmentRules,
			TEXT("hand_r")
		);
	}
}

void AOrePickupBase::OnRep_Carrier()
{
	UpdatePickupCollision();
	UpdatePickupPhysics();
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

