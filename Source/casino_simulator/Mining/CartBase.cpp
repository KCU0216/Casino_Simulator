#include "Mining/CartBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Mining/OrePickupBase.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Cart_Pickup, "Event.Cart.Pickup");

ACartBase::ACartBase()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;

	CartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CartMesh"));
	SetRootComponent(CartMesh);
	InteractionSphere->SetupAttachment(CartMesh);
	CartMesh->SetCollisionProfileName(TEXT("BlockAll"));
	CartMesh->SetCollisionObjectType(ECC_PhysicsBody);
	CartMesh->SetGenerateOverlapEvents(true);
	CartMesh->SetSimulatePhysics(true);
	CartMesh->SetEnableGravity(true);
}

void ACartBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACartBase, Carrier);
}

void ACartBase::BeginPlay()
{
	Super::BeginPlay();
}

void ACartBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(Carrier))
	{
		return;
	}

	const FVector* TargetLocation = &CarrierTargetLocation;
	if (!TargetLocation || TargetLocation->ContainsNaN())
	{
		return;
	}

	if (FVector::DistSquared(Carrier->GetActorLocation(), *TargetLocation) > FMath::Square(MaxCarryTargetDistance))
	{
		return;
	}

	const FVector NewLocation = FMath::VInterpTo(
		GetActorLocation(),
		CarrierTargetLocation,
		DeltaTime,
		20.f
	);
	//500.f -> temp carry velocity (hard coding)

	SetActorLocation(NewLocation, false);

}

//로컬에서 시작 (예측)
void ACartBase::BeginLocalInteraction(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (!InteractingCharacter)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = TAG_Event_Cart_Pickup;
	Payload.Instigator = InteractingCharacter;
	Payload.Target = this;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		InteractingCharacter,
		TAG_Event_Cart_Pickup,
		Payload
	);
}

bool ACartBase::UpdateCarryTargetLocation(Acasino_simulatorCharacter* Character, FVector TargetLocation)
{
	//서버냐
	if (!HasAuthority() || !IsValid(Character) || Carrier != (Character))
	{
		return false;
	}
	//말 되는 값이냐
	if (TargetLocation.ContainsNaN())
	{
		return false;
	}
	//클라가 똑바로 보냈냐
	if (FVector::DistSquared(Character->GetActorLocation(), TargetLocation) > FMath::Square(MaxCarryTargetDistance))
	{
		return false;
	}
	//광물이랑 너무 머냐
	if (FVector::DistSquared(Character->GetActorLocation(), GetActorLocation()) > FMath::Square(MaxCarryTargetDistance))
	{
		return false;
	}
	CarrierTargetLocation = TargetLocation;
	return true;
}

bool ACartBase::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	return Super::CanInteract(InteractingCharacter);
}


bool ACartBase::TryCarry(Acasino_simulatorCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character) || Character->GetCarriedCart())
	{
		return false;        
	}

	if (Carrier)
	{
		return false;
	}

	CartMesh->SetSimulatePhysics(false);
	CartMesh->SetEnableGravity(false);

	Carrier = Character;
	CarrierTargetLocation = GetActorLocation();
	Character->SetCarriedCart(this);
	ForceNetUpdate();
	return true;
}

bool ACartBase::TryRelease(Acasino_simulatorCharacter* Character)
{
	UE_LOG(LogTemp, Error, TEXT("Cart TryRelease called: HasAuthority=%d Character=%s CharacterCarriedCart=%s This=%s Carrier=%s"),
		HasAuthority(),
		*GetNameSafe(Character),
		*GetNameSafe(Character ? Character->GetCarriedCart() : nullptr),
		*GetNameSafe(this),
		*GetNameSafe(Carrier));

	if (!HasAuthority()
		|| !Character
		|| Character->GetCarriedCart() != this
		)
	{
		UE_LOG(LogTemp, Error, TEXT("Cart TryRelease failed"));
		return false;
	}
	Carrier = nullptr;
	CartMesh->SetSimulatePhysics(true);
	CartMesh->SetEnableGravity(true);
	Character->SetCarriedCart(nullptr);
	UE_LOG(LogTemp, Error, TEXT("Cart TryRelease succeeded"));
	return true;
}

void ACartBase::OnRep_Carrier()
{

}
