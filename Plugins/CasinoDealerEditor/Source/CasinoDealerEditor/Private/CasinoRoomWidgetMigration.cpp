#include "CasinoRoomWidgetMigration.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FeedbackContext.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "UI/CasinoRoomDialogWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace
{
const TCHAR* MainMenuPath = TEXT("/Game/Blackjack/Level/WBP_MainMenu.WBP_MainMenu");
const TCHAR* RoomEntryPath = TEXT("/Game/Blackjack/Level/WBP_RoomEntry.WBP_RoomEntry");
const FName PasswordMarkerName(TEXT("CasinoRoomPasswordMarker"));

UEdGraph* EventGraph(UBlueprint* Blueprint)
{
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
        if (Graph && Graph->GetFName() == TEXT("EventGraph")) return Graph;
    return nullptr;
}

template<typename T>
T* AddNode(UEdGraph* Graph, int32 X, int32 Y)
{
    T* Node = NewObject<T>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->NodePosX = X; Node->NodePosY = Y;
    return Node;
}

UK2Node_CallFunction* AddCall(UEdGraph* Graph, UClass* Owner, FName Function, int32 X, int32 Y)
{
    UK2Node_CallFunction* Node = AddNode<UK2Node_CallFunction>(Graph, X, Y);
    Node->FunctionReference.SetExternalMember(Function, Owner);
    Node->AllocateDefaultPins();
    return Node;
}

bool Link(UEdGraphPin* A, UEdGraphPin* B, FString& Error)
{
    if (!A || !B || !A->GetOwningNode()->GetSchema()->TryCreateConnection(A, B))
    {
        Error = FString::Printf(TEXT("Room dialog pin connection failed: %s -> %s"),
            A ? *A->PinName.ToString() : TEXT("missing"), B ? *B->PinName.ToString() : TEXT("missing"));
        return false;
    }
    return true;
}

bool MoveLinks(UEdGraphPin* From, UEdGraphPin* To, FString& Error)
{
    if (!From || !To) { Error = TEXT("Room action has a missing execution pin."); return false; }
    const TArray<UEdGraphPin*> Others = From->LinkedTo;
    From->BreakAllPinLinks();
    for (UEdGraphPin* Other : Others) if (!Link(Other, To, Error)) return false;
    return true;
}

UEdGraphNode* SingleSuccessor(UEdGraphNode* Node)
{
    UEdGraphPin* Then = Node ? Node->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr;
    return Then && Then->LinkedTo.Num() == 1 ? Then->LinkedTo[0]->GetOwningNode() : nullptr;
}

UK2Node_ComponentBoundEvent* ButtonEvent(UEdGraph* Graph, FName Button)
{
    UK2Node_ComponentBoundEvent* Result = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (UK2Node_ComponentBoundEvent* Event = Cast<UK2Node_ComponentBoundEvent>(Node);
            Event && Event->ComponentPropertyName == Button && Event->DelegatePropertyName == TEXT("OnClicked"))
        {
            if (Result) return nullptr;
            Result = Event;
        }
    return Result;
}

UK2Node_CallFunction* FindCall(UEdGraph* Graph, FName Name)
{
    UK2Node_CallFunction* Result = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
            Call && Call->FunctionReference.GetMemberName() == Name)
        {
            if (Result) return nullptr;
            Result = Call;
        }
    return Result;
}

UEdGraphPin* RoomValue(UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes)
        if (UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node);
            Get && Get->VariableReference.GetMemberName() == TEXT("RoomInfo"))
            return Get->FindPin(TEXT("RoomInfo"));
    return nullptr;
}

UEdGraphPin* SelfValue(UEdGraph* Graph, int32 X, int32 Y)
{
    for (UEdGraphNode* Node : Graph->Nodes)
        if (UK2Node_Self* Self = Cast<UK2Node_Self>(Node)) return Self->FindPin(UEdGraphSchema_K2::PN_Self);
    UK2Node_Self* Self = AddNode<UK2Node_Self>(Graph, X, Y);
    Self->AllocateDefaultPins();
    return Self->FindPin(UEdGraphSchema_K2::PN_Self);
}

bool WireRoomAction(UWidgetBlueprint* Blueprint, FName ButtonName, FName OldFunction, FName DialogFunction, FString& Error)
{
    UEdGraph* Graph = EventGraph(Blueprint);
    UK2Node_ComponentBoundEvent* Event = Graph ? ButtonEvent(Graph, ButtonName) : nullptr;
    UK2Node_CallFunction* Action = Cast<UK2Node_CallFunction>(SingleSuccessor(Event));
    if (!Action) { Error = TEXT("Room button no longer has its inspected direct action: ") + ButtonName.ToString(); return false; }
    if (Action->FunctionReference.GetMemberName() == DialogFunction
        && Action->FunctionReference.GetMemberParentClass() == UCasinoRoomDialogWidget::StaticClass()) return true;
    if (Action->FunctionReference.GetMemberName() != OldFunction
        || Action->FunctionReference.GetMemberParentClass() != UCasinoOnlineSubsystem::StaticClass())
    { Error = TEXT("Room button action differs from the inspected online call: ") + ButtonName.ToString(); return false; }
    if (OldFunction == TEXT("CreateRoom"))
    {
        const UEdGraphPin* RoomName = Action->FindPin(TEXT("RoomName"));
        if (!RoomName || !RoomName->LinkedTo.IsEmpty())
        { Error = TEXT("Existing room title contains custom logic; refusing to replace it."); return false; }
    }
    UK2Node_CallFunction* Dialog = AddCall(Graph, UCasinoRoomDialogWidget::StaticClass(), DialogFunction,
        Action->NodePosX, Action->NodePosY);
    UK2Node_CallFunction* OwningPlayer = AddCall(Graph, UWidget::StaticClass(), TEXT("GetOwningPlayer"),
        Action->NodePosX - 220, Action->NodePosY + 180);
    if (!Link(OwningPlayer->FindPin(UEdGraphSchema_K2::PN_ReturnValue), Dialog->FindPin(TEXT("PlayerController")), Error)
        || !MoveLinks(Action->FindPin(UEdGraphSchema_K2::PN_Execute), Dialog->FindPin(UEdGraphSchema_K2::PN_Execute), Error)
        || !MoveLinks(Action->FindPin(UEdGraphSchema_K2::PN_Then), Dialog->FindPin(UEdGraphSchema_K2::PN_Then), Error)) return false;
    if (DialogFunction == TEXT("ShowJoinRoomDialog") && !Link(RoomValue(Graph), Dialog->FindPin(TEXT("Room")), Error)) return false;
    FBlueprintEditorUtils::RemoveNode(Blueprint, Action, true);
    return true;
}

bool AddPasswordMarker(UWidgetBlueprint* Blueprint, FString& Error)
{
    UTextBlock* RoomName = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("RoomNameText")));
    UHorizontalBox* Row = RoomName ? Cast<UHorizontalBox>(RoomName->GetParent()) : nullptr;
    if (!Row || !Blueprint->WidgetTree->FindWidget(TEXT("PlayerCountText")) || !Blueprint->WidgetTree->FindWidget(TEXT("JoinButton")))
    { Error = TEXT("Room entry name/count/join layout contract was not found."); return false; }
    UWidget* Existing = Blueprint->WidgetTree->FindWidget(PasswordMarkerName);
    UTextBlock* Marker = Cast<UTextBlock>(Existing);
    if (Existing && !Marker) { Error = TEXT("Room password marker name is occupied by a different widget type."); return false; }
    if (!Marker)
    {
        Marker = Blueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), PasswordMarkerName);
        Marker->bIsVariable = false;
        Blueprint->OnVariableAdded(PasswordMarkerName);
        UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Row->InsertChildAt(Row->GetChildIndex(RoomName) + 1, Marker));
        if (!Slot) { Error = TEXT("Could not insert the room password marker next to the room name."); return false; }
        Slot->SetPadding(FMargin(4.f, 4.f, 12.f, 4.f));
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        Marker->SetText(FText::FromString(TEXT("비밀번호")));
        FSlateFontInfo Font = RoomName->GetFont(); Font.Size = 14.f;
        Marker->SetFont(Font);
        Marker->SetColorAndOpacity(FSlateColor(FLinearColor(.76f, .49f, .15f, 1.f)));
        Marker->SetToolTipText(FText::FromString(TEXT("입장할 때 비밀번호가 필요한 방입니다.")));
        Marker->SetVisibility(ESlateVisibility::Collapsed);
    }
    else if (Marker->GetParent() != Row)
    { Error = TEXT("Existing room password marker is outside the room row."); return false; }
    return true;
}

bool AppendConstructMarkerRefresh(UWidgetBlueprint* Blueprint, FString& Error)
{
    UEdGraph* Graph = EventGraph(Blueprint);
    if (!Graph) { Error = TEXT("Room entry EventGraph is missing."); return false; }
    UK2Node_Event* Construct = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
            Event && !Cast<UK2Node_ComponentBoundEvent>(Event) && Event->EventReference.GetMemberName() == TEXT("Construct")) Construct = Event;
    UK2Node_CallFunction* First = Cast<UK2Node_CallFunction>(SingleSuccessor(Construct));
    UK2Node_CallFunction* Last = Cast<UK2Node_CallFunction>(SingleSuccessor(First));
    if (!First || !Last || First->FunctionReference.GetMemberName() != TEXT("SetText") || Last->FunctionReference.GetMemberName() != TEXT("SetText"))
    { Error = TEXT("Room entry Construct no longer has the inspected name/count setters."); return false; }
    if (UK2Node_CallFunction* Existing = FindCall(Graph, TEXT("RefreshRoomPasswordIndicator")))
    {
        if (SingleSuccessor(Last) == Existing) return true;
        Error = TEXT("Existing password marker refresh is not attached to Construct."); return false;
    }
    UK2Node_CallFunction* Refresh = AddCall(Graph, UCasinoRoomDialogWidget::StaticClass(), TEXT("RefreshRoomPasswordIndicator"),
        Last->NodePosX + 280, Last->NodePosY);
    return MoveLinks(Last->FindPin(UEdGraphSchema_K2::PN_Then), Refresh->FindPin(UEdGraphSchema_K2::PN_Then), Error)
        && Link(Last->FindPin(UEdGraphSchema_K2::PN_Then), Refresh->FindPin(UEdGraphSchema_K2::PN_Execute), Error)
        && Link(SelfValue(Graph, Refresh->NodePosX, Refresh->NodePosY + 180), Refresh->FindPin(TEXT("Entry")), Error)
        && Link(RoomValue(Graph), Refresh->FindPin(TEXT("Room")), Error);
}

bool AddMainMenuErrorRestore(UWidgetBlueprint* Blueprint, FString& Error)
{
    UEdGraph* Graph = EventGraph(Blueprint);
    if (!Graph) { Error = TEXT("Main menu EventGraph is missing."); return false; }
    UK2Node_CustomEvent* Changed = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node);
            Event && Event->CustomFunctionName == TEXT("HandleOnlineChanged")) Changed = Event;
    UK2Node_VariableSet* StatusSetter = Cast<UK2Node_VariableSet>(SingleSuccessor(Changed));
    UEdGraphPin* Target = StatusSetter ? StatusSetter->FindPin(UEdGraphSchema_K2::PN_Self) : nullptr;
    UK2Node_VariableGet* TargetGet = Target && Target->LinkedTo.Num() == 1 ? Cast<UK2Node_VariableGet>(Target->LinkedTo[0]->GetOwningNode()) : nullptr;
    if (!StatusSetter || StatusSetter->VariableReference.GetMemberName() != TEXT("Text")
        || !TargetGet || TargetGet->VariableReference.GetMemberName() != TEXT("StatusText"))
    { Error = TEXT("Main menu online status setter differs from the inspected graph."); return false; }
    if (UK2Node_CallFunction* Existing = FindCall(Graph, TEXT("RestoreMainMenuError")))
    {
        if (SingleSuccessor(StatusSetter) == Existing) return true;
        Error = TEXT("Existing menu error restoration is not attached after the status setter."); return false;
    }
    UK2Node_CallFunction* Restore = AddCall(Graph, UCasinoRoomDialogWidget::StaticClass(), TEXT("RestoreMainMenuError"),
        StatusSetter->NodePosX + 160, StatusSetter->NodePosY + 140);
    return MoveLinks(StatusSetter->FindPin(UEdGraphSchema_K2::PN_Then), Restore->FindPin(UEdGraphSchema_K2::PN_Then), Error)
        && Link(StatusSetter->FindPin(UEdGraphSchema_K2::PN_Then), Restore->FindPin(UEdGraphSchema_K2::PN_Execute), Error)
        && Link(SelfValue(Graph, Restore->NodePosX, Restore->NodePosY + 180), Restore->FindPin(TEXT("Menu")), Error);
}

bool ValidateAction(UWidgetBlueprint* Blueprint, FName Button, FName Function, bool bJoin, FString& Error)
{
    UEdGraph* Graph = EventGraph(Blueprint);
    UK2Node_CallFunction* Call = Graph ? Cast<UK2Node_CallFunction>(SingleSuccessor(ButtonEvent(Graph, Button))) : nullptr;
    const UEdGraphPin* Player = Call ? Call->FindPin(TEXT("PlayerController")) : nullptr;
    const UK2Node_CallFunction* Owner = Player && Player->LinkedTo.Num() == 1 ? Cast<UK2Node_CallFunction>(Player->LinkedTo[0]->GetOwningNode()) : nullptr;
    if (!Call || Call->FunctionReference.GetMemberName() != Function || Call->FunctionReference.GetMemberParentClass() != UCasinoRoomDialogWidget::StaticClass()
        || !Owner || Owner->FunctionReference.GetMemberName() != TEXT("GetOwningPlayer"))
    { Error = TEXT("Room action is not connected to the dialog with its owning player: ") + Button.ToString(); return false; }
    if (bJoin)
    {
        const UEdGraphPin* Room = Call->FindPin(TEXT("Room"));
        const UK2Node_VariableGet* Get = Room && Room->LinkedTo.Num() == 1 ? Cast<UK2Node_VariableGet>(Room->LinkedTo[0]->GetOwningNode()) : nullptr;
        if (!Get || Get->VariableReference.GetMemberName() != TEXT("RoomInfo"))
        { Error = TEXT("Join dialog does not receive the entire selected RoomInfo snapshot."); return false; }
    }
    return true;
}

bool RegisterWidgetGuids(UWidgetBlueprint* Blueprint, FString& Error)
{
    Blueprint->ForEachSourceWidget([Blueprint](UWidget* Widget)
    {
        if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())) Blueprint->OnVariableAdded(Widget->GetFName());
    });
    for (UWidgetAnimation* Animation : Blueprint->Animations)
        if (Animation && !Blueprint->WidgetVariableNameToGuidMap.Contains(Animation->GetFName())) Blueprint->OnVariableAdded(Animation->GetFName());
    TSet<FGuid> Seen;
    for (const TPair<FName, FGuid>& Guid : Blueprint->WidgetVariableNameToGuidMap)
    {
        if (!Guid.Value.IsValid() || Seen.Contains(Guid.Value))
        { Error = TEXT("Room widget has an invalid or duplicate GUID: ") + Guid.Key.ToString(); return false; }
        Seen.Add(Guid.Value);
    }
    return true;
}

struct FPreservedWidget
{
    UClass* Class = nullptr;
    bool bVariable = false;
    bool bEnabled = false;
    ESlateVisibility Visibility = ESlateVisibility::Visible;
};

struct FOriginalContract
{
    UClass* Parent = nullptr;
    TMap<FName, FPreservedWidget> Widgets;
    TMap<FGuid, UClass*> Nodes;
    TArray<FName> Variables;
    TArray<FName> Graphs;
};

FOriginalContract Capture(UWidgetBlueprint* Blueprint)
{
    FOriginalContract Result; Result.Parent = Blueprint->ParentClass;
    Blueprint->WidgetTree->ForEachWidget([&Result](UWidget* Widget)
    {
        Result.Widgets.Add(Widget->GetFName(), {Widget->GetClass(), Widget->bIsVariable != 0, Widget->GetIsEnabled(), Widget->GetVisibility()});
    });
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables) Result.Variables.Add(Variable.VarName);
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        Result.Graphs.Add(Graph->GetFName());
        for (UEdGraphNode* Node : Graph->Nodes) if (Node)
        {
            // These two known calls are the only existing nodes replaced by this migration.
            const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
            if (Call && Call->FunctionReference.GetMemberParentClass() == UCasinoOnlineSubsystem::StaticClass()
                && (Call->FunctionReference.GetMemberName() == TEXT("CreateRoom") || Call->FunctionReference.GetMemberName() == TEXT("JoinRoom"))) continue;
            Result.Nodes.Add(Node->NodeGuid, Node->GetClass());
        }
    }
    return Result;
}

bool Preserved(UWidgetBlueprint* Blueprint, const FOriginalContract& Original, FString& Error)
{
    if (Blueprint->ParentClass != Original.Parent) { Error = TEXT("Room migration unexpectedly changed a widget parent."); return false; }
    for (const TPair<FName, FPreservedWidget>& Item : Original.Widgets)
    {
        const UWidget* Widget = Blueprint->WidgetTree->FindWidget(Item.Key);
        if (!Widget || Widget->GetClass() != Item.Value.Class || Widget->bIsVariable != Item.Value.bVariable
            || Widget->GetIsEnabled() != Item.Value.bEnabled || Widget->GetVisibility() != Item.Value.Visibility)
        { Error = TEXT("Room migration changed an existing widget contract: ") + Item.Key.ToString(); return false; }
    }
    TArray<FName> Variables; for (const FBPVariableDescription& Variable : Blueprint->NewVariables) Variables.Add(Variable.VarName);
    if (Variables != Original.Variables) { Error = TEXT("Room migration changed existing Blueprint variables."); return false; }
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    TArray<FName> GraphNames; TMap<FGuid, UClass*> Nodes;
    for (UEdGraph* Graph : Graphs)
    {
        GraphNames.Add(Graph->GetFName());
        for (UEdGraphNode* Node : Graph->Nodes) if (Node) Nodes.Add(Node->NodeGuid, Node->GetClass());
    }
    if (GraphNames != Original.Graphs) { Error = TEXT("Room migration changed existing graph/function names."); return false; }
    for (const TPair<FGuid, UClass*>& Node : Original.Nodes)
        if (Nodes.FindRef(Node.Key) != Node.Value) { Error = TEXT("Room migration removed an unrelated graph node."); return false; }
    return true;
}

bool Backup(UWidgetBlueprint* Blueprint, FString& Error)
{
    const FString Source = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup/BeforeRoomDialogs");
    const FString Destination = Directory / FPaths::GetCleanFilename(Source);
    if (IFileManager::Get().FileExists(*Destination)) return true;
    if (!IFileManager::Get().MakeDirectory(*Directory, true)
        || IFileManager::Get().Copy(*Destination, *Source, false) != COPY_OK)
    { Error = TEXT("Could not back up room UI asset before editing: ") + Source; return false; }
    return true;
}

bool Save(UWidgetBlueprint* Blueprint, FString& Error)
{
    if ((Blueprint->GetPathName() != MainMenuPath && Blueprint->GetPathName() != RoomEntryPath) || Blueprint->Status != BS_UpToDate)
    { Error = TEXT("Refusing to save a room UI asset outside the two-package scope or without a clean compile."); return false; }
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args))
    { Error = TEXT("Could not save migrated room UI: ") + Filename; return false; }
    return true;
}
}

bool MigrateCasinoRoomWidgets(FString& OutError)
{
    OutError.Reset();
    const SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
    const int32 ErrorsBefore = GWarn ? GWarn->GetNumErrors() : 0;
    UWidgetBlueprint* Entry = LoadObject<UWidgetBlueprint>(nullptr, RoomEntryPath);
    UWidgetBlueprint* Menu = LoadObject<UWidgetBlueprint>(nullptr, MainMenuPath);
    if (!Entry || !Menu || !Entry->WidgetTree || !Menu->WidgetTree || !Entry->GeneratedClass || !Menu->GeneratedClass)
    { OutError = TEXT("Could not load the two shipped room menu widgets."); return false; }
    if (!Backup(Entry, OutError) || !Backup(Menu, OutError)) return false;
    const FOriginalContract EntryContract = Capture(Entry), MenuContract = Capture(Menu);
    Entry->Modify(); Entry->WidgetTree->Modify(); Menu->Modify(); Menu->WidgetTree->Modify();
    if (!WireRoomAction(Menu, TEXT("LobbyButton"), TEXT("CreateRoom"), TEXT("ShowCreateRoomDialog"), OutError)
        || !WireRoomAction(Entry, TEXT("JoinButton"), TEXT("JoinRoom"), TEXT("ShowJoinRoomDialog"), OutError)
        || !AddPasswordMarker(Entry, OutError) || !AppendConstructMarkerRefresh(Entry, OutError)
        || !AddMainMenuErrorRestore(Menu, OutError)) return false;
    for (UWidgetBlueprint* Blueprint : {Entry, Menu})
    {
        if (!RegisterWidgetGuids(Blueprint, OutError)
            || !Preserved(Blueprint, Blueprint == Entry ? EntryContract : MenuContract, OutError)) return false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        if (Results.NumErrors || Results.NumWarnings || Blueprint->Status != BS_UpToDate)
        { OutError = FString::Printf(TEXT("Room UI did not compile cleanly: %s (errors %d, warnings %d, status %d)."),
            *Blueprint->GetPathName(), Results.NumErrors, Results.NumWarnings, static_cast<int32>(Blueprint->Status)); return false; }
        if (!Preserved(Blueprint, Blueprint == Entry ? EntryContract : MenuContract, OutError)) return false;
    }
    if (!ValidateAction(Menu, TEXT("LobbyButton"), TEXT("ShowCreateRoomDialog"), false, OutError)
        || !ValidateAction(Entry, TEXT("JoinButton"), TEXT("ShowJoinRoomDialog"), true, OutError)) return false;
    if (FDebug::GetNumEnsureFailures() != EnsuresBefore || (GWarn && GWarn->GetNumErrors() > ErrorsBefore))
    { OutError = TEXT("Room migration logged an ensure or engine error; no UI packages were saved."); return false; }
    // Both widgets pass the complete contract/compile checks before the first save.
    return Save(Entry, OutError) && Save(Menu, OutError);
}
