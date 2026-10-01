	#include "Mining/OreBase.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "casino_simulatorCharacter.h"
#include "Mining/OrePickupBase.h"

AOreBase::AOreBase()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;

	OreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OreMesh"));
	SetRootComponent(OreMesh);
	OreMesh->SetCollisionProfileName(TEXT("BlockAll"));
	OreMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> MiningHitEffectAsset(
		TEXT("/Game/KCU/Mine/NS_MiningHit_Custom.NS_MiningHit_Custom"));
	if (MiningHitEffectAsset.Succeeded())
	{
		MiningHitEffect = MiningHitEffectAsset.Object;
	}
}

void AOreBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (bUseOreTypeDefaultDurability)
		{
			MaxDurability = GetDefaultMaxDurabilityForOreType();
		}

		CurrentDurability = MaxDurability;
	}
}

void AOreBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AOreBase, CurrentDurability);
}

bool AOreBase::ApplyMiningHit(const int32 Damage)
{
	return ApplyMiningHitInternal(Damage, nullptr);
}

bool AOreBase::ApplyMiningHitInternal(const int32 Damage, const FHitResult* HitResult)
{
	if (!HasAuthority() || Damage <= 0 || IsDepleted())
	{
		return false;
	}

	CurrentDurability = FMath::Max(0, CurrentDurability - Damage);
	if (HitResult)
	{
		const FVector ImpactNormal = HitResult->ImpactNormal.IsNearlyZero()
			? FVector::UpVector
			: HitResult->ImpactNormal.GetSafeNormal();
		Multicast_PlayMiningHitEffect(HitResult->ImpactPoint, ImpactNormal);
	}
	OnDurabilityChanged.Broadcast(CurrentDurability, MaxDurability);
	OnMiningHitApplied(CurrentDurability);

	if (IsDepleted())
	{
		OnOreDepleted.Broadcast();
		ReceiveOreDepleted();

		//spawn OrePickup

		if (OrePickupClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			AOrePickupBase* Pickup = GetWorld()->SpawnActor<AOrePickupBase>(
				OrePickupClass,
				GetActorTransform(), 
				SpawnParams);
		}
		
		Destroy();
	}

	return true;
}

bool AOreBase::ApplyMiningHitFromCharacter(Acasino_simulatorCharacter* MiningCharacter)
{
	if (!HasAuthority() || !MiningCharacter)
	{
		return false;
	}

	return ApplyMiningHit(MiningCharacter->GetPickaxeMiningPower());
}

bool AOreBase::ApplyMiningHitFromCharacterAtHit(
	Acasino_simulatorCharacter* MiningCharacter,
	const FHitResult& HitResult)
{
	if (!HasAuthority() ||
		!MiningCharacter ||
		!HitResult.bBlockingHit ||
		HitResult.GetActor() != this)
	{
		return false;
	}

	return ApplyMiningHitInternal(
		MiningCharacter->GetPickaxeMiningPower(),
		&HitResult);
}

void AOreBase::Multicast_PlayMiningHitEffect_Implementation(
	FVector_NetQuantize ImpactPoint,
	FVector_NetQuantizeNormal ImpactNormal)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FVector SafeNormal = FVector(ImpactNormal).IsNearlyZero()
		? FVector::UpVector
		: FVector(ImpactNormal).GetSafeNormal();

	if (MiningHitEffect)
	{
		FVector TangentX;
		FVector TangentY;
		SafeNormal.FindBestAxisVectors(TangentX, TangentY);

		constexpr float SpreadAmount = 0.55f;
		const FVector SpawnDirections[] =
		{
			SafeNormal,
			(SafeNormal + TangentX * SpreadAmount).GetSafeNormal(),
			(SafeNormal - TangentX * SpreadAmount).GetSafeNormal(),
			(SafeNormal + TangentY * SpreadAmount).GetSafeNormal(),
			(SafeNormal - TangentY * SpreadAmount).GetSafeNormal()
		};

		for (const FVector& SpawnDirection : SpawnDirections)
		{
			UNiagaraComponent* EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this,
				MiningHitEffect,
				FVector(ImpactPoint),
				FRotationMatrix::MakeFromZ(SpawnDirection).Rotator(),
				FVector::OneVector,
				true,
				false);

			if (EffectComponent)
			{
				EffectComponent->SetVariableFloat(TEXT("User.DensityScale"), 0.04f);
				EffectComponent->SetVariableFloat(TEXT("User.VelocityScale"), 0.11f);
				EffectComponent->SetVariableLinearColor(
					TEXT("NPC.PyroGlobals.Spark_DefaultColor"),
					FLinearColor(1.0f, 0.96f, 0.92f, 1.0f));
				EffectComponent->Activate(true);
			}
		}
	}

	if (MiningHitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, MiningHitSound, FVector(ImpactPoint));
	}
}

int32 AOreBase::GetDefaultMaxDurabilityForOreType() const
{
	switch (OreId)
	{
	case EOreType::Iron:
		return 100;
	case EOreType::Gold:
		return 200;
	case EOreType::Diamond:
		return 500;
	default:
		return 100;
	}
}

void AOreBase::OnRep_CurrentDurability(const int32 PreviousDurability)
{
	OnDurabilityChanged.Broadcast(CurrentDurability, MaxDurability);

	if (CurrentDurability <= 0 && PreviousDurability > 0)
	{
		OnOreDepleted.Broadcast();
	}
}
