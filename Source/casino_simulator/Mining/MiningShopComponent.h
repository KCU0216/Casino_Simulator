// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningShopComponent.generated.h"

class Acasino_simulatorCharacter;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EMiningShopUpgradeType : uint8
{
	Power UMETA(DisplayName = "Power"),
	Speed UMETA(DisplayName = "Speed")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMiningShopPurchaseCompleted, EMiningShopUpgradeType, UpgradeType, int32, TotalPrice);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMiningShopPurchaseFailed, EMiningShopUpgradeType, UpgradeType, const FString&, Reason);

UCLASS(ClassGroup = (Casino), meta = (BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UMiningShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMiningShopComponent();

	UFUNCTION(BlueprintCallable, Category = "Mining|Shop")
	void RequestBuyPowerUpgrade(Acasino_simulatorCharacter* Buyer);

	UFUNCTION(BlueprintCallable, Category = "Mining|Shop")
	void RequestBuySpeedUpgrade(Acasino_simulatorCharacter* Buyer);

	UFUNCTION(BlueprintCallable, Category = "Mining|Shop")
	void RequestBuyUpgrade(Acasino_simulatorCharacter* Buyer, EMiningShopUpgradeType UpgradeType);

	UFUNCTION(BlueprintCallable, Category = "Mining|Shop")
	bool ProcessUpgradePurchase(Acasino_simulatorCharacter* Buyer, EMiningShopUpgradeType UpgradeType);

	UFUNCTION(BlueprintCallable, Category = "Mining|Shop")
	void HandlePurchaseCompletedFromServer(EMiningShopUpgradeType UpgradeType, int32 TotalPrice);

	UFUNCTION(BlueprintCallable, Category = "Mining|Shop")
	void HandlePurchaseFailedFromServer(EMiningShopUpgradeType UpgradeType, const FString& Reason);

	UFUNCTION(BlueprintPure, Category = "Mining|Shop")
	int32 GetUpgradePrice(Acasino_simulatorCharacter* Buyer, EMiningShopUpgradeType UpgradeType) const;

	UPROPERTY(BlueprintAssignable, Category = "Mining|Shop")
	FOnMiningShopPurchaseCompleted OnPurchaseCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Mining|Shop")
	FOnMiningShopPurchaseFailed OnPurchaseFailed;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Shop|Price")
	int32 PowerUpgradeBasePrice = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Shop|Price")
	int32 PowerUpgradePriceIncrease = 75;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Shop|Price")
	int32 SpeedUpgradeBasePrice = 150;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Shop|Price")
	int32 SpeedUpgradePriceIncrease = 100;

private:
	bool ApplyUpgradeEffect(Acasino_simulatorCharacter* Buyer, EMiningShopUpgradeType UpgradeType) const;
	TSubclassOf<UGameplayEffect> GetUpgradeEffectClass(EMiningShopUpgradeType UpgradeType) const;
	void FailPurchase(EMiningShopUpgradeType UpgradeType, const FString& Reason);

	UPROPERTY(EditDefaultsOnly, Category = "Mining|Shop|Effects")
	TSubclassOf<UGameplayEffect> PowerUpgradeEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Mining|Shop|Effects")
	TSubclassOf<UGameplayEffect> SpeedUpgradeEffectClass;
};
