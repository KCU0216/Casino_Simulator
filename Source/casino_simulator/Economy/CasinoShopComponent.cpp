// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/CasinoShopComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Item/ItemData.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulatorPlayerState.h"

UCasinoShopComponent::UCasinoShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	BuildDefaultShopItems();
}

void UCasinoShopComponent::BeginPlay()
{
	Super::BeginPlay();

	ReloadShopItems();
}

void UCasinoShopComponent::BuildDefaultShopItems()
{
	FCasinoShopItemData CheapCigarette;
	CheapCigarette.ItemId = TEXT("CheapCigarette");
	CheapCigarette.DisplayName = FText::FromString(TEXT("Cheap Cigarette"));
	CheapCigarette.Category = ECasinoShopItemCategory::Cigarette;
	CheapCigarette.BasePrice = 100;
	CheapCigarette.RecoveryType = ECasinoShopRecoveryType::Nicotine;
	CheapCigarette.RestoreAmount = 15.0f;
	CheapCigarette.Description = FText::FromString(TEXT("카지노 뒷문 근처에서만 유통된다는 소문이 있다. 피우면 니코틴은 차지만 자존감은 조금 깎인다."));

	FCasinoShopItemData RegularCigarette;
	RegularCigarette.ItemId = TEXT("RegularCigarette");
	RegularCigarette.DisplayName = FText::FromString(TEXT("Regular Cigarette"));
	RegularCigarette.Category = ECasinoShopItemCategory::Cigarette;
	RegularCigarette.BasePrice = 180;
	RegularCigarette.RecoveryType = ECasinoShopRecoveryType::Nicotine;
	RegularCigarette.RestoreAmount = 30.0f;
	RegularCigarette.Description = FText::FromString(TEXT("도박왕이 즐겨 피던 담배. 황금 필터가 둘러져 있고, 가끔 이빨에 금이 낀다는 컴플레인이 걸려온다."));

	FCasinoShopItemData Beer;
	Beer.ItemId = TEXT("Beer");
	Beer.DisplayName = FText::FromString(TEXT("Beer"));
	Beer.Category = ECasinoShopItemCategory::Alcohol;
	Beer.BasePrice = 150;
	Beer.RecoveryType = ECasinoShopRecoveryType::Alcohol;
	Beer.RestoreAmount = 15.0f;
	Beer.Description = FText::FromString(TEXT("거품이 많은 맥주. 사장은 프리미엄이라고 우기지만 컵 아래쪽에서는 편의점 냄새가 난다."));

	FCasinoShopItemData Whiskey;
	Whiskey.ItemId = TEXT("Whiskey");
	Whiskey.DisplayName = FText::FromString(TEXT("Whiskey"));
	Whiskey.Category = ECasinoShopItemCategory::Alcohol;
	Whiskey.BasePrice = 300;
	Whiskey.RecoveryType = ECasinoShopRecoveryType::Alcohol;
	Whiskey.RestoreAmount = 35.0f;
	Whiskey.Description = FText::FromString(TEXT("칩을 잃은 사람들이 마지막으로 고르는 위스키. 한 잔 마시면 판단력은 흐려지고 자신감은 매우 선명해진다."));

	ShopItems = { CheapCigarette, RegularCigarette, Beer, Whiskey };
}

void UCasinoShopComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCasinoShopComponent, CurrentDay);
}

TArray<FCasinoShopItemData> UCasinoShopComponent::GetShopItems() const
{
	TArray<FCasinoShopItemData> ResolvedItems;
	ResolvedItems.Reserve(ShopItems.Num());

	for (const FCasinoShopItemData& ShopItem : ShopItems)
	{
		FCasinoShopItemData ResolvedItem = ShopItem;
		ApplyInventoryItemData(ResolvedItem);
		ResolvedItems.Add(ResolvedItem);
	}

	return ResolvedItems;
}

TArray<FCasinoShopItemData> UCasinoShopComponent::GetShopItemsByCategory(ECasinoShopItemCategory Category) const
{
	TArray<FCasinoShopItemData> MatchingItems;
	for (const FCasinoShopItemData& Item : ShopItems)
	{
		if (Item.Category == Category)
		{
			FCasinoShopItemData ResolvedItem = Item;
			ApplyInventoryItemData(ResolvedItem);
			MatchingItems.Add(ResolvedItem);
		}
	}

	return MatchingItems;
}

bool UCasinoShopComponent::GetShopItem(FName ItemId, FCasinoShopItemData& OutItem) const
{
	if (const FCasinoShopItemData* Item = FindShopItem(ItemId))
	{
		OutItem = *Item;
		ApplyInventoryItemData(OutItem);
		return true;
	}

	return false;
}

int32 UCasinoShopComponent::GetItemUnitPrice(FName ItemId) const
{
	if (const FCasinoShopItemData* Item = FindShopItem(ItemId))
	{
		FCasinoShopItemData ResolvedItem = *Item;
		ApplyInventoryItemData(ResolvedItem);
		return GetScaledPrice(ResolvedItem.BasePrice);
	}

	return 0;
}

int32 UCasinoShopComponent::GetItemTotalPrice(FName ItemId, int32 Quantity) const
{
	return GetItemUnitPrice(ItemId) * FMath::Max(Quantity, 0);
}

bool UCasinoShopComponent::BuyShopItem(FName ItemId, int32 Quantity)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerBuyShopItem(ItemId, Quantity);
		return true;
	}

	return ProcessPurchase(ItemId, Quantity);
}

bool UCasinoShopComponent::BuyCigarette(int32 Quantity)
{
	return BuyShopItem(TEXT("RegularCigarette"), Quantity);
}

bool UCasinoShopComponent::BuyAlcohol(int32 Quantity)
{
	return BuyShopItem(TEXT("Beer"), Quantity);
}

void UCasinoShopComponent::SetCurrentDay(int32 NewCurrentDay)
{
	CurrentDay = FMath::Max(NewCurrentDay, 1);
}

float UCasinoShopComponent::GetCurrentPriceMultiplier() const
{
	const int32 DayIndex = FMath::Max(CurrentDay - 1, 0);
	return 1.0f + (PriceIncreasePerDay * DayIndex);
}

void UCasinoShopComponent::ReloadShopItems()
{
	if (!LoadShopItemsFromDataTable())
	{
		BuildDefaultShopItems();
	}
}

void UCasinoShopComponent::ServerBuyShopItem_Implementation(FName ItemId, int32 Quantity)
{
	ProcessPurchase(ItemId, Quantity);
}

void UCasinoShopComponent::ClientPurchaseCompleted_Implementation(FName ItemId, int32 Quantity, int32 TotalPrice)
{
	OnPurchaseCompleted.Broadcast(ItemId, Quantity, TotalPrice);
}

void UCasinoShopComponent::ClientPurchaseFailed_Implementation(const FString& Reason)
{
	OnPurchaseFailed.Broadcast(Reason);
}

bool UCasinoShopComponent::ProcessPurchase(FName ItemId, int32 Quantity)
{
	FString Reason;
	if (!ValidateQuantity(Quantity, Reason))
	{
		FailPurchase(Reason);
		return false;
	}

	const FCasinoShopItemData* FoundItem = FindShopItem(ItemId);
	if (!FoundItem)
	{
		FailPurchase(TEXT("Shop item was not found."));
		return false;
	}

	FCasinoShopItemData Item = *FoundItem;
	ApplyInventoryItemData(Item);

	const int32 TotalPrice = GetItemTotalPrice(ItemId, Quantity);
	if (!CanGrantPurchasedItems(Item, Reason))
	{
		FailPurchase(Reason);
		return false;
	}

	if (!TrySpendForPurchase(TotalPrice))
	{
		FailPurchase(TEXT("Not enough personal money."));
		return false;
	}

	if (!GrantPurchasedItems(Item, Quantity))
	{
		RefundPurchase(TotalPrice);
		FailPurchase(TEXT("Could not add item to inventory."));
		return false;
	}

	const bool bShouldApplyEffects = Item.bApplyEffectsOnPurchase || Item.InventoryItemID == INDEX_NONE;
	if (bShouldApplyEffects && !ApplyItemEffects(Item, Quantity))
	{
		RefundPurchase(TotalPrice);
		FailPurchase(TEXT("Could not apply item effect."));
		return false;
	}

	ClientPurchaseCompleted(ItemId, Quantity, TotalPrice);
	OnPurchaseCompleted.Broadcast(ItemId, Quantity, TotalPrice);
	return true;
}

bool UCasinoShopComponent::TrySpendForPurchase(int32 Price)
{
	if (Price <= 0)
	{
		return true;
	}

	const AActor* Owner = GetOwner();
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return bAllowFreePurchasesUntilEconomyExists;
	}

	const FGameplayAttribute CurrencyAttribute = Ucasino_simulatorAttributeSet::GetCurrencyAttribute();
	const float CurrentCurrency = AbilitySystemComponent->GetNumericAttribute(CurrencyAttribute);
	if (CurrentCurrency < static_cast<float>(Price))
	{
		return false;
	}

	AbilitySystemComponent->ApplyModToAttribute(CurrencyAttribute, EGameplayModOp::Additive, -static_cast<float>(Price));
	return true;
}

void UCasinoShopComponent::RefundPurchase(int32 Price)
{
	if (Price <= 0)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->ApplyModToAttribute(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), EGameplayModOp::Additive, static_cast<float>(Price));
}

bool UCasinoShopComponent::CanGrantPurchasedItems(const FCasinoShopItemData& Item, FString& OutReason) const
{
	if (Item.InventoryItemID == INDEX_NONE)
	{
		return true;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const Acasino_simulatorPlayerState* CasinoPlayerState = OwnerPawn ? OwnerPawn->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
	if (!CasinoPlayerState)
	{
		OutReason = TEXT("Player inventory was not found.");
		return false;
	}

	return true;
}

bool UCasinoShopComponent::GrantPurchasedItems(const FCasinoShopItemData& Item, int32 Quantity)
{
	if (Item.InventoryItemID == INDEX_NONE)
	{
		return true;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	Acasino_simulatorPlayerState* CasinoPlayerState = OwnerPawn ? OwnerPawn->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
	if (!CasinoPlayerState)
	{
		return false;
	}

	return CasinoPlayerState->AddItem(Item.InventoryItemID, Quantity) == Quantity;
}

bool UCasinoShopComponent::ApplyItemEffects(const FCasinoShopItemData& Item, int32 Quantity)
{
	bool bAppliedAnyEffect = false;
	const float EffectLevel = FMath::Max(static_cast<float>(Quantity), 1.0f);

	if (ApplyGameplayEffect(Item.RecoveryEffectClass, EffectLevel))
	{
		bAppliedAnyEffect = true;
	}

	for (const TSubclassOf<UGameplayEffect>& BonusEffectClass : Item.BonusEffectClasses)
	{
		if (ApplyGameplayEffect(BonusEffectClass, EffectLevel))
		{
			bAppliedAnyEffect = true;
		}
	}

	const float TotalRecovery = Item.RestoreAmount * Quantity;
	if (ApplyFallbackAttributeRecovery(Item, TotalRecovery))
	{
		bAppliedAnyEffect = true;
	}

	return bAppliedAnyEffect || Item.RecoveryType == ECasinoShopRecoveryType::None;
}

bool UCasinoShopComponent::ApplyGameplayEffect(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!EffectClass)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, Level, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}

bool UCasinoShopComponent::ApplyFallbackAttributeRecovery(const FCasinoShopItemData& Item, float TotalRecovery)
{
	if (Item.RecoveryType == ECasinoShopRecoveryType::None || TotalRecovery <= 0.0f)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	FGameplayAttribute Attribute;
	if (Item.RecoveryType == ECasinoShopRecoveryType::Nicotine)
	{
		Attribute = Ucasino_simulatorAttributeSet::GetNicotineAttribute();
	}
	else if (Item.RecoveryType == ECasinoShopRecoveryType::Alcohol)
	{
		Attribute = Ucasino_simulatorAttributeSet::GetAlcoholAttribute();
	}
	else
	{
		return false;
	}

	AbilitySystemComponent->ApplyModToAttribute(Attribute, EGameplayModOp::Additive, TotalRecovery);
	return true;
}

bool UCasinoShopComponent::ValidateQuantity(int32 Quantity, FString& OutReason) const
{
	if (Quantity <= 0)
	{
		OutReason = TEXT("Quantity must be greater than zero.");
		return false;
	}

	return true;
}

int32 UCasinoShopComponent::GetScaledPrice(int32 BasePrice) const
{
	if (BasePrice <= 0)
	{
		return 0;
	}

	return FMath::Max(FMath::RoundToInt(BasePrice * GetCurrentPriceMultiplier()), 1);
}

const FCasinoShopItemData* UCasinoShopComponent::FindShopItem(FName ItemId) const
{
	return ShopItems.FindByPredicate([ItemId](const FCasinoShopItemData& Item)
	{
		return Item.ItemId == ItemId;
	});
}

const UDataTable* UCasinoShopComponent::GetResolvedItemDataTable() const
{
	if (ItemDataTable)
	{
		return ItemDataTable;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const Acasino_simulatorPlayerState* CasinoPlayerState = OwnerPawn ? OwnerPawn->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
	return CasinoPlayerState ? CasinoPlayerState->ItemDataTable : nullptr;
}

bool UCasinoShopComponent::ApplyInventoryItemData(FCasinoShopItemData& Item) const
{
	if (Item.InventoryItemID == INDEX_NONE)
	{
		return false;
	}

	const UDataTable* ResolvedItemDataTable = GetResolvedItemDataTable();
	if (!ResolvedItemDataTable)
	{
		return false;
	}

	for (const TPair<FName, uint8*>& RowPair : ResolvedItemDataTable->GetRowMap())
	{
		const FItemData* InventoryItemData = reinterpret_cast<const FItemData*>(RowPair.Value);
		if (!InventoryItemData || InventoryItemData->UniqueID != Item.InventoryItemID)
		{
			continue;
		}

		Item.DisplayName = InventoryItemData->DisplayName;
		Item.Description = InventoryItemData->Description;
		Item.Icon = InventoryItemData->Icon.LoadSynchronous();
		Item.BasePrice = InventoryItemData->Price;
		Item.RestoreAmount = InventoryItemData->EffectMagnitude;
		Item.RecoveryEffectClass = InventoryItemData->OnUseEffect;
		return true;
	}

	return false;
}

bool UCasinoShopComponent::LoadShopItemsFromDataTable()
{
	if (!ShopItemDataTable)
	{
		return false;
	}

	ShopItems.Empty();

	const TMap<FName, uint8*>& RowMap = ShopItemDataTable->GetRowMap();
	for (const TPair<FName, uint8*>& RowPair : RowMap)
	{
		const FCasinoShopItemData* Row = reinterpret_cast<const FCasinoShopItemData*>(RowPair.Value);
		if (!Row)
		{
			continue;
		}

		FCasinoShopItemData Item = *Row;
		if (Item.ItemId.IsNone())
		{
			Item.ItemId = RowPair.Key;
		}

		ShopItems.Add(Item);
	}

	return !ShopItems.IsEmpty();
}

void UCasinoShopComponent::FailPurchase(const FString& Reason)
{
	ClientPurchaseFailed(Reason);
	OnPurchaseFailed.Broadcast(Reason);
}
