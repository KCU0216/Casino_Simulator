#include "CasinoQuickSlotMigration.h"

#include "Blueprint/WidgetTree.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "HAL/FileManager.h"
#include "Item/ItemData.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace CasinoQuickSlotMigration
{
template<typename T> T* Node(UEdGraph* Graph, FName Name)
{
    for (UEdGraphNode* Existing : Graph->Nodes)
        if (Existing && Existing->GetFName() == Name) return Cast<T>(Existing);
    return nullptr;
}
template<typename T> T* Add(UEdGraph* Graph, int32 X, int32 Y)
{
    T* Added = NewObject<T>(Graph);
    Graph->AddNode(Added, false, false);
    Added->CreateNewGuid(); Added->NodePosX = X; Added->NodePosY = Y;
    return Added;
}
bool Link(UEdGraphPin* A, UEdGraphPin* B, FString& Error)
{
    if (A && B && A->GetOwningNode()->GetSchema()->TryCreateConnection(A, B)) return true;
    Error = TEXT("Quick-slot migration could not connect the inspected graph pins.");
    return false;
}
bool IsLink(UEdGraphNode* A, FName From, UEdGraphNode* B, FName To)
{
    UEdGraphPin* Pin = A ? A->FindPin(From) : nullptr;
    return Pin && Pin->LinkedTo.Num() == 1 && B && Pin->LinkedTo[0] == B->FindPin(To);
}
TMap<FName, FString> OtherGraphs(UWidgetBlueprint* Blueprint, UEdGraph* Excluded)
{
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    TMap<FName, FString> Exports;
    for (UEdGraph* Graph : Graphs) if (Graph && Graph != Excluded)
    {
        TSet<UObject*> Nodes; for (UEdGraphNode* Item : Graph->Nodes) if (Item) Nodes.Add(Item);
        FString Export; FEdGraphUtilities::ExportNodesToText(Nodes, Export);
        Exports.Add(Graph->GetFName(), Export);
    }
    return Exports;
}
}

bool MigrateCasinoQuickSlots(FString& OutError)
{
    using namespace CasinoQuickSlotMigration;
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/1_BluePrint/Widget/WBP_ItemSlot.WBP_ItemSlot"));
    UEdGraph* Graph = nullptr;
    if (Blueprint) for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
        if (Candidate && Candidate->GetFName() == TEXT("OnDrop")) Graph = Candidate;
    if (!Graph || !Blueprint->WidgetTree)
    { OutError = TEXT("The inspected item-slot OnDrop graph is missing."); return false; }
    int32 Requests = 0, SlotWrites = 0;
    for (UEdGraphNode* Existing : Graph->Nodes)
        if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Existing))
        {
            if (Call->FunctionReference.GetMemberName() == TEXT("RequestSetNumberSlot")) ++Requests;
            if (Call->FunctionReference.GetMemberName() == TEXT("Array_Set")) ++SlotWrites;
        }
    if (Requests == 1 && SlotWrites == 0) return true;
    auto* DragCast = Node<UK2Node_DynamicCast>(Graph, TEXT("K2Node_DynamicCast_0"));
    auto* ItemSet = Node<UK2Node_VariableSet>(Graph, TEXT("K2Node_VariableSet_0"));
    auto* Draw = Node<UK2Node_CallFunction>(Graph, TEXT("K2Node_CallFunction_2"));
    auto* Branch = Node<UK2Node_IfThenElse>(Graph, TEXT("K2Node_IfThenElse_1"));
    auto* OldCast = Node<UK2Node_DynamicCast>(Graph, TEXT("K2Node_DynamicCast_1"));
    auto* ArraySet = Node<UK2Node_CallArrayFunction>(Graph, TEXT("K2Node_CallArrayFunction_0"));
    auto* Pawn = Node<UK2Node_CallFunction>(Graph, TEXT("K2Node_CallFunction_1"));
    auto* DragData = Node<UK2Node_VariableGet>(Graph, TEXT("K2Node_VariableGet_2"));
    auto* OriginalResult = Node<UK2Node_FunctionResult>(Graph, TEXT("K2Node_FunctionResult_0"));
    UClass* CharacterClass = LoadObject<UClass>(nullptr, TEXT("/Script/casino_simulator.casino_simulatorCharacter"));
    UEdGraphPin* ArrayInput = ArraySet ? ArraySet->FindPin(TEXT("TargetArray")) : nullptr;
    auto* NumberSlots = ArrayInput && ArrayInput->LinkedTo.Num() == 1
        ? Cast<UK2Node_VariableGet>(ArrayInput->LinkedTo[0]->GetOwningNode()) : nullptr;
    UEdGraphPin* SlotIndex = ArraySet && ArraySet->FindPin(TEXT("Index")) && ArraySet->FindPin(TEXT("Index"))->LinkedTo.Num() == 1
        ? ArraySet->FindPin(TEXT("Index"))->LinkedTo[0] : nullptr;
    if (Requests || SlotWrites != 1 || !OriginalResult || !SlotIndex || !Pawn || !DragData
        || !CharacterClass || !CharacterClass->FindFunctionByName(TEXT("RequestSetNumberSlot"))
        || !NumberSlots || NumberSlots->VariableReference.GetMemberName() != TEXT("NumberSlots")
        || Pawn->FunctionReference.GetMemberName() != TEXT("GetOwningPlayerPawn")
        || DragData->VariableReference.GetMemberName() != TEXT("DragItemData")
        || !ItemSet || ItemSet->VariableReference.GetMemberName() != TEXT("Item Data")
        || !Draw || Draw->FunctionReference.GetMemberName() != TEXT("SetDraw")
        || !ArraySet || ArraySet->FunctionReference.GetMemberName() != TEXT("Array_Set")
        || !IsLink(DragCast, UEdGraphSchema_K2::PN_Then, ItemSet, UEdGraphSchema_K2::PN_Execute)
        || !IsLink(ItemSet, UEdGraphSchema_K2::PN_Then, Draw, UEdGraphSchema_K2::PN_Execute)
        || !IsLink(Draw, UEdGraphSchema_K2::PN_Then, Branch, UEdGraphSchema_K2::PN_Execute)
        || !IsLink(Branch, UEdGraphSchema_K2::PN_Then, OldCast, UEdGraphSchema_K2::PN_Execute)
        || !IsLink(OldCast, UEdGraphSchema_K2::PN_Then, ArraySet, UEdGraphSchema_K2::PN_Execute)
        || !Branch->GetElsePin()->LinkedTo.IsEmpty())
    { OutError = TEXT("Item-slot drop wiring differs from the inspected quick-slot contract; no asset was saved."); return false; }
    const FString Source = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("BeforeQuickSlots");
    const FString Backup = Directory / FPaths::GetCleanFilename(Source);
    if (!IFileManager::Get().FileExists(*Backup)
        && (!IFileManager::Get().MakeDirectory(*Directory, true) || IFileManager::Get().Copy(*Backup, *Source, false) != COPY_OK))
    { OutError = TEXT("Could not back up the item-slot widget."); return false; }
    const auto UnchangedGraphs = OtherGraphs(Blueprint, Graph);
    const auto WidgetGuids = Blueprint->WidgetVariableNameToGuidMap;
    Blueprint->Modify(); Graph->Modify();
    auto* CharacterCast = Add<UK2Node_DynamicCast>(Graph, 2200, -500);
    CharacterCast->TargetType = CharacterClass; CharacterCast->AllocateDefaultPins();
    auto* Request = Add<UK2Node_CallFunction>(Graph, 2500, -500);
    Request->FunctionReference.SetExternalMember(TEXT("RequestSetNumberSlot"), CharacterClass); Request->AllocateDefaultPins();
    auto* ItemBreak = Add<UK2Node_BreakStruct>(Graph, 2200, -200);
    ItemBreak->StructType = FItemData::StaticStruct();
    // Normal editor placement initializes the engine's override-pin version marker.
    ItemBreak->PostPlacedNewNode();
    ItemBreak->AllocateDefaultPins();
    UEdGraphPin* StructInput = nullptr;
    for (UEdGraphPin* Pin : ItemBreak->Pins) if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) StructInput = Pin;
    auto* Handled = Add<UK2Node_FunctionResult>(Graph, 2850, -500);
    Handled->FunctionReference = OriginalResult->FunctionReference; Handled->AllocateDefaultPins();
    UEdGraphPin* Return = Handled->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
    if (!Return) { OutError = TEXT("Could not construct a handled drop return."); return false; }
    Return->DefaultValue = TEXT("true");
    DragCast->GetThenPin()->BreakAllPinLinks();
    Draw->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    Branch->GetThenPin()->BreakAllPinLinks();
    if (!Link(DragCast->GetThenPin(), Branch->GetExecPin(), OutError)
        || !Link(Branch->GetThenPin(), CharacterCast->GetExecPin(), OutError)
        || !Link(Branch->GetElsePin(), ItemSet->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)
        || !Link(Draw->FindPin(UEdGraphSchema_K2::PN_Then), OriginalResult->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)
        || !Link(Pawn->FindPin(UEdGraphSchema_K2::PN_ReturnValue), CharacterCast->GetCastSourcePin(), OutError)
        || !Link(CharacterCast->GetThenPin(), Request->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)
        || !Link(CharacterCast->GetCastResultPin(), Request->FindPin(UEdGraphSchema_K2::PN_Self), OutError)
        || !Link(SlotIndex, Request->FindPin(TEXT("SlotIndex")), OutError)
        || !Link(DragData->FindPin(TEXT("DragItemData")), StructInput, OutError)
        || !Link(ItemBreak->FindPin(TEXT("UniqueID")), Request->FindPin(TEXT("ItemID")), OutError)
        || !Link(Request->FindPin(UEdGraphSchema_K2::PN_Then), Handled->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)) return false;
    FBlueprintEditorUtils::RemoveNode(Blueprint, ArraySet, true);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    const auto AfterGraphs = OtherGraphs(Blueprint, Graph);
    const bool bGraphsPreserved = AfterGraphs.OrderIndependentCompareEqual(UnchangedGraphs);
    const bool bWidgetGuidsPreserved = Blueprint->WidgetVariableNameToGuidMap.OrderIndependentCompareEqual(WidgetGuids);
    if (Results.NumErrors || Blueprint->Status != BS_UpToDate || !bGraphsPreserved || !bWidgetGuidsPreserved)
    {
        FString ChangedGraph = TEXT("none");
        if (!bGraphsPreserved)
        {
            for (const auto& Entry : UnchangedGraphs)
                if (const FString* After = AfterGraphs.Find(Entry.Key); !After || *After != Entry.Value)
                { ChangedGraph = Entry.Key.ToString(); break; }
            if (ChangedGraph == TEXT("none"))
                for (const auto& Entry : AfterGraphs) if (!UnchangedGraphs.Contains(Entry.Key))
                { ChangedGraph = Entry.Key.ToString(); break; }
        }
        OutError = FString::Printf(TEXT("Quick-slot checks failed: compilerErrors=%d, compilerWarnings=%d, status=%d, otherGraphsPreserved=%s, firstChangedGraph=%s, widgetGuidsPreserved=%s; asset not saved."),
            Results.NumErrors, Results.NumWarnings, int32(Blueprint->Status), bGraphsPreserved ? TEXT("true") : TEXT("false"),
            *ChangedGraph, bWidgetGuidsPreserved ? TEXT("true") : TEXT("false"));
        return false;
    }
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Source, Args))
    { OutError = TEXT("Could not save the quick-slot widget."); return false; }
    return true;
}
