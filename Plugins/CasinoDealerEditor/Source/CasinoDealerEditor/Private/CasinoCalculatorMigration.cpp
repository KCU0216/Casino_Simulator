#include "CasinoCalculatorMigration.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FeedbackContext.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/CasinoCalculatorWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace CasinoCalculatorMigration
{
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
			FString Export; FEdGraphUtilities::ExportNodesToText(Nodes, Export);
			Records.Add(Graph->GetName() + TEXT(":") + Export);
		}
	}
	Records.Sort();
	return FString::Join(Records, TEXT("\n"));
}

FString WidgetSnapshot(UWidgetBlueprint* Blueprint)
{
	TArray<FString> Records;
	Blueprint->WidgetTree->ForEachWidget([&Records](UWidget* Widget)
	{
		FString Record = Widget->GetName() + TEXT(":") + Widget->GetClass()->GetPathName()
			+ TEXT(":") + GetNameSafe(Widget->GetParent());
		for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_Edit)) continue;
			Record += TEXT("\n") + It->GetName() + TEXT("=") + ExportProperty(Widget, It->GetFName());
		}
		Records.Add(Record);
	});
	Records.Sort();
	return FString::Join(Records, TEXT("\n"));
}
}

bool MigrateCasinoCalculatorKeyboard(FString& OutError)
{
	using namespace CasinoCalculatorMigration;
	OutError.Reset();
	const TCHAR* AssetPath = TEXT("/Game/1_BluePrint/Widget/WBP_Calculate.WBP_Calculate");
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, AssetPath);
	if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->GeneratedClass ||
		(Blueprint->ParentClass != UUserWidget::StaticClass() && Blueprint->ParentClass != UCasinoCalculatorWidget::StaticClass()))
	{ OutError = TEXT("Shared calculator asset has an unexpected parent or widget tree."); return false; }
	for (int32 Digit = 0; Digit < 10; ++Digit)
		if (!Cast<UButton>(Blueprint->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("Button_%d"), Digit)))))
		{ OutError = TEXT("Shared calculator has a missing digit button."); return false; }
	for (const TCHAR* Name : { TEXT("Button_Delete"), TEXT("Button_OK"), TEXT("Button_BG") })
		if (!Cast<UButton>(Blueprint->WidgetTree->FindWidget(FName(Name))))
		{ OutError = TEXT("Shared calculator has a missing action button."); return false; }
	if (!FindFProperty<FMulticastDelegateProperty>(Blueprint->GeneratedClass, TEXT("NumberPadEvent")))
	{ OutError = TEXT("Shared calculator NumberPadEvent is missing."); return false; }

	const FString Source = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	const FString BackupDirectory = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup/CalculatorBackups") /
		FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	IFileManager::Get().MakeDirectory(*BackupDirectory, true);
	if (IFileManager::Get().Copy(*(BackupDirectory / FPaths::GetCleanFilename(Source)), *Source) != COPY_OK)
	{ OutError = TEXT("Could not back up the shared calculator before reparenting."); return false; }

	const FString GraphsBefore = GraphSnapshot(Blueprint);
	const FString WidgetsBefore = WidgetSnapshot(Blueprint);
	const FString BindingsBefore = ExportProperty(Blueprint, TEXT("Bindings"));
	const FString VariablesBefore = ExportProperty(Blueprint, TEXT("NewVariables"));
	const TMap<FName, FGuid> GuidsBefore = Blueprint->WidgetVariableNameToGuidMap;
	const SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
	const int32 ErrorsBefore = GWarn ? GWarn->GetNumErrors() : 0;
	Blueprint->Modify();
	Blueprint->ParentClass = UCasinoCalculatorWidget::StaticClass();
	CastChecked<UUserWidget>(Blueprint->GeneratedClass->GetDefaultObject())->SetIsFocusable(true);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	if (Results.NumErrors > 0 || Blueprint->Status != BS_UpToDate || !Blueprint->GeneratedClass->IsChildOf(UCasinoCalculatorWidget::StaticClass()))
	{ OutError = TEXT("Shared calculator failed to compile with keyboard support."); return false; }
	CastChecked<UUserWidget>(Blueprint->GeneratedClass->GetDefaultObject())->SetIsFocusable(true);
	if (GraphSnapshot(Blueprint) != GraphsBefore || WidgetSnapshot(Blueprint) != WidgetsBefore ||
		ExportProperty(Blueprint, TEXT("Bindings")) != BindingsBefore || ExportProperty(Blueprint, TEXT("NewVariables")) != VariablesBefore)
	{ OutError = TEXT("Shared calculator logic, controls, or dispatcher changed; asset was not saved."); return false; }
	for (const TPair<FName, FGuid>& Entry : GuidsBefore)
		if (Blueprint->WidgetVariableNameToGuidMap.FindRef(Entry.Key) != Entry.Value)
		{ OutError = TEXT("Existing calculator widget GUID changed; asset was not saved."); return false; }
	if (FDebug::GetNumEnsureFailures() != EnsuresBefore || (GWarn && GWarn->GetNumErrors() > ErrorsBefore))
	{ OutError = TEXT("Calculator migration logged an engine error; asset was not saved."); return false; }

	FSavePackageArgs Save;
	Save.TopLevelFlags = RF_Public | RF_Standalone;
	Save.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Source, Save))
	{ OutError = TEXT("Could not save the shared calculator keyboard parent."); return false; }
	return true;
}
