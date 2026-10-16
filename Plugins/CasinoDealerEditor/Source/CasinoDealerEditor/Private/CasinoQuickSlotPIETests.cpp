#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Item/ItemData.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "casino_simulatorPlayerState.h"

namespace CasinoMatchLoadingPIE
{
bool QueueTravel(FAutomationTestBase* Test, int32 PlayerCount, bool bCheckResults,
    TSharedPtr<IAutomationLatentCommand> AfterTravel);
}

namespace CasinoQuickSlotTests
{
struct FItems
{
    UDataTable* Table = nullptr;
    int32 Basic[2] = {0, 1};
    int32 Selected[2] = {INDEX_NONE, INDEX_NONE};
};

bool LoadItems(FItems& Items, FAutomationTestBase& Test)
{
    Items.Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/3_DataTable/DT_ItemData.DT_ItemData"));
    if (!Test.TestNotNull(TEXT("Actual inventory item definitions"), Items.Table)
        || !Test.TestTrue(TEXT("Inventory table uses FItemData"), Items.Table->GetRowStruct() == FItemData::StaticStruct())) return false;
    const TCHAR* Categories[] = {TEXT("Item.Consumable.Cigarette"), TEXT("Item.Consumable.Alcohol")};
    for (int32 Slot = 0; Slot < 2; ++Slot)
    {
        const FItemData* Basic = nullptr;
        const FItemData* Selected = nullptr;
        for (const auto& Entry : Items.Table->GetRowMap())
        {
            const FItemData* Row = reinterpret_cast<const FItemData*>(Entry.Value);
            if (Row->UniqueID == Items.Basic[Slot]) Basic = Row;
            if (Row->UniqueID == Items.Basic[Slot] || !Row->bConsumeOnUse || !Row->OnUseEffect
                || !Row->ItemCategory.ToString().StartsWith(Categories[Slot])) continue;
            if (!Selected || Row->Price > Selected->Price
                || (Row->Price == Selected->Price && Row->EffectMagnitude > Selected->EffectMagnitude)
                || (Row->Price == Selected->Price && Row->EffectMagnitude == Selected->EffectMagnitude
                    && Row->UniqueID > Selected->UniqueID)) Selected = Row;
        }
        if (!Test.TestNotNull(TEXT("Default quick-slot item exists"), Basic)
            || !Test.TestTrue(TEXT("Default quick-slot item is usable"), Basic->bConsumeOnUse && Basic->OnUseEffect != nullptr)
            || !Test.TestNotNull(TEXT("Actual table has a different premium consumable"), Selected)) return false;
        Items.Selected[Slot] = Selected->UniqueID;
    }
    return true;
}

bool UseSlot(Acasino_simulatorCharacter* Character, int32 Slot, FAutomationTestBase& Test)
{
    UFunction* Function = Character ? Character->FindFunction(TEXT("ServerUseNumberSlotItem")) : nullptr;
    if (!Test.TestNotNull(TEXT("Production quick-use server RPC"), Function)) return false;
    struct FParameters { int32 SlotIndex; } Parameters{Slot};
    Character->ProcessEvent(Function, &Parameters);
    return true;
}

bool DropPremiumItem(Acasino_simulatorPlayerController* Owner, const FItems& Items, int32 Slot, FAutomationTestBase& Test)
{
    UClass* SlotClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/1_BluePrint/Widget/WBP_ItemSlot.WBP_ItemSlot_C"));
    UClass* DragClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/1_BluePrint/Widget/WBP_Drag.WBP_Drag_C"));
    if (!Test.TestNotNull(TEXT("Actual quick-slot Blueprint"), SlotClass)
        || !Test.TestNotNull(TEXT("Actual inventory drag visual"), DragClass)) return false;
    TStrongObjectPtr<UUserWidget> Widget(CreateWidget<UUserWidget>(Owner, SlotClass));
    TStrongObjectPtr<UUserWidget> DragWidget(CreateWidget<UUserWidget>(Owner, DragClass));
    TStrongObjectPtr<UDragDropOperation> Operation(NewObject<UDragDropOperation>());
    if (!Test.TestNotNull(TEXT("Owning client's actual quick-slot instance"), Widget.Get())
        || !Test.TestNotNull(TEXT("Owning client's actual drag visual instance"), DragWidget.Get())) return false;
    // WBP_ItemSlot.OnDrop reads Operation.DefaultDragVisual and casts that
    // widget to WBP_Drag before reading DragItemData.
    Operation->DefaultDragVisual = DragWidget.Get();
    FBoolProperty* CanDrop = FindFProperty<FBoolProperty>(SlotClass, TEXT("CanDrop"));
    FIntProperty* SlotNum = FindFProperty<FIntProperty>(SlotClass, TEXT("SlotNum"));
    FStructProperty* ItemData = FindFProperty<FStructProperty>(SlotClass, TEXT("Item Data"));
    FStructProperty* DragData = FindFProperty<FStructProperty>(DragClass, TEXT("DragItemData"));
    UFunction* OnDrop = Widget->FindFunction(TEXT("OnDrop"));
    if (!Test.TestNotNull(TEXT("Actual quick-slot drop permission"), CanDrop)
        || !Test.TestNotNull(TEXT("Actual quick-slot index"), SlotNum)
        || !Test.TestNotNull(TEXT("Actual quick-slot displayed item"), ItemData)
        || !Test.TestNotNull(TEXT("Actual drag-operation item payload"), DragData)
        || !Test.TestNotNull(TEXT("Actual Blueprint OnDrop handler"), OnDrop)) return false;
    if (!Test.TestTrue(TEXT("Quick-slot and drag payload use FItemData"),
        ItemData->Struct == FItemData::StaticStruct() && DragData->Struct == FItemData::StaticStruct())) return false;
    const FItemData* Basic = nullptr;
    const FItemData* Premium = nullptr;
    for (const auto& Entry : Items.Table->GetRowMap())
    {
        const FItemData* Row = reinterpret_cast<const FItemData*>(Entry.Value);
        if (Row->UniqueID == Items.Basic[Slot]) Basic = Row;
        if (Row->UniqueID == Items.Selected[Slot]) Premium = Row;
    }
    if (!Test.TestNotNull(TEXT("Displayed baseline definition"), Basic)
        || !Test.TestNotNull(TEXT("Dropped premium definition"), Premium)) return false;
    CanDrop->SetPropertyValue_InContainer(Widget.Get(), true);
    SlotNum->SetPropertyValue_InContainer(Widget.Get(), Slot);
    ItemData->CopyCompleteValue(ItemData->ContainerPtrToValuePtr<void>(Widget.Get()), Basic);
    DragData->CopyCompleteValue(DragData->ContainerPtrToValuePtr<void>(DragWidget.Get()), Premium);
    FObjectPropertyBase* OperationParameter = FindFProperty<FObjectPropertyBase>(OnDrop, TEXT("Operation"));
    FBoolProperty* Handled = FindFProperty<FBoolProperty>(OnDrop, TEXT("ReturnValue"));
    if (!Test.TestNotNull(TEXT("Blueprint OnDrop operation parameter"), OperationParameter)
        || !Test.TestNotNull(TEXT("Blueprint OnDrop handled return"), Handled)) return false;
    // Let reflection initialize Geometry/PointerEvent correctly instead of
    // reproducing a Blueprint parameter struct's alignment by hand.
    FStructOnScope Parameters(OnDrop);
    OperationParameter->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Operation.Get());
    Widget->ProcessEvent(OnDrop, Parameters.GetStructMemory());
    const bool bHandled = Handled->GetPropertyValue_InContainer(Parameters.GetStructMemory());
    Test.TestTrue(TEXT("Actual quick-slot OnDrop handles the premium item"), bHandled);
    Test.TestEqual(TEXT("Drop does not optimistically replace displayed item before server confirmation"),
        ItemData->ContainerPtrToValuePtr<FItemData>(Widget.Get())->UniqueID, Items.Basic[Slot]);
    return bHandled;
}

Acasino_simulatorPlayerController* LocalController(UWorld* World)
{
    if (!World) return nullptr;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()); PC && PC->IsLocalController()) return PC;
    return nullptr;
}

Acasino_simulatorPlayerState* StateWithId(UWorld* World, int32 PlayerId)
{
    if (!World || !World->GetGameState()) return nullptr;
    for (APlayerState* State : World->GetGameState()->PlayerArray)
        if (State && State->GetPlayerId() == PlayerId) return Cast<Acasino_simulatorPlayerState>(State);
    return nullptr;
}

UUserWidget* FindQuickSlot(UUserWidget* Root, int32 Slot)
{
    if (!Root) return nullptr;
    FIntProperty* Index = FindFProperty<FIntProperty>(Root->GetClass(), TEXT("SlotNum"));
    if (Index && Index->GetPropertyValue_InContainer(Root) == Slot
        && FindFProperty<FStructProperty>(Root->GetClass(), TEXT("Item Data"))) return Root;
    UUserWidget* Found = nullptr;
    if (Root->WidgetTree)
        Root->WidgetTree->ForEachWidget([&](UWidget* Child)
        {
            if (!Found)
                if (UUserWidget* Nested = Cast<UUserWidget>(Child)) Found = FindQuickSlot(Nested, Slot);
        });
    return Found;
}

UUserWidget* ConfirmedHudSlot(Acasino_simulatorPlayerController* Owner, int32 Slot)
{
    FObjectPropertyBase* HUDProperty = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), TEXT("PlayerHUDWidget"));
    UUserWidget* HUD = HUDProperty ? Cast<UUserWidget>(HUDProperty->GetObjectPropertyValue_InContainer(Owner)) : nullptr;
    return FindQuickSlot(HUD, Slot);
}

class FReplication final : public IAutomationLatentCommand
{
public:
    explicit FReplication(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if (Started == 0.0) Started = Now;
        if (Now - Started > 30.0)
        {
            Test->AddError(FString::Printf(TEXT("Three-player quick-slot selection/use timed out at stage %d."), Stage));
            return true;
        }
        if (!Server.IsValid() || !OwnerWorld.IsValid() || !ObserverWorld.IsValid())
        {
            TArray<UWorld*> Clients;
            for (const auto& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
                if (Context.World()->GetNetMode() == NM_ListenServer) Server = Context.World();
                if (Context.World()->GetNetMode() == NM_Client) Clients.Add(Context.World());
            }
            if (!Server.IsValid() || Clients.Num() != 2) return false;
            OwnerWorld = Clients[0];
            ObserverWorld = Clients[1];
        }
        auto* OwnerPC = LocalController(OwnerWorld.Get());
        auto* ObserverPC = LocalController(ObserverWorld.Get());
        auto* OwnerState = OwnerPC ? OwnerPC->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
        auto* OwnerPawn = OwnerPC ? Cast<Acasino_simulatorCharacter>(OwnerPC->GetPawn()) : nullptr;
        auto* ObserverState = ObserverPC ? ObserverPC->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
        if (!OwnerState || !OwnerPawn || !ObserverState) return false;
        auto* AuthorityState = StateWithId(Server.Get(), OwnerState->GetPlayerId());
        auto* ObserverMirror = StateWithId(ObserverWorld.Get(), OwnerState->GetPlayerId());
        if (!AuthorityState || !ObserverMirror) return false;
        if (Stage == 0)
        {
            if (!LoadItems(Items, *Test)) return true;
            Test->TestEqual(TEXT("Real quick-slot regression uses one host and two remote clients"),
                Server->GetNumPlayerControllers(), 3);
            AuthorityState->ItemDataTable = Items.Table;
            for (int32 Slot = 0; Slot < 2; ++Slot)
            {
                for (int32 ItemId : {Items.Basic[Slot], Items.Selected[Slot]})
                {
                    AuthorityState->RemoveItem(ItemId, AuthorityState->GetItemQuantity(ItemId));
                    AuthorityState->AddItem(ItemId, 3);
                }
                if (!Test->TestTrue(TEXT("Server initializes an owned baseline quick-slot item"),
                    AuthorityState->SetNumberSlotAuthoritative(Slot, Items.Basic[Slot]))) return true;
            }
            ObserverSlots = ObserverState->NumberSlots;
            MirrorSlots = ObserverMirror->NumberSlots;
            Stage = 1;
            return false;
        }
        if (Stage == 1)
        {
            for (int32 Slot = 0; Slot < 2; ++Slot)
                if (OwnerState->GetItemQuantity(Items.Basic[Slot]) != 3
                    || OwnerState->GetItemQuantity(Items.Selected[Slot]) != 3
                    || !OwnerState->NumberSlots.IsValidIndex(Slot)
                    || OwnerState->NumberSlots[Slot] != Items.Basic[Slot]) return false;
            Test->TestFalse(TEXT("Owning client cannot directly write authoritative quick slots"),
                OwnerState->SetNumberSlotAuthoritative(0, Items.Selected[0]));
            if (!DropPremiumItem(OwnerPC, Items, 0, *Test)) return true;
            // Selection and immediate use use reliable RPCs on the same character.
            // No optimistic local array write may hide a missing server selection.
            Test->TestEqual(TEXT("Client selection waits for server confirmation"),
                OwnerState->NumberSlots[0], Items.Basic[0]);
            if (!UseSlot(OwnerPawn, 0, *Test)) return true;
            Stage = 2;
            return false;
        }
        if (Stage == 2 || Stage == 3)
        {
            const int32 Slot = Stage - 2;
            if (AuthorityState->GetItemQuantity(Items.Basic[Slot]) != 3)
            {
                Test->AddError(TEXT("Immediate quick-use consumed the default item instead of the selected premium item."));
                return true;
            }
            if (AuthorityState->NumberSlots[Slot] != Items.Selected[Slot]
                || AuthorityState->GetItemQuantity(Items.Selected[Slot]) != 2
                || OwnerState->NumberSlots[Slot] != Items.Selected[Slot]
                || OwnerState->GetItemQuantity(Items.Selected[Slot]) != 2) return false;
            UUserWidget* HudSlot = ConfirmedHudSlot(OwnerPC, Slot);
            FStructProperty* DisplayData = HudSlot ? FindFProperty<FStructProperty>(HudSlot->GetClass(), TEXT("Item Data")) : nullptr;
            if (!DisplayData || DisplayData->Struct != FItemData::StaticStruct()
                || DisplayData->ContainerPtrToValuePtr<FItemData>(HudSlot)->UniqueID != Items.Selected[Slot]) return false;
            UTextBlock* Count = HudSlot->WidgetTree ? Cast<UTextBlock>(HudSlot->WidgetTree->FindWidget(TEXT("Text_Count"))) : nullptr;
            if (Count && Count->GetText().ToString() != TEXT("2")) return false;
            Test->TestEqual(TEXT("Selected consumable is consumed on the authority"),
                AuthorityState->GetItemQuantity(Items.Selected[Slot]), 2);
            Test->TestEqual(TEXT("Owning client receives the confirmed selected item"),
                OwnerState->NumberSlots[Slot], Items.Selected[Slot]);
            Test->TestEqual(TEXT("Default consumable remains untouched"),
                AuthorityState->GetItemQuantity(Items.Basic[Slot]), 3);
            Test->TestEqual(TEXT("Actual HUD quick-slot display follows server-confirmed selection"),
                DisplayData->ContainerPtrToValuePtr<FItemData>(HudSlot)->UniqueID, Items.Selected[Slot]);
            if (Count) Test->TestEqual(TEXT("Actual HUD quick-slot count follows consumed quantity"), Count->GetText().ToString(), FString(TEXT("2")));
            if (Stage == 2)
            {
                if (!DropPremiumItem(OwnerPC, Items, 1, *Test)) return true;
                if (!UseSlot(OwnerPawn, 1, *Test)) return true;
                Stage = 3;
            }
            else { Stage = 4; FinalSeen = Now; }
            return false;
        }
        if (Now - FinalSeen < .5) return false;
        Test->TestTrue(TEXT("Another client cannot see the owner's private slot selections"),
            ObserverMirror->NumberSlots == MirrorSlots);
        Test->TestTrue(TEXT("Another player's own slots are not changed"),
            ObserverState->NumberSlots == ObserverSlots);
        Test->TestEqual(TEXT("Owner confirms selected premium cigarette"),
            OwnerState->NumberSlots[0], Items.Selected[0]);
        Test->TestEqual(TEXT("Owner confirms selected premium alcohol"),
            OwnerState->NumberSlots[1], Items.Selected[1]);
        return true;
    }
private:
    FAutomationTestBase* Test;
    FItems Items;
    TWeakObjectPtr<UWorld> Server, OwnerWorld, ObserverWorld;
    TArray<int32> ObserverSlots, MirrorSlots;
    double Started = 0.0, FinalSeen = 0.0;
    int32 Stage = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoQuickSlotSelectionTest, "Casino.Inventory.QuickSlotSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoQuickSlotSelectionTest::RunTest(const FString& Parameters)
{
    using namespace CasinoQuickSlotTests;
    FItems Items;
    if (!LoadItems(Items, *this)) return false;
    UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Concrete production character class"), CharacterClass)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* State = World->SpawnActor<Acasino_simulatorPlayerState>();
    auto* Character = World->SpawnActor<Acasino_simulatorCharacter>(CharacterClass);
    auto* StateProperty = FindFProperty<FObjectPropertyBase>(APawn::StaticClass(), TEXT("PlayerState"));
    if (!TestNotNull(TEXT("Authority quick-slot state"), State)
        || !TestNotNull(TEXT("Authority quick-use character"), Character)
        || !TestNotNull(TEXT("Pawn state binding"), StateProperty)) return false;
    StateProperty->SetObjectPropertyValue_InContainer(Character, State);
    UDataTable* Table = DuplicateObject<UDataTable>(Items.Table, GetTransientPackage());
    State->ItemDataTable = Table;
    for (int32 Slot = 0; Slot < 2; ++Slot)
    {
        State->AddItem(Items.Basic[Slot], 3);
        TestFalse(TEXT("Unowned premium item cannot be assigned"), State->SetNumberSlotAuthoritative(Slot, Items.Selected[Slot]));
        State->AddItem(Items.Selected[Slot], 3);
    }
    const TArray<int32> OriginalSlots = State->NumberSlots;
    TestFalse(TEXT("Negative quick-slot index rejected"), State->SetNumberSlotAuthoritative(-1, Items.Selected[0]));
    TestFalse(TEXT("Third quick-slot index rejected"), State->SetNumberSlotAuthoritative(2, Items.Selected[0]));
    TestTrue(TEXT("Invalid indexes cannot resize or change slot selection"), State->NumberSlots == OriginalSlots);
    FItemData Premium;
    if (!TestTrue(TEXT("Selected premium definition resolves"), State->FindItemData(Items.Selected[0], Premium))) return false;
    int32 InvalidId = 1000000;
    FItemData Existing;
    while (State->FindItemData(InvalidId, Existing)) ++InvalidId;
    State->AddItem(InvalidId, 1);
    TestFalse(TEXT("Owned ID missing from definitions rejected"), State->SetNumberSlotAuthoritative(0, InvalidId));
    FItemData Invalid = Premium;
    Invalid.UniqueID = ++InvalidId;
    Invalid.bConsumeOnUse = false;
    Table->AddRow(TEXT("QuickSlotTest_NonConsumable"), Invalid);
    State->AddItem(Invalid.UniqueID, 1);
    TestFalse(TEXT("Non-consumable item rejected"), State->SetNumberSlotAuthoritative(0, Invalid.UniqueID));
    Invalid.UniqueID = ++InvalidId;
    Invalid.bConsumeOnUse = true;
    Invalid.OnUseEffect = nullptr;
    Table->AddRow(TEXT("QuickSlotTest_NoEffect"), Invalid);
    State->AddItem(Invalid.UniqueID, 1);
    TestFalse(TEXT("Consumable without effect rejected"), State->SetNumberSlotAuthoritative(0, Invalid.UniqueID));

    UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Character, Character);
    ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetMaxNicotineAttribute(), 100000.f);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetMaxAlcoholAttribute(), 100000.f);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetNicotineAttribute(), 0.f);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetAlcoholAttribute(), 0.f);
    // This isolated world deliberately never begins play. AActor::ProcessEvent
    // otherwise discards reflected calls before AreActorsInitialized(), so permit
    // local editor script execution only for this authority fixture. The real PIE
    // test retains normal client/server RPC callspace and ordering.
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    for (int32 Slot = 0; Slot < 2; ++Slot)
    {
        Character->RequestSetNumberSlot(Slot, Items.Selected[Slot]);
        TestEqual(TEXT("Character request selects the actual premium definition"), State->NumberSlots[Slot], Items.Selected[Slot]);
        if (!UseSlot(Character, Slot, *this)) return false;
        TestEqual(TEXT("Selected premium item loses one copy"), State->GetItemQuantity(Items.Selected[Slot]), 2);
        TestEqual(TEXT("Default quick-slot item is not consumed"), State->GetItemQuantity(Items.Basic[Slot]), 3);
    }
    TestTrue(TEXT("Selected cigarette applies its production nicotine effect"),
        ASC->GetNumericAttribute(Ucasino_simulatorAttributeSet::GetNicotineAttribute()) > 0.f);
    TestTrue(TEXT("Selected alcohol applies its production alcohol effect"),
        ASC->GetNumericAttribute(Ucasino_simulatorAttributeSet::GetAlcoholAttribute()) > 0.f);
    for (int32 Count = 0; Count < 3; ++Count)
        if (!UseSlot(Character, 0, *this)) return false;
    TestEqual(TEXT("Empty selected stack cannot spend below zero"), State->GetItemQuantity(Items.Selected[0]), 0);
    TestEqual(TEXT("Empty premium stack never falls back to default item"), State->GetItemQuantity(Items.Basic[0]), 3);
    State->AddItem(Items.Selected[0], 1);
    TestTrue(TEXT("Baseline selection can be restored"), State->SetNumberSlotAuthoritative(0, Items.Basic[0]));
    Table->AddRow(TEXT("QuickSlotTest_AmbiguousId"), Premium);
    TestFalse(TEXT("Duplicate item definitions cannot select an ambiguous ID"), State->SetNumberSlotAuthoritative(0, Items.Selected[0]));
    TestEqual(TEXT("Rejected ambiguous selection retains previous item"), State->NumberSlots[0], Items.Basic[0]);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoQuickSlotsNetworkPIETest, "Casino.Inventory.QuickSlots.NetworkPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoQuickSlotsNetworkPIETest::RunTest(const FString& Parameters)
{
    return CasinoMatchLoadingPIE::QueueTravel(this, 3, false, MakeShared<CasinoQuickSlotTests::FReplication>(this));
}

#endif
