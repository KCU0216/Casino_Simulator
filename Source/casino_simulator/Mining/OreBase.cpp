	#include "Mining/OreBase.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AOreBase::AOreBase()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;

	OreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OreMesh"));
	SetRootComponent(OreMesh);
	OreMesh->SetCollisionProfileName(TEXT("BlockAll"));
	OreMesh->SetGenerateOverlapEvents(false);
}

void AOreBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
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
	if (!HasAuthority() || Damage <= 0 || IsDepleted())
	{
		return false;
	}

	CurrentDurability = FMath::Max(0, CurrentDurability - Damage);
	OnDurabilityChanged.Broadcast(CurrentDurability, MaxDurability);
	OnMiningHitApplied(CurrentDurability);

	if (IsDepleted())
	{
		OnOreDepleted.Broadcast();
		ReceiveOreDepleted();
		Destroy();
	}

	return true;
}

void AOreBase::OnRep_CurrentDurability(const int32 PreviousDurability)
{
	OnDurabilityChanged.Broadcast(CurrentDurability, MaxDurability);

	if (CurrentDurability <= 0 && PreviousDurability > 0)
	{
		OnOreDepleted.Broadcast();
	}
}
