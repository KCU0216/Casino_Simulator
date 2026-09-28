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

}

void UCasinoShopComponent::BeginPlay()
{
	Super::BeginPlay();

	ReloadShopItems();
}



void UCasinoShopComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCasinoShopComponent, CurrentDay);
}

TArray<FCasinoShopItemData> UCasinoShopComponent::GetShopItems() const
{
 TArray<FCasinoShopItemData> Items;
 TSet<int32> Seen;
 for (int32 ID : SoldItemIDs)
 {
  if (ID == INDEX_NONE || Seen.Contains(ID)) continue;
  Seen.Add(ID);
  FCasinoShopItemData Item;
  Item.InventoryItemID = ID;
  if (ApplyInventoryItemData(Item)) Items.Add(Item);
 }
 return Items;
}

TArray<FCasinoShopItemData> UCasinoShopComponent::GetShopItemsByCategory(ECasinoShopItemCategory Category) const
{
	TArray<FCasinoShopItemData> MatchingItems;
	for (const FCasinoShopItemData& Item : GetShopItems())
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
 OutItem = FCasinoShopItemData();
 // Resolve only advertised IDs; a client cannot request another table entry.
 for (int32 ID : SoldItemIDs)
 {
  if (ID != INDEX_NONE && FName(*FString::FromInt(ID)) == ItemId)
  {
   OutItem.InventoryItemID = ID;
   return ApplyInventoryItemData(OutItem);
  }
 }
 return false;
}

int32 UCasinoShopComponent::GetItemUnitPrice(FName ItemId) const
{
 FCasinoShopItemData Item;
 return GetShopItem(ItemId, Item) ? GetScaledPrice(Item.BasePrice) : 0;
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
 ShopItems = GetShopItems();
}

bool UCasinoShopComponent::ProcessPurchase(Acasino_simulatorCharacter* Buyer, FName ItemId, int32 Quantity, int32& OutPrice, FString& OutReason)
{
    OutPrice = 0;
    OutReason.Reset();
    if (!IsValid(Buyer) || !Buyer->HasAuthority() || !IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() || Buyer->GetWorld() != GetWorld())
    { OutReason = TEXT("Invalid buyer or shop."); return false; }
    if (!FMath::IsFinite(PurchaseRadius) || FVector::DistSquared(Buyer->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(FMath::Max(1.0f, PurchaseRadius)))
    { OutReason = TEXT("Too far from the shop."); return false; }
    if (!ValidateQuantity(Quantity, OutReason)) return false;
    FCasinoShopItemData Item;
    if (!GetShopItem(ItemId, Item)) { OutReason = TEXT("Item is not sold here or its definition is invalid."); return false; }
    const int64 Price = static_cast<int64>(GetItemUnitPrice(ItemId)) * Quantity;
    if (Price <= 0 || Price > MAX_int32)
    { OutReason = TEXT("Invalid total price."); return false; }
    const int32 TotalPrice = static_cast<int32>(Price);
    if (!CanGrantPurchasedItems(Buyer, Item, OutReason)) return false;
    if (!TrySpendForPurchase(Buyer, TotalPrice))
    { OutReason = TEXT("Not enough personal money."); return false; }
    if (!GrantPurchasedItems(Buyer, Item, Quantity))
    { RefundPurchase(Buyer, TotalPrice); OutReason = TEXT("Could not add item to inventory."); return false; }
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
	if (!FMath::IsFinite(CurrentCurrency) || CurrentCurrency < static_cast<float>(Price))
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
 const auto* PS = Buyer ? Buyer->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
 FItemData Definition;
 if (!PS || PS->ItemDataTable != ItemDataTable || !PS->FindItemData(Item.InventoryItemID, Definition))
 {
  OutReason = TEXT("Shop and player inventory must use the same item definition table.");
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
 const double Price = static_cast<double>(BasePrice) * GetCurrentPriceMultiplier();
 if (BasePrice <= 0 || !FMath::IsFinite(Price) || Price <= 0.0 || Price > MAX_int32) return 0;
 return FMath::Max(FMath::RoundToInt(Price), 1);
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
 const UDataTable* Table = GetResolvedItemDataTable();
 if (Item.InventoryItemID == INDEX_NONE || !Table || Table->GetRowStruct() != FItemData::StaticStruct()) return false;
 const FItemData* Definition = nullptr;
 for (const auto& Pair : Table->GetRowMap())
 {
  const FItemData* Row = reinterpret_cast<const FItemData*>(Pair.Value);
  if (Row && Row->UniqueID == Item.InventoryItemID)
  {
   if (Definition) return false; // Ambiguous IDs must never grant a different inventory item.
   Definition = Row;
  }
 }
 if (!Definition) return false;
 Item.ItemId = FName(*FString::FromInt(Item.InventoryItemID));
 Item.DisplayName = Definition->DisplayName;
 Item.Description = Definition->Description;
 Item.Icon = Definition->Icon.LoadSynchronous();
 Item.BasePrice = Definition->Price;
 Item.RestoreAmount = Definition->EffectMagnitude;
 Item.RecoveryEffectClass = Definition->OnUseEffect;
 Item.bApplyEffectsOnPurchase = false; // Consumption belongs to the inventory use action.
 Item.Category = ECasinoShopItemCategory::Other;
 Item.RecoveryType = ECasinoShopRecoveryType::None;
 const FString Tag = Definition->ItemCategory.ToString();
 if (Tag == TEXT("Item.Consumable.Cigarette") || Tag.StartsWith(TEXT("Item.Consumable.Cigarette.")))
 {
  Item.Category = ECasinoShopItemCategory::Cigarette;
  Item.RecoveryType = ECasinoShopRecoveryType::Nicotine;
 }
 else if (Tag == TEXT("Item.Consumable.Alcohol") || Tag.StartsWith(TEXT("Item.Consumable.Alcohol.")))
 {
  Item.Category = ECasinoShopItemCategory::Alcohol;
  Item.RecoveryType = ECasinoShopRecoveryType::Alcohol;
 }
 return true;
}

void UCasinoShopComponent::FailPurchase(const FString& Reason)
{
	OnPurchaseFailed.Broadcast(Reason);
}
