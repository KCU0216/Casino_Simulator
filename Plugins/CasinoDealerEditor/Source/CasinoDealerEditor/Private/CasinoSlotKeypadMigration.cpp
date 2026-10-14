#include "CasinoSlotKeypadMigration.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelSlot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_VariableSet.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FeedbackContext.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/CasinoCalculatorWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace CasinoSlotKeypadMigration
{
bool CloseUnconfirmedZeroAmount(UWidgetBlueprint* Blueprint, FString& OutError)
{
    UEdGraph* ConfirmGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        if (Graph && Graph->GetFName() == TEXT("ConfirmBetAmount")) ConfirmGraph = Graph;
    if (!ConfirmGraph)
    { OutError = TEXT("Slot confirmation graph is missing."); return false; }

    UK2Node_VariableSet* Unconfirmed = nullptr;
    UK2Node_CallFunction* HideKeypad = nullptr;
    for (UEdGraphNode* Node : ConfirmGraph->Nodes)
    {
        if (UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(Node))
        {
            UEdGraphPin* Rejected = Branch->GetElsePin();
            if (Rejected && Rejected->LinkedTo.Num() == 1)
            {
                UEdGraphNode* RejectedNode = Rejected->LinkedTo[0]->GetOwningNode();
                TSet<UEdGraphNode*> Visited;
                while (UK2Node_Knot* Reroute = Cast<UK2Node_Knot>(RejectedNode))
                {
                    UEdGraphPin* Output = Reroute->GetOutputPin();
                    if (Visited.Contains(Reroute) || !Output || Output->LinkedTo.Num() != 1)
                    { RejectedNode = nullptr; break; }
                    Visited.Add(Reroute);
                    RejectedNode = Output->LinkedTo[0]->GetOwningNode();
                }
                UK2Node_VariableSet* Setter = Cast<UK2Node_VariableSet>(RejectedNode);
                if (Setter && Setter->VariableReference.GetMemberName() == TEXT("bHasConfirmedBet"))
                {
                    const UEdGraphPin* Value = Setter->FindPin(TEXT("bHasConfirmedBet"), EGPD_Input);
                    if (Value && Value->LinkedTo.IsEmpty() && Value->DefaultValue == TEXT("false"))
                        Unconfirmed = Setter;
                }
            }
        }
        if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
            if (Call->FunctionReference.GetMemberName() == TEXT("SetVisibility"))
                if (const UEdGraphPin* Visibility = Call->FindPin(TEXT("InVisibility")))
                    if (Visibility->DefaultValue == TEXT("Hidden") || Visibility->DefaultValue == TEXT("ESlateVisibility::Hidden"))
                        HideKeypad = Call;
    }
    UEdGraphPin* Then = Unconfirmed ? Unconfirmed->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
    UEdGraphPin* Hide = HideKeypad ? HideKeypad->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input) : nullptr;
    if (!Then || !Hide)
    { OutError = TEXT("Slot confirmation no longer has the expected rejection and hide nodes."); return false; }
    if (Then->LinkedTo.Num() == 1 && Then->LinkedTo[0] == Hide) return true;
    if (!Then->LinkedTo.IsEmpty())
    { OutError = TEXT("Slot rejection already has another action; it was not replaced."); return false; }

    ConfirmGraph->Modify();
    Unconfirmed->Modify();
    HideKeypad->Modify();
    const TArray<UEdGraphPin*> ExistingCloseLinks = Hide->LinkedTo;
    if (!ConfirmGraph->GetSchema()->TryCreateConnection(Then, Hide))
    { OutError = TEXT("Could not connect zero-amount rejection to the existing keypad close action."); return false; }
    for (UEdGraphPin* Original : ExistingCloseLinks)
        if (!Hide->LinkedTo.Contains(Original) || !Original->LinkedTo.Contains(Hide))
        { OutError = TEXT("An existing positive-amount close connection changed; asset was not saved."); return false; }
    return true;
}

FString ExportProperty(UObject* Object, FName Name)
{
    FString Value;
    if (const FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), Name))
        Property->ExportText_InContainer(0, Value, Object, nullptr, Object, PPF_None);
    return Value;
}

FString GraphSnapshot(UWidgetBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<FString> Records;
    for (UEdGraph* Graph : Graphs) if (Graph)
    {
        Records.Add(Graph->GetPathName());
        for (UEdGraphNode* Node : Graph->Nodes) if (Node)
        {
            TSet<UObject*> Nodes; Nodes.Add(Node);
            FString Export;
            FEdGraphUtilities::ExportNodesToText(Nodes, Export);
            Records.Add(Graph->GetName() + TEXT(":") + Export);
        }
    }
    Records.Sort();
    return FString::Join(Records, TEXT("\n"));
}

FString WidgetSnapshot(UWidgetBlueprint* Blueprint)
{
    TArray<FString> Records;
    Records.Add(TEXT("Root=") + GetNameSafe(Blueprint->WidgetTree->RootWidget));
    Blueprint->WidgetTree->ForEachWidget([&Records](UWidget* Widget)
    {
        FString Record = Widget->GetName() + TEXT(":") + Widget->GetClass()->GetPathName()
            + TEXT(":") + GetNameSafe(Widget->GetParent());
        for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
            if (It->HasAnyPropertyFlags(CPF_Edit))
                Record += TEXT("\n") + It->GetName() + TEXT("=") + ExportProperty(Widget, It->GetFName());
        if (Widget->Slot)
            for (TFieldIterator<FProperty> It(Widget->Slot->GetClass()); It; ++It)
                if (It->HasAnyPropertyFlags(CPF_Edit))
                    Record += TEXT("\nSlot.") + It->GetName() + TEXT("=") + ExportProperty(Widget->Slot, It->GetFName());
        Record += FString::Printf(TEXT("\nVariable=%d"), Widget->bIsVariable);
        Records.Add(Record);
    });
    Records.Sort();
    return FString::Join(Records, TEXT("\n"));
}

FString DefaultsSnapshot(UWidgetBlueprint* Blueprint)
{
    UObject* Defaults = Blueprint->GeneratedClass->GetDefaultObject();
    TArray<FString> Records;
    for (TFieldIterator<FProperty> It(Defaults->GetClass()); It; ++It)
    {
        if (It->GetFName() == TEXT("bIsFocusable")) continue;
        if (It->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
            Records.Add(It->GetName() + TEXT("=") + ExportProperty(Defaults, It->GetFName()));
    }
    Records.Sort();
    return FString::Join(Records, TEXT("\n"));
}
}

bool MigrateCasinoSlotKeypad(FString& OutError)
{
    using namespace CasinoSlotKeypadMigration;
    OutError.Reset();
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr,
        TEXT("/Game/LJH/UI/WBP_SlotMachineControl.WBP_SlotMachineControl"));
    if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->GeneratedClass ||
        (Blueprint->ParentClass != UUserWidget::StaticClass() && Blueprint->ParentClass != UCasinoCalculatorWidget::StaticClass()))
    { OutError = TEXT("Slot controls have an unexpected parent or widget tree."); return false; }
    for (int32 Digit = 0; Digit < 10; ++Digit)
        if (!Cast<UButton>(Blueprint->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("Button_%d"), Digit)))))
        { OutError = TEXT("Slot keypad has a missing digit button."); return false; }
    for (const TCHAR* Name : { TEXT("Button_Backspace"), TEXT("Button_OK"), TEXT("Button_BetAmount"), TEXT("Button_PullLever"), TEXT("Button_Exit") })
        if (!Cast<UButton>(Blueprint->WidgetTree->FindWidget(FName(Name))))
        { OutError = TEXT("Slot keypad has a missing original action button."); return false; }
    for (const TCHAR* Name : { TEXT("AppendDigit"), TEXT("BackspaceBet"), TEXT("ConfirmBetAmount"), TEXT("ResetBetInput") })
        if (!Blueprint->GeneratedClass->FindFunctionByName(FName(Name)))
        { OutError = TEXT("Slot keypad is missing an original betting-input function."); return false; }

    const FString Source = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString BackupDirectory = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup/SlotKeypadBackups") /
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true) ||
        IFileManager::Get().Copy(*(BackupDirectory / FPaths::GetCleanFilename(Source)), *Source) != COPY_OK)
    { OutError = TEXT("Could not back up slot controls before reparenting."); return false; }

    // Rejecting a zero bet still leaves it unconfirmed, but must also dismiss
    // the keypad. Both OK and Enter already use this same confirmation graph.
    Blueprint->Modify();
    if (!CloseUnconfirmedZeroAmount(Blueprint, OutError)) return false;

    const FString GraphsBefore = GraphSnapshot(Blueprint);
    const FString WidgetsBefore = WidgetSnapshot(Blueprint);
    const FString DefaultsBefore = DefaultsSnapshot(Blueprint);
    const FString BindingsBefore = ExportProperty(Blueprint, TEXT("Bindings"));
    const FString VariablesBefore = ExportProperty(Blueprint, TEXT("NewVariables"));
    const TMap<FName, FGuid> GuidsBefore = Blueprint->WidgetVariableNameToGuidMap;
    const SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
    const int32 ErrorsBefore = GWarn ? GWarn->GetNumErrors() : 0;

    // The existing slot number buttons and ConfirmBetAmount stay authoritative.
    // The shared parent styles only keypad controls and routes Enter to the
    // original OK click; it never invokes the separate lever/spin action.
    Blueprint->Modify();
    Blueprint->ParentClass = UCasinoCalculatorWidget::StaticClass();
    CastChecked<UUserWidget>(Blueprint->GeneratedClass->GetDefaultObject())->SetIsFocusable(true);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    if (Results.NumErrors > 0 || Blueprint->Status != BS_UpToDate ||
        !Blueprint->GeneratedClass->IsChildOf(UCasinoCalculatorWidget::StaticClass()))
    { OutError = TEXT("Slot controls failed to compile with shared keypad support."); return false; }
    CastChecked<UUserWidget>(Blueprint->GeneratedClass->GetDefaultObject())->SetIsFocusable(true);

    if (GraphSnapshot(Blueprint) != GraphsBefore || WidgetSnapshot(Blueprint) != WidgetsBefore ||
        DefaultsSnapshot(Blueprint) != DefaultsBefore || ExportProperty(Blueprint, TEXT("Bindings")) != BindingsBefore ||
        ExportProperty(Blueprint, TEXT("NewVariables")) != VariablesBefore || Blueprint->WidgetVariableNameToGuidMap.Num() != GuidsBefore.Num())
    { OutError = TEXT("Slot input logic, controls, defaults or bindings changed; asset was not saved."); return false; }
    for (const TPair<FName, FGuid>& Entry : GuidsBefore)
        if (Blueprint->WidgetVariableNameToGuidMap.FindRef(Entry.Key) != Entry.Value)
        { OutError = TEXT("An original slot widget GUID changed; asset was not saved."); return false; }
    if (FDebug::GetNumEnsureFailures() != EnsuresBefore || (GWarn && GWarn->GetNumErrors() > ErrorsBefore))
    { OutError = TEXT("Slot keypad migration logged an engine error; asset was not saved."); return false; }

    FSavePackageArgs Save;
    Save.TopLevelFlags = RF_Public | RF_Standalone;
    Save.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Source, Save))
    { OutError = TEXT("Could not save the slot keypad parent."); return false; }
    return true;
}
