#include "Mining/MiningSellZone.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Mining/OrePickupBase.h"
#include "casino_simulatorCharacter.h"

AMiningSellZone::AMiningSellZone()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SellSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SellSphere"));
	SellSphere->SetupAttachment(SceneRoot);
	SellSphere->SetSphereRadius(140.0f);
	SellSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SellSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	SellSphere->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SellSphere->SetGenerateOverlapEvents(true);
	SellSphere->OnComponentBeginOverlap.AddDynamic(this, &AMiningSellZone::OnSellSphereBeginOverlap);
	SellSphere->OnComponentEndOverlap.AddDynamic(this, &AMiningSellZone::OnSellSphereEndOverlap);
}

int32 AMiningSellZone::GetSalePrice(const EOreType OreType) const
{
	switch (OreType)
	{
	case EOreType::Iron:
		return IronSalePrice;
	case EOreType::Gold:
		return GoldSalePrice;
	case EOreType::Diamond:
		return DiamondSalePrice;
	default:
		return 0;
	}
}

bool AMiningSellZone::TrySellOre(AOrePickupBase* OrePickup)
{
	if (!HasAuthority() || !OrePickup)
	{
		return false;
	}

	if (bRequireDroppedOre && OrePickup->GetCarrier())
	{
		return false;
	}

	Acasino_simulatorCharacter* Seller = OrePickup->GetLastCarrier();
	if (!Seller)
	{
		return false;
	}

	const int32 SalePrice = GetSalePrice(OrePickup->GetOreType());
	if (SalePrice <= 0)
	{
		return false;
	}

	Seller->AddCurrency(static_cast<float>(SalePrice));
	ReceiveOreSold(OrePickup, Seller, SalePrice);
	OrePickup->Destroy();
	return true;
}

void AMiningSellZone::OnSellSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	AOrePickupBase* OrePickup = Cast<AOrePickupBase>(OtherActor);
	if (!IsValidSellOverlap(OrePickup, OtherComp))
	{
		return;
	}

	OverlappingOres.AddUnique(OrePickup);
	TrySellOre(OrePickup);
	StartSellRetryTimer();
}

void AMiningSellZone::OnSellSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	AOrePickupBase* OrePickup = Cast<AOrePickupBase>(OtherActor);
	if (!IsValidSellOverlap(OrePickup, OtherComp))
	{
		return;
	}

	OverlappingOres.Remove(OrePickup);
	StopSellRetryTimerIfIdle();
}

void AMiningSellZone::RetrySellOverlappingOres()
{
	for (int32 Index = OverlappingOres.Num() - 1; Index >= 0; --Index)
	{
		AOrePickupBase* OrePickup = OverlappingOres[Index];
		if (!IsValid(OrePickup))
		{
			OverlappingOres.RemoveAtSwap(Index);
			continue;
		}

		if (TrySellOre(OrePickup))
		{
			OverlappingOres.RemoveAtSwap(Index);
		}
	}

	StopSellRetryTimerIfIdle();
}

void AMiningSellZone::StartSellRetryTimer()
{
	if (!GetWorld() || GetWorldTimerManager().IsTimerActive(SellRetryTimerHandle))
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		SellRetryTimerHandle,
		this,
		&AMiningSellZone::RetrySellOverlappingOres,
		FMath::Max(SellRetryInterval, 0.05f),
		true
	);
}

void AMiningSellZone::StopSellRetryTimerIfIdle()
{
	if (OverlappingOres.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(SellRetryTimerHandle);
	}
}

bool AMiningSellZone::IsValidSellOverlap(AOrePickupBase* OrePickup, const UPrimitiveComponent* OtherComp) const
{
	return OrePickup && OtherComp == OrePickup->GetOrePickupMesh();
}
