#include "Mining/CartBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Mining/MiningGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
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

	CargoVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoVolume"));
	CargoVolume->SetupAttachment(CartMesh);
	CargoVolume->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	CargoVolume->SetBoxExtent(FVector(100.0f, 60.0f, 60.0f));
	CargoVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CargoVolume->SetCollisionObjectType(ECC_WorldDynamic);
	CargoVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	CargoVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	CargoVolume->SetGenerateOverlapEvents(true);
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

	ApplyTiltStabilization();
	if (!IsValid(Carrier))
	{
		CarryBlockedTime = 0.f;
		return;
	}

	UpdateCargoLastCarrier();

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
	ApplyStepAssist(DeltaTime, ToTarget);

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

	if (const UAbilitySystemComponent* AbilitySystem = InteractingCharacter->GetAbilitySystemComponent();
		AbilitySystem && AbilitySystem->HasMatchingGameplayTag(MiningGameplayTags::PickaxeEquipped))
	{
		ReceivePickupBlocked(InteractingCharacter);
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
	if (!HasAuthority() || !IsValid(Character) || !CanInteract(Character)
		|| Character->GetCarriedCart())
	{
		return false;
	}

	if (const UAbilitySystemComponent* AbilitySystem = Character->GetAbilitySystemComponent();
		AbilitySystem && AbilitySystem->HasMatchingGameplayTag(MiningGameplayTags::PickaxeEquipped))
	{
		return false;
	}

	if (Carrier)
	{
		return false;
	}

	Carrier = Character;
	UpdateCargoLastCarrier();
	CarrierTargetLocation = GetActorLocation();
	CarryBlockedTime = 0.f;
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
	UpdateCargoLastCarrier();
	Carrier = nullptr;
	CarryBlockedTime = 0.f;
	Character->SetCarriedCart(nullptr);
	return true;
}

void ACartBase::OnRep_Carrier()
{

}

void ACartBase::UpdateCargoLastCarrier()
{
	if (!HasAuthority() || !IsValid(Carrier) || !CargoVolume)
	{
		return;
	}

	TArray<UPrimitiveComponent*> OverlappingComponents;
	CargoVolume->GetOverlappingComponents(OverlappingComponents);
	for (UPrimitiveComponent* Component : OverlappingComponents)
	{
		AOrePickupBase* Ore = IsValid(Component) ? Cast<AOrePickupBase>(Component->GetOwner()) : nullptr;
		if (IsValid(Ore) && Component == Ore->GetOrePickupMesh() && !Ore->IsBeingCarried())
		{
			Ore->SetLastCarrier(Carrier);
		}
	}
}

float ACartBase::CalculateStepAssistAcceleration(float GroundZ, float BottomZ) const
{
	const float HeightRemaining = FMath::Clamp(GroundZ + MaxStepAssistHeight - BottomZ, 0.f, MaxStepAssistHeight);
	return FMath::Clamp(HeightRemaining * StepAssistStrength, 0.f, MaxStepAssistAcceleration);
}

void ACartBase::ApplyStepAssist(float DeltaTime, const FVector& ToTarget)
{
	if (!CartMesh->IsSimulatingPhysics()) return;
	const FVector PullDirection = ToTarget.GetSafeNormal2D();
	const float ForwardSpeed = FVector::DotProduct(CartMesh->GetPhysicsLinearVelocity(), PullDirection);
	// Do not lift a parked cart or a cart already following its carrier normally.
	if (ToTarget.SizeSquared2D() < FMath::Square(40.f) || ForwardSpeed >= StepAssistBlockedSpeed)
	{
		CarryBlockedTime = 0.f;
		return;
	}
	CarryBlockedTime += DeltaTime;
	if (CarryBlockedTime < StepAssistBlockedDelay || MaxStepAssistHeight <= 0.f) return;

	const FBoxSphereBounds Bounds = CartMesh->Bounds;
	const float BottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
	const FVector TraceStart = Bounds.Origin;
	const FVector TraceEnd(TraceStart.X, TraceStart.Y, BottomZ - MaxStepAssistHeight - 50.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CartStepAssist), false, this);
	Params.AddIgnoredActor(Carrier.Get());
	// Ores and players are not ground; ignore physics bodies and pawns.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult GroundHit;
	if (!GetWorld()->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, Objects, Params)
		|| GroundHit.bStartPenetrating || GroundHit.ImpactNormal.Z < 0.5f) return;

	const float LiftAcceleration = CalculateStepAssistAcceleration(GroundHit.ImpactPoint.Z, BottomZ);
	// Upward assistance only. Gravity handles descent; no added Z velocity damping.
	CartMesh->AddForce(FVector(0.f, 0.f, LiftAcceleration), NAME_None, true);
}

FVector ACartBase::CalculateTiltTorque(const FVector& AngularVelocity) const
{
	const FVector Up = CartMesh->GetUpVector();
	const float TiltAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, FVector::UpVector), -1.f, 1.f));
	const float ExcessTilt = FMath::Max(0.f, TiltAngle - FMath::DegreesToRadians(MaxTiltAngle));
	FVector CorrectionAxis = FVector::CrossProduct(Up, FVector::UpVector).GetSafeNormal();
	if (ExcessTilt > 0.f && CorrectionAxis.IsNearlyZero())
	{
		// An upside-down cart still needs a recovery axis.
		CorrectionAxis = CartMesh->GetForwardVector().GetSafeNormal2D();
	}
	const FVector TiltVelocity(AngularVelocity.X, AngularVelocity.Y, 0.f);
	return (CorrectionAxis * ExcessTilt * TiltSpringStrength - TiltVelocity * TiltDampingStrength)
		.GetClampedToMaxSize(MaxTiltTorque);
}

void ACartBase::ApplyTiltStabilization()
{
	if (!CartMesh->IsSimulatingPhysics()) return;
	// World-horizontal torque controls tilt while preserving free yaw and vertical travel.
	CartMesh->AddTorqueInRadians(CalculateTiltTorque(CartMesh->GetPhysicsAngularVelocityInRadians()), NAME_None, true);
}
