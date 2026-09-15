#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mining/OreTypes.h"
#include "MiningSellZone.generated.h"

class AOrePickupBase;
class Acasino_simulatorCharacter;
class USceneComponent;
class USphereComponent;

UCLASS(Blueprintable)
class CASINO_SIMULATOR_API AMiningSellZone : public AActor
{
	GENERATED_BODY()

public:
	AMiningSellZone();

	UFUNCTION(BlueprintPure, Category = "Mining|Sell")
	USphereComponent* GetSellSphere() const { return SellSphere; }

	UFUNCTION(BlueprintPure, Category = "Mining|Sell")
	int32 GetSalePrice(EOreType OreType) const;

	UFUNCTION(BlueprintCallable, Category = "Mining|Sell")
	bool TrySellOre(AOrePickupBase* OrePickup);

	UFUNCTION(BlueprintImplementableEvent, Category = "Mining|Sell")
	void ReceiveOreSold(AOrePickupBase* OrePickup, Acasino_simulatorCharacter* Seller, int32 SalePrice);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Sell|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Sell|Components")
	TObjectPtr<USphereComponent> SellSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Sell")
	int32 IronSalePrice = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Sell")
	int32 GoldSalePrice = 120;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Sell")
	int32 DiamondSalePrice = 400;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Sell")
	bool bRequireDroppedOre = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Sell", meta = (ClampMin = "0.05"))
	float SellRetryInterval = 0.15f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AOrePickupBase>> OverlappingOres;

	FTimerHandle SellRetryTimerHandle;

	UFUNCTION()
	void OnSellSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnSellSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void RetrySellOverlappingOres();
	void StartSellRetryTimer();
	void StopSellRetryTimerIfIdle();
	bool IsValidSellOverlap(AOrePickupBase* OrePickup, const UPrimitiveComponent* OtherComp) const;
};
