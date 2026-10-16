#include "CasinoVoiceSettingsMigration.h"

#include "CasinoMenuLayoutMigration.h"
#include "CasinoWidgetStyle.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "UI/CasinoVoiceSettingsWidgets.h"

namespace CasinoVoiceSettingsLayout
{
const TCHAR* AssetPath = TEXT("/Game/KCU/WBP_Settings.WBP_Settings");

void ColumnSlot(UWidget* Widget, ESlateSizeRule::Type SizeRule, FMargin Padding = FMargin())
{
    if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Widget->Slot))
    {
        Slot->SetSize(FSlateChildSize(SizeRule));
        Slot->SetPadding(Padding);
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Fill);
    }
}

void FillSize(UWidget* Widget)
{
    if (USizeBoxSlot* Slot = Cast<USizeBoxSlot>(Widget->Slot))
    {
        Slot->SetPadding(FMargin());
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Fill);
    }
}

void Append(UVerticalBox* Column, UWidget* Widget)
{
    if (Widget->GetParent() == Column) return;
    Widget->RemoveFromParent();
    Column->AddChildToVerticalBox(Widget);
}

bool PreserveEmptyRows(UWidgetBlueprint* Blueprint, UVerticalBox* Column,
    const TArray<FName>& RowNames, FString& Error)
{
    for (const FName RowName : RowNames)
    {
        UHorizontalBox* Row = CasinoWidgetStyle::Find<UHorizontalBox>(Blueprint, *RowName.ToString());
        if (!Row || Row->GetChildrenCount() != 0)
        {
            Error = TEXT("An inspected empty settings row now contains controls: ") + RowName.ToString();
            return false;
        }
        const FString WrapperName = TEXT("CasinoVoiceUnused_") + RowName.ToString();
        USizeBox* Bounds = CasinoWidgetStyle::Decoration<USizeBox>(Blueprint, *WrapperName);
        if (Row->GetParent() != Bounds)
        {
            const int32 Index = Column->GetChildIndex(Row);
            Row->RemoveFromParent();
            Bounds->SetContent(Row);
            if (Bounds->GetParent() != Column)
            {
                Bounds->RemoveFromParent();
                Column->InsertChildAt(FMath::Max(0, Index), Bounds);
            }
        }
        Bounds->SetHeightOverride(0.f);
        Bounds->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        FillSize(Row);
        ColumnSlot(Bounds, ESlateSizeRule::Automatic);
    }
    return true;
}

bool ApplyLayout(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* AudioPage = CasinoWidgetStyle::Find<UCanvasPanel>(Blueprint, TEXT("AudioPanel"));
    UCanvasPanel* ControlPage = CasinoWidgetStyle::Find<UCanvasPanel>(Blueprint, TEXT("KeySettingsPanel"));
    UVerticalBox* AudioRows = CasinoWidgetStyle::Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_4"));
    UVerticalBox* ControlRows = CasinoWidgetStyle::Find<UVerticalBox>(Blueprint, TEXT("VerticalBox"));
    UBorder* AudioFrame = CasinoWidgetStyle::Find<UBorder>(Blueprint, TEXT("CasinoSettingsFrame_AudioPanel"));
    UBorder* ControlFrame = CasinoWidgetStyle::Find<UBorder>(Blueprint, TEXT("CasinoSettingsFrame_KeySettingsPanel"));
    USizeBox* MasterRow = CasinoWidgetStyle::Find<USizeBox>(Blueprint, TEXT("CasinoSettingsRow_HorizontalBox_94"));
    UHorizontalBox* MasterControls = CasinoWidgetStyle::Find<UHorizontalBox>(Blueprint, TEXT("HorizontalBox_94"));
    UHorizontalBox* AudioFooter = CasinoWidgetStyle::Find<UHorizontalBox>(Blueprint, TEXT("HorizontalBox_Footer_1"));
    UTextBlock* AudioHint = CasinoWidgetStyle::Find<UTextBlock>(Blueprint, TEXT("CasinoSettingsHint_AudioPanel"));
    UTextBlock* ControlHint = CasinoWidgetStyle::Find<UTextBlock>(Blueprint, TEXT("CasinoSettingsHint_KeySettingsPanel"));
    UTextBlock* OverviewAudio = CasinoWidgetStyle::Find<UTextBlock>(Blueprint, TEXT("CasinoSettingsOverviewHint1"));
    UTextBlock* OverviewControls = CasinoWidgetStyle::Find<UTextBlock>(Blueprint, TEXT("CasinoSettingsOverviewHint2"));
    if (!AudioPage || !ControlPage || !AudioRows || !ControlRows || !AudioFrame || !ControlFrame
        || !MasterRow || !MasterControls || !AudioFooter || !AudioHint || !ControlHint || !OverviewAudio || !OverviewControls
        || AudioRows->GetParent() != AudioFrame || ControlRows->GetParent() != ControlFrame
        || MasterRow->GetParent() != AudioRows || MasterControls->GetParent() != MasterRow)
    {
        Error = TEXT("Settings pages differ from the inspected audio/control layout.");
        return false;
    }
    if (!PreserveEmptyRows(Blueprint, AudioRows,
        {FName(TEXT("HorizontalBox")), FName(TEXT("HorizontalBox_1")), FName(TEXT("HorizontalBox_2")), FName(TEXT("HorizontalBox_3"))}, Error)
        || !PreserveEmptyRows(Blueprint, ControlRows,
        {FName(TEXT("HorizontalBox_6")), FName(TEXT("HorizontalBox_7")), FName(TEXT("HorizontalBox_8"))}, Error)) return false;

    // The master row retains its controls, saved values and existing Apply/Reset graphs.
    MasterRow->SetHeightOverride(56.f);
    ColumnSlot(MasterRow, ESlateSizeRule::Automatic, FMargin(0.f, 0.f, 0.f, 12.f));
    AudioFrame->SetPadding(FMargin(20.f));
    AudioFrame->SetHorizontalAlignment(HAlign_Fill);
    AudioFrame->SetVerticalAlignment(VAlign_Fill);
    if (UBorderSlot* Slot = Cast<UBorderSlot>(AudioRows->Slot))
    {
        Slot->SetPadding(FMargin(20.f));
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Fill);
    }
    CasinoWidgetStyle::Place(AudioPage, AudioFrame, FAnchors(.06f, .22f, .94f, .81f));
    UCasinoVoiceSettingsWidget* Voice = CasinoWidgetStyle::Decoration<UCasinoVoiceSettingsWidget>(Blueprint, TEXT("CasinoVoiceSettingsPanel"));
    Voice->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Append(AudioRows, Voice);
    ColumnSlot(Voice, ESlateSizeRule::Fill);

    CasinoWidgetStyle::Place(AudioPage, AudioFooter, FAnchors(.06f, .84f, .94f, .84f), FMargin(0.f, 0.f, 0.f, 56.f));
    AudioHint->SetText(FText::FromString(TEXT("음성 설정은 즉시 적용·저장됩니다. 마스터 볼륨은 적용 버튼으로 저장하세요.")));
    CasinoWidgetStyle::TextStyle(AudioHint, 16.f, CasinoWidgetStyle::Muted);
    AudioHint->SetAutoWrapText(true);
    CasinoWidgetStyle::Place(AudioPage, AudioHint, FAnchors(.06f, .93f, .94f, .93f), FMargin(0.f, 0.f, 0.f, 32.f), FVector2D::ZeroVector, 5);

    USizeBox* BindingBounds = CasinoWidgetStyle::Decoration<USizeBox>(Blueprint, TEXT("CasinoVoiceKeyBindingBounds"));
    BindingBounds->SetHeightOverride(100.f);
    BindingBounds->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UCasinoVoiceKeyBindingWidget* Binding = CasinoWidgetStyle::Decoration<UCasinoVoiceKeyBindingWidget>(Blueprint, TEXT("CasinoVoiceKeyBindingPanel"));
    Binding->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (Binding->GetParent() != BindingBounds)
    {
        Binding->RemoveFromParent();
        BindingBounds->SetContent(Binding);
    }
    FillSize(Binding);
    Append(ControlRows, BindingBounds);
    ColumnSlot(BindingBounds, ESlateSizeRule::Automatic, FMargin(0.f, 16.f, 0.f, 0.f));
    ControlHint->SetText(FText::FromString(TEXT("음성 단축키는 즉시 저장됩니다. 감도·Y축 반전은 적용 버튼으로 저장하세요.")));
    CasinoWidgetStyle::TextStyle(ControlHint, 16.f, CasinoWidgetStyle::Muted);
    ControlHint->SetAutoWrapText(true);
    OverviewAudio->SetText(FText::FromString(TEXT("마스터 · 음성 · 참가자별 음량")));
    OverviewControls->SetText(FText::FromString(TEXT("마우스 감도 · Y축 반전 · 음성 단축키")));
    return true;
}

bool Backup(FString& Error)
{
    const FString Source = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("KCU/WBP_Settings.uasset"));
    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT("_")
        + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CasinoDealerSetup/VoiceSettingsBackup"), Stamp);
    IFileManager& Files = IFileManager::Get();
    if (!Files.FileExists(*Source) || !Files.MakeDirectory(*Directory, true))
    {
        Error = TEXT("Could not prepare the voice settings UI backup.");
        return false;
    }
    const FString Destination = FPaths::Combine(Directory, TEXT("WBP_Settings.uasset"));
    if (Files.Copy(*Destination, *Source, false) != COPY_OK)
    {
        Error = TEXT("Could not back up the settings UI: ") + Destination;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("Voice settings UI backup: %s"), *Destination);
    return true;
}
}

bool MigrateCasinoVoiceSettings(FString& OutError)
{
    OutError.Reset();
    if (!CasinoVoiceSettingsLayout::Backup(OutError)) return false;
    return MigrateCasinoPresentationWidget(CasinoVoiceSettingsLayout::AssetPath,
        CasinoVoiceSettingsLayout::ApplyLayout, OutError);
}
