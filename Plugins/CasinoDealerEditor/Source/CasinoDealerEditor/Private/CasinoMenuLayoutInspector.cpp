#include "CasinoMenuLayoutInspector.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace
{
TSharedRef<FJsonObject> ExportEditableProperties(UObject* Object)
{
    TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
    if (!Object) return Values;
    for (TFieldIterator<FProperty> Property(Object->GetClass()); Property; ++Property)
    {
        if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) continue;
        for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
        {
            FString Value;
            Property->ExportText_InContainer(Index, Value, Object, nullptr, Object, PPF_None);
            const FString Key = Property->ArrayDim == 1 ? Property->GetName()
                : FString::Printf(TEXT("%s[%d]"), *Property->GetName(), Index);
            Values->SetStringField(Key, Value);
        }
    }
    return Values;
}

bool WriteJson(const TSharedRef<FJsonObject>& Object, const FString& Filename)
{
    FString Json;
    const auto Writer = TJsonWriterFactory<>::Create(&Json);
    return FJsonSerializer::Serialize(Object, Writer)
        && FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool InspectWidget(const TCHAR* AssetPath, const FString& OutputDirectory, FString& OutError)
{
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, AssetPath);
    if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->WidgetTree->RootWidget || !Blueprint->GeneratedClass)
    {
        OutError = FString::Printf(TEXT("Menu asset has no complete WidgetBlueprint/WidgetTree: %s"), AssetPath);
        return false;
    }
    const FString Directory = OutputDirectory / Blueprint->GetName();
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    {
        OutError = TEXT("Could not create UI inspection directory: ") + Directory;
        return false;
    }

    TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
    Metadata->SetStringField(TEXT("asset"), Blueprint->GetPathName());
    Metadata->SetStringField(TEXT("parent"), GetPathNameSafe(Blueprint->ParentClass));
    Metadata->SetStringField(TEXT("class"), Blueprint->GeneratedClass->GetPathName());
    Metadata->SetStringField(TEXT("root"), Blueprint->WidgetTree->RootWidget->GetName());
    Metadata->SetNumberField(TEXT("status"), static_cast<int32>(Blueprint->Status));
    Metadata->SetObjectField(TEXT("blueprintEditableProperties"), ExportEditableProperties(Blueprint));
    Metadata->SetObjectField(TEXT("defaults"), ExportEditableProperties(Blueprint->GeneratedClass->GetDefaultObject()));

    // Include every designer widget, without descending into foreign UserWidget trees.
    TArray<UWidget*> Widgets;
    Blueprint->WidgetTree->GetAllWidgets(Widgets);
    TArray<TSharedPtr<FJsonValue>> WidgetRecords;
    for (UWidget* Widget : Widgets)
    {
        if (!Widget) continue;
        TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("name"), Widget->GetName());
        Record->SetStringField(TEXT("path"), Widget->GetPathName());
        Record->SetStringField(TEXT("class"), Widget->GetClass()->GetPathName());
        Record->SetStringField(TEXT("parent"), GetNameSafe(Widget->GetParent()));
        Record->SetBoolField(TEXT("isVariable"), Widget->bIsVariable);
        Record->SetObjectField(TEXT("properties"), ExportEditableProperties(Widget));
        if (Widget->Slot)
        {
            TSharedRef<FJsonObject> Slot = MakeShared<FJsonObject>();
            Slot->SetStringField(TEXT("class"), Widget->Slot->GetClass()->GetPathName());
            Slot->SetObjectField(TEXT("properties"), ExportEditableProperties(Widget->Slot));
            Record->SetObjectField(TEXT("slot"), Slot);
        }
        if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
        {
            TArray<TSharedPtr<FJsonValue>> Children;
            for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
            {
                Children.Add(MakeShared<FJsonValueString>(GetNameSafe(Panel->GetChildAt(Index))));
            }
            Record->SetArrayField(TEXT("children"), Children);
        }
        WidgetRecords.Add(MakeShared<FJsonValueObject>(Record));
    }
    Metadata->SetArrayField(TEXT("widgets"), WidgetRecords);

    TArray<TSharedPtr<FJsonValue>> Variables;
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("name"), Variable.VarName.ToString());
        Record->SetStringField(TEXT("category"), Variable.VarType.PinCategory.ToString());
        Record->SetStringField(TEXT("type"), GetPathNameSafe(Variable.VarType.PinSubCategoryObject.Get()));
        Record->SetNumberField(TEXT("container"), static_cast<int32>(Variable.VarType.ContainerType));
        Record->SetStringField(TEXT("default"), Variable.DefaultValue);
        Variables.Add(MakeShared<FJsonValueObject>(Record));
    }
    Metadata->SetArrayField(TEXT("variables"), Variables);

    // Binding is not CPF_Edit on every engine version; export it explicitly by reflection.
    if (const FProperty* Bindings = FindFProperty<FProperty>(Blueprint->GetClass(), TEXT("Bindings")))
    {
        FString Text;
        Bindings->ExportText_InContainer(0, Text, Blueprint, nullptr, Blueprint, PPF_None);
        Metadata->SetStringField(TEXT("bindings"), Text);
    }
    if (!WriteJson(Metadata, Directory / TEXT("layout.json")))
    {
        OutError = TEXT("Could not save menu layout inspection: ") + Directory;
        return false;
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) continue;
        TSet<UObject*> Nodes;
        for (UEdGraphNode* Node : Graph->Nodes) if (Node) Nodes.Add(Node);
        FString Text;
        FEdGraphUtilities::ExportNodesToText(Nodes, Text);
        if (!FFileHelper::SaveStringToFile(Text, *(Directory / (Graph->GetName() + TEXT(".txt"))),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = TEXT("Could not save menu graph inspection: ") + Graph->GetName();
            return false;
        }
    }
    return true;
}
}

bool InspectCasinoMenuWidgets(FString& OutError)
{
    OutError.Reset();
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup/UIInspect");
    const TCHAR* Paths[] = {
        TEXT("/Game/Blackjack/Level/WBP_MainMenu.WBP_MainMenu"),
        TEXT("/Game/Blackjack/Level/WBP_LobbyUI.WBP_LobbyUI"),
        TEXT("/Game/Blackjack/Level/WBP_LobbyPlayerEntry.WBP_LobbyPlayerEntry"),
        TEXT("/Game/Blackjack/Level/WBP_RoomEntry.WBP_RoomEntry"),
        TEXT("/Game/KCU/WBP_Settings.WBP_Settings")
    };
    for (const TCHAR* Path : Paths)
    {
        if (!InspectWidget(Path, Directory, OutError)) return false;
    }
    return true;
}
