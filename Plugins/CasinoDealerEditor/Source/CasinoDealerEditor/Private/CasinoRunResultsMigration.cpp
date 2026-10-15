#include "CasinoRunResultsMigration.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/CasinoManagedWidget.h"
#include "UI/CasinoRunResultWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace CasinoRunResultsMigration
{
FString GraphSnapshot(UBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    FString Result;
    for (UEdGraph* Graph : Graphs)
    {
        TSet<UObject*> Nodes;
        for (UEdGraphNode* Node : Graph->Nodes) if (Node) Nodes.Add(Node);
        FString Export;
        FEdGraphUtilities::ExportNodesToText(Nodes, Export);
        Result += Graph->GetName() + TEXT("\n") + Export;
    }
    return Result;
}
}

bool MigrateCasinoRunResults(FString& OutError)
{
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr,
        TEXT("/Game/Blackjack/WBP/WBP_Result.WBP_Result"));
    if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->ParentClass
        || !Blueprint->ParentClass->IsChildOf(UCasinoManagedWidget::StaticClass())
        || !Cast<UVerticalBox>(Blueprint->WidgetTree->FindWidget(TEXT("VerticalBox_33")))
        || !Cast<UButton>(Blueprint->WidgetTree->FindWidget(TEXT("RestartButton")))
        || !Cast<UButton>(Blueprint->WidgetTree->FindWidget(TEXT("MainMenuButton"))))
    { OutError = TEXT("Result widget does not match the expected managed screen contract."); return false; }
    if (Blueprint->ParentClass == UCasinoRunResultWidget::StaticClass()) return true;

    const FString Source = FPackageName::LongPackageNameToFilename(
        Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup/BeforeRunResults");
    const FString Backup = Directory / FPaths::GetCleanFilename(Source);
    if (!IFileManager::Get().FileExists(*Backup)
        && (!IFileManager::Get().MakeDirectory(*Directory, true)
            || IFileManager::Get().Copy(*Backup, *Source, false) != COPY_OK))
    { OutError = TEXT("Could not back up the existing result widget."); return false; }

    const FString Before = CasinoRunResultsMigration::GraphSnapshot(Blueprint);
    const auto Guids = Blueprint->WidgetVariableNameToGuidMap;
    Blueprint->Modify();
    Blueprint->ParentClass = UCasinoRunResultWidget::StaticClass();
    FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    if (Results.NumErrors || Blueprint->Status != BS_UpToDate)
    { OutError = TEXT("Result widget failed to compile; the asset was not saved."); return false; }
    if (CasinoRunResultsMigration::GraphSnapshot(Blueprint) != Before
        || Blueprint->WidgetVariableNameToGuidMap.OrderIndependentCompareEqual(Guids) == false)
    { OutError = TEXT("Result graph or widget identities changed; the asset was not saved."); return false; }
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Source, Args))
    { OutError = TEXT("Could not save the result widget."); return false; }
    return true;
}
