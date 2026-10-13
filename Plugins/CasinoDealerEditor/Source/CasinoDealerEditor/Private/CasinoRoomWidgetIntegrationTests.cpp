#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_VariableGet.h"
#include "Misc/AutomationTest.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "UI/CasinoRoomDialogWidget.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace
{
UK2Node_CallFunction* RoomButtonAction(UWidgetBlueprint* Blueprint, FName Button)
{
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (const UK2Node_ComponentBoundEvent* Event = Cast<UK2Node_ComponentBoundEvent>(Node);
                Event && Event->ComponentPropertyName == Button && Event->DelegatePropertyName == TEXT("OnClicked"))
            {
                const UEdGraphPin* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then);
                return Then && Then->LinkedTo.Num() == 1 ? Cast<UK2Node_CallFunction>(Then->LinkedTo[0]->GetOwningNode()) : nullptr;
            }
    return nullptr;
}

bool AssignRoom(UUserWidget* Entry, const FCasinoRoomInfo& Room)
{
    FStructProperty* Property = FindFProperty<FStructProperty>(Entry->GetClass(), TEXT("RoomInfo"));
    if (!Property || Property->Struct != FCasinoRoomInfo::StaticStruct()) return false;
    Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Entry), &Room);
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoRoomWidgetIntegrationTest,
    "Casino.UI.RoomDialogs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoRoomWidgetIntegrationTest::RunTest(const FString& Parameters)
{
    UWidgetBlueprint* MenuBP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Blackjack/Level/WBP_MainMenu.WBP_MainMenu"));
    UWidgetBlueprint* EntryBP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Blackjack/Level/WBP_RoomEntry.WBP_RoomEntry"));
    if (!TestNotNull(TEXT("Shipped main menu"), MenuBP) || !TestNotNull(TEXT("Shipped room entry"), EntryBP)
        || !TestNotNull(TEXT("Main menu generated class"), MenuBP->GeneratedClass.Get())
        || !TestNotNull(TEXT("Room entry generated class"), EntryBP->GeneratedClass.Get())) return false;
    TestTrue(TEXT("Main menu is compiled without warnings"), MenuBP->Status == BS_UpToDate);
    TestTrue(TEXT("Room entry is compiled without warnings"), EntryBP->Status == BS_UpToDate);
    UK2Node_CallFunction* Create = RoomButtonAction(MenuBP, TEXT("LobbyButton"));
    UK2Node_CallFunction* Join = RoomButtonAction(EntryBP, TEXT("JoinButton"));
    if (!TestNotNull(TEXT("Existing create button action"), Create) || !TestNotNull(TEXT("Existing join button action"), Join)) return false;
    TestEqual(TEXT("Create button opens the title/password dialog"), Create->FunctionReference.GetMemberName(), FName(TEXT("ShowCreateRoomDialog")));
    TestEqual(TEXT("Join button goes through protected/public room routing"), Join->FunctionReference.GetMemberName(), FName(TEXT("ShowJoinRoomDialog")));
    for (UK2Node_CallFunction* Action : {Create, Join})
    {
        TestTrue(TEXT("Room action calls the native dialog class"), Action->FunctionReference.GetMemberParentClass() == UCasinoRoomDialogWidget::StaticClass());
        const UEdGraphPin* Player = Action->FindPin(TEXT("PlayerController"));
        UK2Node_CallFunction* Owner = Player && Player->LinkedTo.Num() == 1 ? Cast<UK2Node_CallFunction>(Player->LinkedTo[0]->GetOwningNode()) : nullptr;
        TestTrue(TEXT("Room dialog uses the widget's owning player"), Owner && Owner->FunctionReference.GetMemberName() == TEXT("GetOwningPlayer"));
        const UEdGraphPin* DialogClass = Action->FindPin(TEXT("DialogClass"));
        TestTrue(TEXT("Optional dialog class uses the native fallback"), DialogClass && !DialogClass->DefaultObject && DialogClass->LinkedTo.IsEmpty());
    }
    const UEdGraphPin* RoomPin = Join->FindPin(TEXT("Room"));
    UK2Node_VariableGet* RoomGet = RoomPin && RoomPin->LinkedTo.Num() == 1 ? Cast<UK2Node_VariableGet>(RoomPin->LinkedTo[0]->GetOwningNode()) : nullptr;
    TestTrue(TEXT("Join preserves the selected room's stable ID and search snapshot"), RoomGet && RoomGet->VariableReference.GetMemberName() == TEXT("RoomInfo"));
    for (UWidgetBlueprint* Blueprint : {MenuBP, EntryBP})
    {
        TSet<FGuid> Seen;
        Blueprint->ForEachSourceWidget([&](UWidget* Widget)
        {
            const FGuid* Guid = Blueprint->WidgetVariableNameToGuidMap.Find(Widget->GetFName());
            TestTrue(TEXT("Every source widget has a unique valid GUID"), Guid && Guid->IsValid() && !Seen.Contains(*Guid));
            if (Guid) Seen.Add(*Guid);
        });
    }

    // Exercise the actual shipped Construct graph without a viewport or an online service.
    UUserWidget* Entry = NewObject<UUserWidget>(GetTransientPackage(), EntryBP->GeneratedClass.Get());
    if (!TestTrue(TEXT("Shipped room widget initializes"), Entry->Initialize())) return false;
    UTextBlock* Name = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("RoomNameText")));
    UTextBlock* Count = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("PlayerCountText")));
    UTextBlock* Marker = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("CasinoRoomPasswordMarker")));
    UButton* JoinButton = Cast<UButton>(Entry->GetWidgetFromName(TEXT("JoinButton")));
    if (!TestNotNull(TEXT("Original room name text survives"), Name) || !TestNotNull(TEXT("Original player count survives"), Count)
        || !TestNotNull(TEXT("Designer password marker"), Marker) || !TestNotNull(TEXT("Original join button survives"), JoinButton)) return false;
    TestTrue(TEXT("Original join delegate remains bound"), JoinButton->OnClicked.IsBound());
    FCasinoRoomInfo Room;
    Room.SearchIndex = 3; Room.SessionId = TEXT("integration-room-id");
    Room.RoomName = TEXT("제목 유지 테스트"); Room.Players = 2; Room.Capacity = 4;
    if (!TestTrue(TEXT("RoomInfo can still be assigned as a complete struct"), AssignRoom(Entry, Room))) return false;
    Entry->Construct();
    TestEqual(TEXT("Public room title remains exact"), Name->GetText().ToString(), Room.RoomName);
    const FString CountBefore = Count->GetText().ToString();
    TestTrue(TEXT("Existing room count includes occupied and total slots"), CountBefore.Contains(TEXT("2")) && CountBefore.Contains(TEXT("4")));
    TestEqual(TEXT("Public rooms have no password badge"), Marker->GetVisibility(), ESlateVisibility::Collapsed);
    Room.bPasswordProtected = true;
    AssignRoom(Entry, Room); Entry->Construct();
    TestTrue(TEXT("Protected room badge is visible"), Marker->GetVisibility() != ESlateVisibility::Collapsed && Marker->GetVisibility() != ESlateVisibility::Hidden);
    TestEqual(TEXT("Password badge does not alter room title"), Name->GetText().ToString(), Room.RoomName);
    TestEqual(TEXT("Password badge does not alter player count"), Count->GetText().ToString(), CountBefore);
    TestTrue(TEXT("Password badge shares the room row"), Marker->GetParent() == Name->GetParent());
    Room.bPasswordProtected = false;
    AssignRoom(Entry, Room); Entry->Construct();
    TestEqual(TEXT("Refreshing back to public hides the badge"), Marker->GetVisibility(), ESlateVisibility::Collapsed);

    // Verify the real native fallback fields/mode while leaving network submission to multiplayer testing.
    UCasinoRoomDialogWidget* Dialog = NewObject<UCasinoRoomDialogWidget>();
    Dialog->Initialize(); Dialog->EnsureLayout(); Dialog->ConfigureCreate();
    TestEqual(TEXT("Create mode"), Dialog->GetDialogMode(), ECasinoRoomDialogMode::Create);
    if (!TestNotNull(TEXT("Fallback room title input"), Dialog->GetRoomNameInput())
        || !TestNotNull(TEXT("Fallback password input"), Dialog->GetPasswordInput())
        || !TestNotNull(TEXT("Fallback inline error text"), Dialog->GetErrorText())) return false;
    TestTrue(TEXT("Password characters are masked"), Dialog->GetPasswordInput()->GetIsPassword());
    Room.bPasswordProtected = true;
    Dialog->ConfigureJoin(Room);
    TestEqual(TEXT("Join mode"), Dialog->GetDialogMode(), ECasinoRoomDialogMode::Join);
    TestFalse(TEXT("Opening a password dialog does not submit a network request"), Dialog->IsSubmitting());
    return true;
}

#endif
