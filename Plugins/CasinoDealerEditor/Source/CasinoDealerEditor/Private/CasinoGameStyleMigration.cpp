#include "CasinoGameStyleMigration.h"

#include "CasinoWidgetStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelSlot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LadderWidget.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace CasinoGameStyle
{
const TCHAR* Paths[] = {
    TEXT("/Game/KCU/LadderMachine/Widget/WBP_LadderControlWidget.WBP_LadderControlWidget"),
    TEXT("/Game/KCU/LadderMachine/Widget/WBP_MachineScreen.WBP_MachineScreen"),
    TEXT("/Game/KCU/LadderMachine/Widget/WBP_LadderWidget.WBP_LadderWidget"),
    TEXT("/Game/KCU/LadderMachine/Widget/WBP_RailButton.WBP_RailButton"),
    TEXT("/Game/KCU/LadderMachine/Widget/WBP_ResultWidget.WBP_ResultWidget"),
    TEXT("/Game/KCU/LadderMachine/Widget/WBP_ResultSlot.WBP_ResultSlot"),
    TEXT("/Game/1_BluePrint/Widget/WBP_DiceBetting.WBP_DiceBetting"),
    TEXT("/Game/1_BluePrint/Widget/WBP_ThreeCardPokerBetting.WBP_ThreeCardPokerBetting")};

bool Protected(UWidget* Widget)
{
    // Includes the ladder's existing keypad launch button and all its children.
    for (UWidget* Current = Widget; Current; Current = Current->GetParent())
        if (Current->GetFName() == TEXT("넘패드") || Current->GetName().Contains(TEXT("Calculate"))) return true;
    return false;
}

bool VisualProperty(UObject* Object, FName Name)
{
    if (UWidget* Widget = Cast<UWidget>(Object)) if (Protected(Widget)) return false;
    if (Cast<UButton>(Object))
        return Name == TEXT("WidgetStyle") || Name == TEXT("BackgroundColor") || Name == TEXT("ColorAndOpacity");
    if (Cast<UTextBlock>(Object)) return Name == TEXT("ColorAndOpacity");
    if (Cast<UBorder>(Object)) return Name == TEXT("Background") || Name == TEXT("BrushColor");
    if (Cast<UImage>(Object)) return Name == TEXT("Brush") || Name == TEXT("ColorAndOpacity");
    if (Cast<ULadderWidget>(Object))
        return Name == TEXT("LineColor") || Name == TEXT("TraceColor") || Name == TEXT("MarbleColor");
    return false;
}

void Properties(UObject* Object, TArray<FString>& Records, bool AllowVisual)
{
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) continue;
        if (AllowVisual && VisualProperty(Object, It->GetFName())) continue;
        FString Value;
        It->ExportText_InContainer(0, Value, Object, nullptr, Object, PPF_None);
        Records.Add(Object->GetPathName() + TEXT(".") + It->GetName() + TEXT("=") + Value);
    }
}

FString Contract(UWidgetBlueprint* Blueprint)
{
    TArray<FString> Records;
    Records.Add(TEXT("Root=") + GetNameSafe(Blueprint->WidgetTree->RootWidget));
    Properties(Blueprint, Records, false);
    Properties(Blueprint->GeneratedClass->GetDefaultObject(), Records, true);
    for (FName Name : {FName(TEXT("Bindings")), FName(TEXT("Animations")), FName(TEXT("NewVariables"))})
        if (const FProperty* Property = FindFProperty<FProperty>(Blueprint->GetClass(), Name))
        {
            FString Value;
            Property->ExportText_InContainer(0, Value, Blueprint, nullptr, Blueprint, PPF_None);
            Records.Add(Name.ToString() + TEXT("=") + Value);
        }
    for (const auto& Entry : Blueprint->WidgetVariableNameToGuidMap)
        Records.Add(Entry.Key.ToString() + TEXT("=") + Entry.Value.ToString());
    Blueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
    {
        Records.Add(Widget->GetName() + TEXT(":") + Widget->GetClass()->GetPathName()
            + TEXT(":") + GetNameSafe(Widget->GetParent()) + FString::Printf(TEXT(":Variable=%d"), Widget->bIsVariable));
        Properties(Widget, Records, true);
        // Every slot, including the embedded calculator, remains byte-for-byte equivalent.
        if (Widget->Slot) Properties(Widget->Slot, Records, false);
        if (const UButton* Button = Cast<UButton>(Widget))
        {
            const FButtonStyle& Style = Button->GetStyle();
            Records.Add(Widget->GetName() + FString::Printf(TEXT(":Padding=%f,%f,%f,%f/%f,%f,%f,%f"),
                Style.NormalPadding.Left, Style.NormalPadding.Top, Style.NormalPadding.Right, Style.NormalPadding.Bottom,
                Style.PressedPadding.Left, Style.PressedPadding.Top, Style.PressedPadding.Right, Style.PressedPadding.Bottom));
        }
    });
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        TSet<UObject*> Nodes;
        for (UEdGraphNode* Node : Graph->Nodes) if (Node) Nodes.Add(Node);
        FString Export;
        FEdGraphUtilities::ExportNodesToText(Nodes, Export);
        Records.Add(Graph->GetName() + TEXT("=") + Export);
    }
    Records.Sort();
    return FString::Join(Records, TEXT("\n"));
}

void LadderColors(ULadderWidget* Ladder)
{
    if (!Ladder) return;
    Ladder->LineColor = CasinoWidgetStyle::Ivory;
    Ladder->TraceColor = CasinoWidgetStyle::Gold;
    Ladder->MarbleColor = FLinearColor(.94f, .67f, .29f, 1.f);
}

void Apply(UWidgetBlueprint* Blueprint)
{
    using namespace CasinoWidgetStyle;
    LadderColors(Cast<ULadderWidget>(Blueprint->GeneratedClass->GetDefaultObject()));
    Blueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
    {
        if (Protected(Widget)) return;
        Widget->Modify();
        LadderColors(Cast<ULadderWidget>(Widget));
        if (UTextBlock* Text = Cast<UTextBlock>(Widget))
        {
            Text->SetColorAndOpacity(FSlateColor(Text->GetFName() == TEXT("InsufficientText")
                ? FLinearColor(.86f, .22f, .17f) : Text->GetFName() == TEXT("ResultText")
                || Blueprint->GetFName() == TEXT("WBP_ResultSlot") ? Gold : Ivory));
            if (Cast<UButton>(Text->GetParent())) Text->SetColorAndOpacity(FSlateColor::UseForeground());
        }
        if (UButton* Button = Cast<UButton>(Widget))
        {
            // The result-dismiss button is an invisible full-screen hit target.
            if (Button->GetFName() == TEXT("결과창닫기버튼")) return;
            const FButtonStyle Original = Button->GetStyle();
            const bool Primary = Button->GetFName() == TEXT("Start") || Button->GetFName() == TEXT("Button_Start")
                || Button->GetFName() == TEXT("Button_Play") || Button->GetFName() == TEXT("Button_Bet");
            UTextBlock* Caption = Cast<UTextBlock>(Button->GetContent());
            const FSlateFontInfo OriginalFont = Caption ? Caption->GetFont() : FSlateFontInfo();
            ButtonStyle(Button, Primary);
            if (Caption) Caption->SetFont(OriginalFont);
            FButtonStyle Style = Button->GetStyle();
            Style.NormalPadding = Original.NormalPadding;
            Style.PressedPadding = Original.PressedPadding;
            Button->SetStyle(Style);
            Button->SetBackgroundColor(FLinearColor::White);
            Button->SetColorAndOpacity(FLinearColor::White);
            if (UTextBlock* Text = Cast<UTextBlock>(Button->GetContent()))
                Text->SetColorAndOpacity(FSlateColor::UseForeground());
        }
        if (UBorder* Border = Cast<UBorder>(Widget))
            Border->SetBrush(Brush(Panel, 10.f, Gold * .5f));
        if (UImage* Image = Cast<UImage>(Widget))
        {
            if (Image->GetFName() == TEXT("BackgroundImage"))
                Image->SetBrush(Brush(Background, 10.f, Gold * .5f));
            else if (Blueprint->GetFName() == TEXT("WBP_RailButton"))
                Image->SetColorAndOpacity(Gold);
        }
    });
}
}

bool MigrateCasinoGameStyles(FString& OutError)
{
    using namespace CasinoGameStyle;
    TArray<UWidgetBlueprint*> Blueprints;
    const FString Backup = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup/GameStyleBackup") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    for (const TCHAR* Path : Paths)
    {
        UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, Path);
        if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->GeneratedClass)
        { OutError = TEXT("Missing game style widget: ") + FString(Path); return false; }
        const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*Backup, true);
        if (IFileManager::Get().Copy(*(Backup / (Blueprint->GetName() + TEXT(".uasset"))), *Filename) != COPY_OK)
        { OutError = TEXT("Could not back up game widget: ") + Filename; return false; }
        const FString Before = Contract(Blueprint);
        Blueprint->Modify();
        Blueprint->WidgetTree->Modify();
        Apply(Blueprint);
        if (Contract(Blueprint) != Before)
        { OutError = TEXT("A non-visual property changed; nothing saved: ") + Blueprint->GetName(); return false; }
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        if (Results.NumErrors || Blueprint->Status != BS_UpToDate || Contract(Blueprint) != Before)
        { OutError = TEXT("Game style compile/contract check failed; nothing saved: ") + Blueprint->GetName(); return false; }
        Blueprints.Add(Blueprint);
    }
    for (UWidgetBlueprint* Blueprint : Blueprints)
    {
        const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args))
        { OutError = TEXT("Could not save game style widget: ") + Filename; return false; }
    }
    UE_LOG(LogTemp, Display, TEXT("Game styles unified: 8 widgets compiled; graphs, bindings, layout and keypad preserved. Backup: %s"), *Backup);
    return true;
}
