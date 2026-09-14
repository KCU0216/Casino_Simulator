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

	if (bRequireDroppedOre && OrePickup->IsBeingCarried())
	{
		return false;
	}

	const TArray<TObjectPtr<Acasino_simulatorCharacter>>& Participants =
		OrePickup->GetSaleParticipants();

	if (Participants.Num() <= 0)
	{
		return false;
	}

	const int32 SalePrice = GetSalePrice(OrePickup->GetOreType());
	if (SalePrice <= 0)
	{
		return false;
	}

	const int32 Share = SalePrice / Participants.Num();
	const int32 Remainder = SalePrice % Participants.Num();

	for (int32 Index = 0; Index < Participants.Num(); ++Index)
	{
		Acasino_simulatorCharacter* Participant = Participants[Index];
		if (!IsValid(Participant))
		{
			continue;
		}

		const int32 Payout = Share + (Index < Remainder ? 1 : 0);
		Participant->AddCurrency(static_cast<float>(Payout));
	}

	//ReceiveOreSold(OrePickup, nullptr, SalePrice);
	OrePickup->Destroy();
	return true;
}
//판매존에 들어온다면 ? 
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
//판매존에서 나가면 목록에서 제외
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
//다시 팔아보기 (타이머 타고 들어옴)
void AMiningSellZone::RetrySellOverlappingOres()
{
	for (int32 Index = OverlappingOres.Num() - 1; Index >= 0; --Index)
	{
		AOrePickupBase* OrePickup = OverlappingOres[Index];
		if (!IsValid(OrePickup))
		{
			OverlappingOres.Remove(OrePickup);
			continue;
		}

		if (TrySellOre(OrePickup))
		{
			OverlappingOres.Remove(OrePickup);
		}
	}

	StopSellRetryTimerIfIdle();
}

//다시팔기 타이머 시작
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

//다시팔기 타이머 멈춤
void AMiningSellZone::StopSellRetryTimerIfIdle()
{
	if (OverlappingOres.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(SellRetryTimerHandle);
	}
}
//광물 잡힌거 있냐
bool AMiningSellZone::IsValidSellOverlap(AOrePickupBase* OrePickup, const UPrimitiveComponent* OtherComp) const
{
	return OrePickup && OtherComp == OrePickup->GetOrePickupMesh();
}
