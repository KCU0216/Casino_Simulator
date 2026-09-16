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

	FVector CarrierToTarget = *TargetLocation - Carrier->GetActorLocation();
	CarrierToTarget.Z = 0.0f;
	if (CarrierToTarget.SizeSquared() > FMath::Square(MaxCarryTargetDistance))
	{
		return;
	}

	FVector ToTarget = *TargetLocation - GetActorLocation();
	ToTarget.Z = 0.0f;

	FVector CurrentVelocity = CartMesh->GetPhysicsLinearVelocity();
	CurrentVelocity.Z = 0.0f;

	const FVector SpringForce = ToTarget * CarrySpringStrength;
	const FVector DampingForce = -CurrentVelocity * CarryDampingStrength;
	const FVector CarryForce = (SpringForce + DampingForce).GetClampedToMaxSize(MaxCarryForce);

	CartMesh->AddForce(CarryForce);

	FVector CartForward = -CartMesh->GetForwardVector();
	CartForward.Z = 0.0f;

	FVector DesiredForward = Carrier->GetActorForwardVector();
	DesiredForward.Z = 0.0f;

	if (!CartForward.IsNearlyZero() && !DesiredForward.IsNearlyZero())
	{
		CartForward.Normalize();
		DesiredForward.Normalize();

		const float TurnAngle = FMath::Atan2(
			FVector::CrossProduct(CartForward, DesiredForward).Z,
			FVector::DotProduct(CartForward, DesiredForward)
		);
		const float AngularVelocityZ = CartMesh->GetPhysicsAngularVelocityInRadians().Z;
		const float TurnTorque = FMath::Clamp(
			TurnAngle * TurnTorqueStrength - AngularVelocityZ * TurnDampingStrength,
			-MaxTurnTorque,
			MaxTurnTorque
		);

		CartMesh->AddTorqueInRadians(FVector(0.0f, 0.0f, TurnTorque), NAME_None, true);
	}

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
	FVector CharacterToTarget = TargetLocation - Character->GetActorLocation();
	CharacterToTarget.Z = 0.0f;
	if (CharacterToTarget.SizeSquared() > FMath::Square(MaxCarryTargetDistance))
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

	Carrier = Character;
	CarrierTargetLocation = GetActorLocation();
	Character->SetCarriedCart(this);
	ForceNetUpdate();
	return true;
}

bool ACartBase::TryRelease(Acasino_simulatorCharacter* Character)
{
	if (!HasAuthority()
		|| !Character
		|| Character->GetCarriedCart() != this
		)
	{
		return false;
	}
	Carrier = nullptr;
	Character->SetCarriedCart(nullptr);
	return true;
}

void ACartBase::OnRep_Carrier()
{

}
