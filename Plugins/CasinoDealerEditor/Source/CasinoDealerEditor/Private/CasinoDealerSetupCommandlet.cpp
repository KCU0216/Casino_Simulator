#include "CasinoDealerSetupCommandlet.h"

#include "DealerAnimationAssetBuilder.h"
#include "DealerBlueprintMigration.h"
#include "DealerVisualPreview.h"
#include "CasinoMenuLayoutInspector.h"
#include "CasinoMenuLayoutMigration.h"
#include "CasinoRoomWidgetMigration.h"
#include "CasinoBlackjackUILayoutMigration.h"
#include "CasinoShopUILayoutMigration.h"
#include "BlackjackCardSoundSetup.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Components/ActorComponent.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCasinoDealerSetup, Log, All);

namespace
{
const TCHAR* BlueprintPaths[] = {
    TEXT("/Game/Blackjack/BluePrint/BP_BlackjackTable.BP_BlackjackTable"),
    TEXT("/Game/Blackjack/BluePrint/BP_NPC_BlackjackDealer.BP_NPC_BlackjackDealer"),
    TEXT("/Game/Blackjack/BluePrint/Card/BP_BlackjackCard.BP_BlackjackCard"),
    TEXT("/Game/Blackjack/BluePrint/Notify/AN_Dealer_DealCardRelease.AN_Dealer_DealCardRelease")};

bool DumpBlueprint(UBlueprint* Blueprint, const FString& Directory)
{
    if (!Blueprint || !Blueprint->GeneratedClass) return false;
    const FString AssetDirectory = Directory / Blueprint->GetName();
    IFileManager::Get().MakeDirectory(*AssetDirectory, true);
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        TSet<UObject*> Nodes;
        for (UEdGraphNode* Node : Graph->Nodes) if (Node) Nodes.Add(Node);
        FString Exported;
        FEdGraphUtilities::ExportNodesToText(Nodes, Exported);
        if (!FFileHelper::SaveStringToFile(Exported, *(AssetDirectory / (Graph->GetName() + TEXT(".txt"))),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
    }
    TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
    Metadata->SetStringField(TEXT("parent"), Blueprint->ParentClass->GetPathName());
    Metadata->SetStringField(TEXT("class"), Blueprint->GeneratedClass->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Variables;
    for (const FBPVariableDescription& Description : Blueprint->NewVariables)
    {
        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Description.VarName.ToString());
        Item->SetStringField(TEXT("category"), Description.VarType.PinCategory.ToString());
        Item->SetStringField(TEXT("subtype"), GetPathNameSafe(Description.VarType.PinSubCategoryObject.Get()));
        Item->SetNumberField(TEXT("container"), static_cast<int32>(Description.VarType.ContainerType));
        Variables.Add(MakeShared<FJsonValueObject>(Item));
    }
    Metadata->SetArrayField(TEXT("variables"), Variables);
    TArray<TSharedPtr<FJsonValue>> Components;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
            Item->SetStringField(TEXT("class"), GetPathNameSafe(Node->ComponentClass));
            Item->SetStringField(TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
            FString Values;
            if (Node->ComponentTemplate)
            {
                for (TFieldIterator<FProperty> Property(Node->ComponentTemplate->GetClass()); Property; ++Property)
                {
                    if (!Property->HasAnyPropertyFlags(CPF_Edit)) continue;
                    FString Value;
                    Property->ExportText_InContainer(0, Value, Node->ComponentTemplate, nullptr,
                        Node->ComponentTemplate, PPF_None);
                    Values += Property->GetName() + TEXT("=") + Value + TEXT("\n");
                }
            }
            Item->SetStringField(TEXT("defaults"), Values);
            Components.Add(MakeShared<FJsonValueObject>(Item));
        }
    }
    Metadata->SetArrayField(TEXT("components"), Components);
    FString CDOValues;
    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    for (TFieldIterator<FProperty> Property(Blueprint->GeneratedClass); Property; ++Property)
    {
        if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) continue;
        FString Value;
        Property->ExportText_InContainer(0, Value, CDO, nullptr, CDO, PPF_None);
        CDOValues += Property->GetName() + TEXT("=") + Value + TEXT("\n");
    }
    Metadata->SetStringField(TEXT("defaults"), CDOValues);
    FString Json;
    const auto Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Metadata, Writer);
    return FFileHelper::SaveStringToFile(Json, *(AssetDirectory / TEXT("metadata.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
}

UCasinoDealerSetupCommandlet::UCasinoDealerSetupCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UCasinoDealerSetupCommandlet::Main(const FString& Params)
{
    FString Mode = TEXT("Inspect");
    FParse::Value(*Params, TEXT("Mode="), Mode);
    if (Mode.Equals(TEXT("Preview"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!CreateDealerVisualPreview(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Dealer preview creation failed: %s"), *Error);
            return 1;
        }
        return 0;
    }
    if (Mode.Equals(TEXT("UIInspect"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!InspectCasinoMenuWidgets(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Menu layout inspection failed: %s"), *Error);
            return 1;
        }
        return 0;
    }
    if (Mode.Equals(TEXT("UIPolish"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!MigrateCasinoMenuLayouts(Error) || !InspectCasinoMenuWidgets(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Menu layout update failed: %s"), *Error);
            return 1;
        }
        UE_LOG(LogCasinoDealerSetup, Display, TEXT("Menu layout update completed."));
        return 0;
    }
    if (Mode.Equals(TEXT("SettingsUI"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!MigrateCasinoSettingsLayout(Error) || !InspectCasinoMenuWidgets(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Settings layout update failed: %s"), *Error);
            return 1;
        }
        UE_LOG(LogCasinoDealerSetup, Display, TEXT("Settings layout update completed; settings logic and defaults preserved."));
        return 0;
    }
    if (Mode.Equals(TEXT("RoomUI"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!MigrateCasinoRoomWidgets(Error) || !InspectCasinoMenuWidgets(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Room dialog migration failed: %s"), *Error);
            return 1;
        }
        UE_LOG(LogCasinoDealerSetup, Display, TEXT("Room dialog migration completed."));
        return 0;
    }
    if (Mode.Equals(TEXT("PauseUI"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!MigrateCasinoPauseMenuLayout(Error) || !InspectCasinoMenuWidgets(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("In-game menu layout update failed: %s"), *Error);
            return 1;
        }
        UE_LOG(LogCasinoDealerSetup, Display, TEXT("In-game menu layout update completed; navigation and actions preserved."));
        return 0;
    }
    if (Mode.Equals(TEXT("BlackjackUI"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!MigrateCasinoBlackjackUILayout(Error) || !InspectCasinoMenuWidgets(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Blackjack UI update failed: %s"), *Error);
            return 1;
        }
        UE_LOG(LogCasinoDealerSetup, Display, TEXT("Blackjack UI update completed."));
        return 0;
    }
    if (Mode.Equals(TEXT("ShopUI"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!MigrateCasinoShopLayouts(Error) || !InspectCasinoMenuWidgets(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Shop UI update failed: %s"), *Error);
            return 1;
        }
        UE_LOG(LogCasinoDealerSetup, Display, TEXT("Shop UI update completed."));
        return 0;
    }
    if (Mode.Equals(TEXT("CardSound"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!SetupBlackjackCardPlacementSound(Error) || !ValidateBlackjackCardPlacementSound(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Card placement sound setup failed: %s"), *Error);
            return 1;
        }
        UE_LOG(LogCasinoDealerSetup, Display, TEXT("Card placement sound setup completed."));
        return 0;
    }
    if (Mode.Equals(TEXT("Build"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!BuildBlackjackDealerAnimationAssets(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Dealer animation generation failed: %s"), *Error);
            return 1;
        }
    }
    if (Mode.Equals(TEXT("Migrate"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!MigrateBlackjackDealerBlueprints(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Dealer Blueprint migration failed: %s"), *Error);
            return 1;
        }
    }
    if (Mode.Equals(TEXT("Refine"), ESearchCase::IgnoreCase))
    {
        FString Error;
        if (!ExtendBlackjackDealerPresentation(Error))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Dealer facing and shuffle setup failed: %s"), *Error);
            return 1;
        }
    }
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup") / Mode;
    for (const TCHAR* Path : BlueprintPaths)
    {
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
        if (!DumpBlueprint(Blueprint, Directory))
        {
            UE_LOG(LogCasinoDealerSetup, Error, TEXT("Could not inspect %s"), Path);
            return 1;
        }
    }
    UE_LOG(LogCasinoDealerSetup, Display, TEXT("Dealer setup %s completed; exported graphs: %s"), *Mode, *Directory);
    return 0;
}
