// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "CasinoShopComponent.generated.h"

class UGameplayEffect;
class UTexture2D;

UENUM(BlueprintType)
enum class ECasinoShopItemCategory : uint8
{
	Cigarette UMETA(DisplayName="Cigarette"),
	Alcohol UMETA(DisplayName="Alcohol")
};

UENUM(BlueprintType)
enum class ECasinoShopRecoveryType : uint8
{
	None UMETA(DisplayName="None"),
	Nicotine UMETA(DisplayName="Nicotine"),
	Alcohol UMETA(DisplayName="Alcohol")
};

USTRUCT(BlueprintType)
struct FCasinoShopItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop")
	ECasinoShopItemCategory Category = ECasinoShopItemCategory::Cigarette;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop", meta=(ClampMin="1"))
	int32 BasePrice = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop|Inventory")
	int32 InventoryItemID = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop")
	ECasinoShopRecoveryType RecoveryType = ECasinoShopRecoveryType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop", meta=(ClampMin="0.0"))
	float RestoreAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop")
	TSubclassOf<UGameplayEffect> RecoveryEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop")
	TArray<TSubclassOf<UGameplayEffect>> BonusEffectClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop")
	bool bApplyEffectsOnPurchase = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Casino|Shop", meta=(MultiLine="true"))
	FText Description;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnShopPurchaseCompleted, FName, ItemId, int32, Quantity, int32, TotalPrice);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShopPurchaseFailed, const FString&, Reason);

UCLASS(ClassGroup=(Casino), meta=(BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UCasinoShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCasinoShopComponent();

	UFUNCTION(BlueprintPure, Category="Casino|Shop")
	TArray<FCasinoShopItemData> GetShopItems() const;

	UFUNCTION(BlueprintPure, Category="Casino|Shop")
	TArray<FCasinoShopItemData> GetShopItemsByCategory(ECasinoShopItemCategory Category) const;

	UFUNCTION(BlueprintPure, Category="Casino|Shop")
	bool GetShopItem(FName ItemId, FCasinoShopItemData& OutItem) const;

	UFUNCTION(BlueprintPure, Category="Casino|Shop")
	int32 GetItemUnitPrice(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category="Casino|Shop")
	int32 GetItemTotalPrice(FName ItemId, int32 Quantity) const;

	UFUNCTION(BlueprintCallable, Category="Casino|Shop")
	bool BuyShopItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Casino|Shop")
	bool BuyCigarette(int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Casino|Shop")
	bool BuyAlcohol(int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Casino|Shop")
	void SetCurrentDay(int32 NewCurrentDay);

	UFUNCTION(BlueprintPure, Category="Casino|Shop")
	int32 GetCurrentDay() const { return CurrentDay; }

	UFUNCTION(BlueprintPure, Category="Casino|Shop")
	float GetCurrentPriceMultiplier() const;

	UFUNCTION(BlueprintCallable, Category="Casino|Shop")
	void ReloadShopItems();

	UPROPERTY(BlueprintAssignable, Category="Casino|Shop")
	FOnShopPurchaseCompleted OnPurchaseCompleted;

	UPROPERTY(BlueprintAssignable, Category="Casino|Shop")
	FOnShopPurchaseFailed OnPurchaseFailed;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Shop|Items")
	TObjectPtr<UDataTable> ShopItemDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Shop|Items")
	TObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Shop|Items")
	TArray<FCasinoShopItemData> ShopItems;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Shop|Price", meta=(ClampMin="1"))
	int32 CurrentDay = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Shop|Price", meta=(ClampMin="0.0"))
	float PriceIncreasePerDay = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Shop|Prototype")
	bool bAllowFreePurchasesUntilEconomyExists = true;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION(Server, Reliable)
	void ServerBuyShopItem(FName ItemId, int32 Quantity);

	UFUNCTION(Client, Reliable)
	void ClientPurchaseCompleted(FName ItemId, int32 Quantity, int32 TotalPrice);

	UFUNCTION(Client, Reliable)
	void ClientPurchaseFailed(const FString& Reason);

	bool ProcessPurchase(FName ItemId, int32 Quantity);
	bool TrySpendForPurchase(int32 Price);
	void RefundPurchase(int32 Price);
	bool CanGrantPurchasedItems(const FCasinoShopItemData& Item, FString& OutReason) const;
	bool GrantPurchasedItems(const FCasinoShopItemData& Item, int32 Quantity);
	bool ApplyItemEffects(const FCasinoShopItemData& Item, int32 Quantity);
	bool ApplyGameplayEffect(TSubclassOf<UGameplayEffect> EffectClass, float Level);
	bool ApplyFallbackAttributeRecovery(const FCasinoShopItemData& Item, float TotalRecovery);
	bool ValidateQuantity(int32 Quantity, FString& OutReason) const;
	int32 GetScaledPrice(int32 BasePrice) const;
	const FCasinoShopItemData* FindShopItem(FName ItemId) const;
	const UDataTable* GetResolvedItemDataTable() const;
	bool ApplyInventoryItemData(FCasinoShopItemData& Item) const;
	void BuildDefaultShopItems();
	bool LoadShopItemsFromDataTable();
	void FailPurchase(const FString& Reason);
};
