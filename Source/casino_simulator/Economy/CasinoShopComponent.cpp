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
#include "casino_simulatorCharacter.h"

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
	const int64 Total = static_cast<int64>(GetItemUnitPrice(ItemId)) * FMath::Max(Quantity, 0);
	return Total <= MAX_int32 ? static_cast<int32>(Total) : 0;
}

bool UCasinoShopComponent::BuyShopItem(FName ItemId, int32 Quantity)
{
    FailPurchase(TEXT("Use PlayerController.ServerBuyShopItem with this shop."));
    return false;
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

bool UCasinoShopComponent::ProcessPurchase(Acasino_simulatorCharacter* Buyer, FName ItemId, int32 Quantity, int32& OutPrice, FString& OutReason)
{
    OutPrice = 0;
    if (!IsValid(Buyer) || !Buyer->HasAuthority() || !IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() || Buyer->GetWorld() != GetWorld())
    { OutReason = TEXT("Invalid buyer or shop."); return false; }
    if (!FMath::IsFinite(PurchaseRadius) || FVector::DistSquared(Buyer->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(FMath::Max(1.0f, PurchaseRadius)))
    { OutReason = TEXT("Too far from the shop."); return false; }
    if (!ValidateQuantity(Quantity, OutReason)) return false;
    const FCasinoShopItemData* FoundItem = FindShopItem(ItemId);
    if (!FoundItem) { OutReason = TEXT("Shop item was not found."); return false; }
    FCasinoShopItemData Item = *FoundItem;
    ApplyInventoryItemData(Item);
    const int64 Price = static_cast<int64>(GetItemUnitPrice(ItemId)) * Quantity;
    if (Price <= 0 || Price > MAX_int32)
    { OutReason = TEXT("Invalid total price."); return false; }
    const int32 TotalPrice = static_cast<int32>(Price);
    if (!CanGrantPurchasedItems(Buyer, Item, OutReason)) return false;
    if (!TrySpendForPurchase(Buyer, TotalPrice))
    { OutReason = TEXT("Not enough personal money."); return false; }
    if (!GrantPurchasedItems(Buyer, Item, Quantity))
    { RefundPurchase(Buyer, TotalPrice); OutReason = TEXT("Could not add item to inventory."); return false; }
    const bool bShouldApplyEffects = Item.bApplyEffectsOnPurchase || Item.InventoryItemID == INDEX_NONE;
    if (bShouldApplyEffects && !ApplyItemEffects(Buyer, Item, Quantity))
    {
        if (Item.InventoryItemID != INDEX_NONE)
            if (auto* PS = Buyer->GetPlayerState<Acasino_simulatorPlayerState>())
                PS->RemoveItem(Item.InventoryItemID, Quantity);
        RefundPurchase(Buyer, TotalPrice);
        OutReason = TEXT("Could not apply item effect."); return false;
    }
    OutPrice = TotalPrice;
    return true;
}

bool UCasinoShopComponent::TrySpendForPurchase(Acasino_simulatorCharacter* Buyer, int32 Price)
{
	if (Price <= 0)
	{
		return true;
	}

	const AActor* Owner = Buyer;
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
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

void UCasinoShopComponent::RefundPurchase(Acasino_simulatorCharacter* Buyer, int32 Price)
{
	if (Price <= 0)
	{
		return;
	}

	const AActor* Owner = Buyer;
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->ApplyModToAttribute(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), EGameplayModOp::Additive, static_cast<float>(Price));
}

bool UCasinoShopComponent::CanGrantPurchasedItems(Acasino_simulatorCharacter* Buyer, const FCasinoShopItemData& Item, FString& OutReason) const
{
	if (Item.InventoryItemID == INDEX_NONE)
	{
		return true;
	}

	const APawn* OwnerPawn = Buyer;
	const Acasino_simulatorPlayerState* CasinoPlayerState = OwnerPawn ? OwnerPawn->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
	if (!CasinoPlayerState)
	{
		OutReason = TEXT("Player inventory was not found.");
		return false;
	}

	return true;
}

bool UCasinoShopComponent::GrantPurchasedItems(Acasino_simulatorCharacter* Buyer, const FCasinoShopItemData& Item, int32 Quantity)
{
	if (Item.InventoryItemID == INDEX_NONE)
	{
		return true;
	}

	APawn* OwnerPawn = Buyer;
	Acasino_simulatorPlayerState* CasinoPlayerState = OwnerPawn ? OwnerPawn->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
	if (!CasinoPlayerState)
	{
		return false;
	}

	return CasinoPlayerState->AddItem(Item.InventoryItemID, Quantity) == Quantity;
}

bool UCasinoShopComponent::ApplyItemEffects(Acasino_simulatorCharacter* Buyer, const FCasinoShopItemData& Item, int32 Quantity)
{
	bool bAppliedAnyEffect = false;
	const float EffectLevel = FMath::Max(static_cast<float>(Quantity), 1.0f);

	if (ApplyGameplayEffect(Buyer, Item.RecoveryEffectClass, EffectLevel))
	{
		bAppliedAnyEffect = true;
	}

	for (const TSubclassOf<UGameplayEffect>& BonusEffectClass : Item.BonusEffectClasses)
	{
		if (ApplyGameplayEffect(Buyer, BonusEffectClass, EffectLevel))
		{
			bAppliedAnyEffect = true;
		}
	}

	const float TotalRecovery = Item.RestoreAmount * Quantity;
	if (ApplyFallbackAttributeRecovery(Buyer, Item, TotalRecovery))
	{
		bAppliedAnyEffect = true;
	}

	return bAppliedAnyEffect || Item.RecoveryType == ECasinoShopRecoveryType::None;
}

bool UCasinoShopComponent::ApplyGameplayEffect(Acasino_simulatorCharacter* Buyer, TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!EffectClass)
	{
		return false;
	}

	AActor* Owner = Buyer;
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

bool UCasinoShopComponent::ApplyFallbackAttributeRecovery(Acasino_simulatorCharacter* Buyer, const FCasinoShopItemData& Item, float TotalRecovery)
{
	if (Item.RecoveryType == ECasinoShopRecoveryType::None || TotalRecovery <= 0.0f)
	{
		return false;
	}

	AActor* Owner = Buyer;
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

	return nullptr;
}

bool UCasinoShopComponent::ApplyInventoryItemData(FCasinoShopItemData& Item) const
{
	if (Item.InventoryItemID == INDEX_NONE)
	{
		return false;
	}

	const UDataTable* ResolvedItemDataTable = GetResolvedItemDataTable();
	if (!ResolvedItemDataTable || ResolvedItemDataTable->GetRowStruct() != FItemData::StaticStruct())
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
	if (!ShopItemDataTable || ShopItemDataTable->GetRowStruct() != FCasinoShopItemData::StaticStruct())
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
	OnPurchaseFailed.Broadcast(Reason);
}
