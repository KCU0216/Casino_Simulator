#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/CasinoShopComponent.h"
#include "Item/ItemData.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoShopCatalogTest, "Casino.Shop.SharedCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoShopCatalogTest::RunTest(const FString& Parameters)
{
    auto* Shop = NewObject<UCasinoShopComponent>();
    auto* Table = NewObject<UDataTable>();
    Table->RowStruct = FItemData::StaticStruct();
    auto* TableProperty = FindFProperty<FObjectPropertyBase>(Shop->GetClass(), TEXT("ItemDataTable"));
    auto* IDsProperty = FindFProperty<FArrayProperty>(Shop->GetClass(), TEXT("SoldItemIDs"));
    if (!TestNotNull(TEXT("Definition table property"), TableProperty) ||
        !TestNotNull(TEXT("Sale IDs property"), IDsProperty)) return false;
    TableProperty->SetObjectPropertyValue_InContainer(Shop, Table);
    auto& IDs = *IDsProperty->ContainerPtrToValuePtr<TArray<int32>>(Shop);

    FItemData Row;
    Row.UniqueID = 42;
    Row.DisplayName = FText::FromString(TEXT("Shared product"));
    Row.Price = 100;
    Table->AddRow(TEXT("Product"), Row);
    Row.UniqueID = 99;
    Table->AddRow(TEXT("NotSold"), Row);
    TestEqual(TEXT("No hardcoded fallback products"), Shop->GetShopItems().Num(), 0);
    IDs = {42, 42, 999};
    TestEqual(TEXT("Duplicate selections and missing definitions are skipped"), Shop->GetShopItems().Num(), 1);
    FCasinoShopItemData Item;
    TestTrue(TEXT("Sale ID resolves"), Shop->GetShopItem(TEXT("42"), Item));
    TestEqual(TEXT("Inventory ID is preserved"), Item.InventoryItemID, 42);
    TestFalse(TEXT("Unsold table item rejected"), Shop->GetShopItem(TEXT("99"), Item));
    TestEqual(TEXT("Price comes from common definition"), Shop->GetItemTotalPrice(TEXT("42"), 2), 200);
    Shop->SetCurrentDay(2);
    TestEqual(TEXT("Day scaling preserved"), Shop->GetItemUnitPrice(TEXT("42")), 125);
    TestEqual(TEXT("Overflow price rejected"), Shop->GetItemTotalPrice(TEXT("42"), MAX_int32), 0);

    Row.UniqueID = 42;
    Row.Price = 200;
    Table->AddRow(TEXT("Product"), Row);
    TestEqual(TEXT("Definition change does not use stale cache"), Shop->GetItemUnitPrice(TEXT("42")), 250);
    Table->AddRow(TEXT("AmbiguousID"), Row);
    TestFalse(TEXT("Ambiguous shared ID rejected"), Shop->GetShopItem(TEXT("42"), Item));
    Table->RemoveRow(TEXT("AmbiguousID"));
    IDs.Reset();
    TestFalse(TEXT("Removed sale item rejected immediately"), Shop->GetShopItem(TEXT("42"), Item));
    return true;
}

#endif
