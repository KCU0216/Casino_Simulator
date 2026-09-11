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
}

// Sets default values
AOrePickupBase::AOrePickupBase()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;

	OrePickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrePickupMesh"));
	SetRootComponent(OrePickupMesh);
	InteractionSphere->SetupAttachment(OrePickupMesh);
	OrePickupMesh->SetCollisionProfileName(TEXT("BlockAll"));
	OrePickupMesh->SetCollisionObjectType(ECC_PhysicsBody);
	OrePickupMesh->SetGenerateOverlapEvents(true);
	OrePickupMesh->SetSimulatePhysics(true);
	OrePickupMesh->SetEnableGravity(true);
}
//상호작용 할 수 있는지 검사
bool AOrePickupBase::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	return Super::CanInteract(InteractingCharacter)
		&& Carriers.Num() < MaxCarryCount;
}
//무게에 따른 속도 변화량 가져오기
float AOrePickupBase::GetCarryMovementMultiplier() const
{
	const float WeightRatio = FMath::Clamp(Weight / WeightForFullPenalty, 0.0f, 1.0f);
	return FMath::Clamp(1.0f - WeightRatio, MinimumCarryMovementMultiplier, 1.0f);
}
//로컬에서 시작 (예측)
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
//주워보기
bool AOrePickupBase::TryPickUp(Acasino_simulatorCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character) || Character->GetCarriedOre())
	{
		return false;
	}

	if (!CanJoinCarry(Character))
	{
		return false;
	}

	if (Carriers.Num() == 0)
	{
		OrePickupMesh->SetEnableGravity(false);
	}
	

	Carriers.AddUnique(Character);
	LastCarriers.AddUnique(Character);
	CarrierTargetLocations.FindOrAdd(Character) = GetActorLocation();
	Character->SetCarriedOre(this);
	ForceNetUpdate();
	return true;
}
//놓아보기
bool AOrePickupBase::TryDrop(Acasino_simulatorCharacter* Character)
{
	if (!HasAuthority()
		|| !Character
		|| Character->GetCarriedOre() != this
		)
	{
		return false;
	}

	Carriers.Remove(Character);
	CarrierTargetLocations.Remove(Character);
	if (Carriers.Num() == 0)
	{
		OrePickupMesh->SetEnableGravity(true);
	}
	Character->SetCarriedOre(nullptr);
	ForceNetUpdate();
	return true;
}
//나도 참여 가능하냐
bool AOrePickupBase::CanJoinCarry(Acasino_simulatorCharacter* Character)
{
	if (!Character)
	{
		return false;
	}
	if (Carriers.Num() >= MaxCarryCount)
	{
		return false;
	}
	if (Carriers.Contains(Character))
	{
		return false;
	}
	return true;
}

//움직일 수 있냐
bool AOrePickupBase::CanMoveCarry()
{
	return Carriers.Num() >= MinCarryCount && Carriers.Num() <= MaxCarryCount;
}

bool AOrePickupBase::UpdateCarryTargetLocation(Acasino_simulatorCharacter* Character, FVector TargetLocation)
{
	if (!HasAuthority() || !IsValid(Character) || !Carriers.Contains(Character) || TargetLocation.ContainsNaN())
	{
		return false;
	}

	if (FVector::DistSquared(Character->GetActorLocation(), TargetLocation) > FMath::Square(MaxCarryTargetDistance))
	{
		return false;
	}

	CarrierTargetLocations.FindOrAdd(Character) = TargetLocation;
	return true;
}

void AOrePickupBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CanMoveCarry() || !HasAuthority())
	{
		return;
	}

	FVector TotalSpringForce = FVector::ZeroVector;
	for (const TObjectPtr<Acasino_simulatorCharacter>& Carrier : Carriers)
	{
		if (!IsValid(Carrier))
		{
			continue;
		}

		const FVector* TargetLocation = CarrierTargetLocations.Find(Carrier);
		if (!TargetLocation || TargetLocation->ContainsNaN())
		{
			continue;
		}

		if (FVector::DistSquared(Carrier->GetActorLocation(), *TargetLocation) > FMath::Square(MaxCarryTargetDistance))
		{
			continue;
		}

		TotalSpringForce += (*TargetLocation - GetActorLocation()) * CarrySpringStrength;
	}

	const FVector CurrentVelocity = OrePickupMesh->GetPhysicsLinearVelocity();
	const FVector DampingForce = -CurrentVelocity * CarryDampingStrength;
	const FVector CarryForce = (TotalSpringForce + DampingForce).GetClampedToMaxSize(MaxCarryForce);

	OrePickupMesh->AddForce(CarryForce);
}

// Called when the game starts or when spawned
void AOrePickupBase::BeginPlay()
{
	Super::BeginPlay();

	OrePickupMesh->SetMassOverrideInKg(NAME_None, FMath::Max(Weight, 1.0f), true);
	
}

void AOrePickupBase::OnRep_Carriers()
{
	ReceiveCarrierCountChanged(
		Carriers.Num(),
		MinCarryCount,
		MaxCarryCount
	);
}

void AOrePickupBase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AOrePickupBase, Carriers);
}
