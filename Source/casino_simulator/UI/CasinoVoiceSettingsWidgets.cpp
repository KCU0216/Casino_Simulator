#include "UI/CasinoVoiceSettingsWidgets.h"

#include "Online/CasinoOnlineSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/InputKeySelector.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "TimerManager.h"

namespace CasinoVoiceSettingsStyle
{
const FLinearColor VoiceSettingsGold(.76f, .49f, .15f);
const FLinearColor VoiceSettingsIvory(.91f, .86f, .75f);
const FLinearColor VoiceSettingsMuted(.58f, .66f, .72f);
const FLinearColor VoiceSettingsPanel(.023f, .043f, .069f, .78f);
const FLinearColor VoiceSettingsSpeaking(.38f, .84f, .66f);

UTextBlock* VoiceText(UWidgetTree* Tree, const TCHAR* Name, const FString& Caption, int32 Size = 16,
    FLinearColor Color = VoiceSettingsIvory)
{
    UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Result->SetText(FText::FromString(Caption));
    Result->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
    Result->SetColorAndOpacity(Color);
    Result->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Result;
}

UVerticalBoxSlot* VoiceRow(UVerticalBox* Box, UWidget* Content, float Bottom = 6.f)
{
    UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Content);
    Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
    return Slot;
}

UHorizontalBoxSlot* VoiceCell(UWidgetTree* Tree, UHorizontalBox* Box, UWidget* Content, float Width = 0.f)
{
    if (Width > 0.f)
    {
        USizeBox* Bounds = Tree->ConstructWidget<USizeBox>();
        Bounds->SetWidthOverride(Width);
        Bounds->SetContent(Content);
        Content = Bounds;
    }
    UHorizontalBoxSlot* Slot = Box->AddChildToHorizontalBox(Content);
    Slot->SetVerticalAlignment(VAlign_Center);
    if (Width <= 0.f) Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    return Slot;
}

USlider* VoiceSlider(UWidgetTree* Tree, const TCHAR* Name, float Maximum)
{
    USlider* Result = Tree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
    Result->SetMinValue(0.f);
    Result->SetMaxValue(Maximum);
    Result->SetValue(1.f);
    Result->SetStepSize(.01f);
    FSliderStyle Style = Result->GetWidgetStyle();
    Style.NormalBarImage = FSlateRoundedBoxBrush(FLinearColor(.16f, .20f, .26f), 3.f);
    Style.HoveredBarImage = FSlateRoundedBoxBrush(VoiceSettingsMuted, 3.f);
    Style.DisabledBarImage = FSlateRoundedBoxBrush(FLinearColor(.06f, .07f, .08f), 3.f);
    Style.NormalThumbImage = FSlateRoundedBoxBrush(VoiceSettingsGold, 8.f);
    Style.NormalThumbImage.ImageSize = FVector2D(18.f, 24.f);
    Style.HoveredThumbImage = Style.NormalThumbImage;
    Style.HoveredThumbImage.TintColor = FSlateColor(VoiceSettingsIvory);
    Style.DisabledThumbImage = Style.NormalThumbImage;
    Style.DisabledThumbImage.TintColor = FSlateColor(VoiceSettingsMuted);
    Style.BarThickness = 4.f;
    Result->SetWidgetStyle(Style);
    Result->SetSliderBarColor(FLinearColor::White);
    Result->SetSliderHandleColor(FLinearColor::White);
    return Result;
}

FString VoicePercent(float Volume)
{ return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Volume * 100.f)); }

UCheckBox* VoiceCheck(UWidgetTree* Tree, const TCHAR* Name, const FString& Caption)
{
    UCheckBox* Result = Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), Name);
    FCheckBoxStyle Style = Result->GetWidgetStyle();
    const FSlateRoundedBoxBrush Off(FLinearColor(.055f, .085f, .12f), 3.f, VoiceSettingsGold.CopyWithNewOpacity(.5f), 1.f);
    const FSlateRoundedBoxBrush On(VoiceSettingsGold, 3.f);
    Style.SetUncheckedImage(Off).SetUncheckedHoveredImage(Off).SetUncheckedPressedImage(Off)
        .SetCheckedImage(On).SetCheckedHoveredImage(On).SetCheckedPressedImage(On);
    Style.UncheckedImage.ImageSize = FVector2D(17.f, 17.f);
    Style.UncheckedHoveredImage.ImageSize = FVector2D(17.f, 17.f);
    Style.UncheckedPressedImage.ImageSize = FVector2D(17.f, 17.f);
    Style.CheckedImage.ImageSize = FVector2D(17.f, 17.f);
    Style.CheckedHoveredImage.ImageSize = FVector2D(17.f, 17.f);
    Style.CheckedPressedImage.ImageSize = FVector2D(17.f, 17.f);
    Style.SetPadding(FMargin(5.f, 3.f));
    Result->SetWidgetStyle(Style);
    Result->SetContent(VoiceText(Tree, TEXT(""), Caption, 15));
    return Result;
}

UCasinoOnlineSubsystem* VoiceOnlineFor(UUserWidget* Widget)
{
    APlayerController* Owner = Widget ? Widget->GetOwningPlayer() : nullptr;
    UGameInstance* GI = Owner ? Owner->GetGameInstance() : nullptr;
    return Owner && Owner->IsLocalController() && GI ? GI->GetSubsystem<UCasinoOnlineSubsystem>() : nullptr;
}

// WidgetSwitcher hides inactive pages without changing those children's Visibility property.
bool VoiceIsShown(UUserWidget* Widget)
{
    UWidget* Child = Widget;
    TSet<const UWidget*> Seen;
    while (Child && !Seen.Contains(Child))
    {
        Seen.Add(Child);
        if (!Child->IsVisible()) return false;
        if (UPanelWidget* Parent = Child->GetParent())
        {
            if (const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Parent))
                if (Switcher->GetActiveWidget() != Child) return false;
            Child = Parent;
        }
        else
        {
            const UWidgetTree* Tree = Child->GetTypedOuter<UWidgetTree>();
            Child = Tree ? Cast<UUserWidget>(Tree->GetOuter()) : nullptr;
        }
    }
    return true;
}
}

void UCasinoVoiceParticipantRowWidget::NativePreConstruct()
{ Super::NativePreConstruct(); BuildPanel(); }
TSharedRef<SWidget> UCasinoVoiceParticipantRowWidget::RebuildWidget()
{ BuildPanel(); return Super::RebuildWidget(); }

void UCasinoVoiceParticipantRowWidget::BuildPanel()
{
    using namespace CasinoVoiceSettingsStyle;
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    if (WidgetTree->RootWidget) return;
    UBorder* Back = WidgetTree->ConstructWidget<UBorder>();
    Back->SetBrush(FSlateRoundedBoxBrush(VoiceSettingsPanel, 6.f, VoiceSettingsGold.CopyWithNewOpacity(.12f), 1.f));
    Back->SetPadding(FMargin(11.f, 7.f));
    WidgetTree->RootWidget = Back;
    UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
    Back->SetContent(Line);
    UVerticalBox* Identity = WidgetTree->ConstructWidget<UVerticalBox>();
    Identity->SetClipping(EWidgetClipping::ClipToBounds);
    NameText = VoiceText(WidgetTree, TEXT("VoicePlayerName"), TEXT("플레이어"), 16);
    NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    NameText->SetClipping(EWidgetClipping::ClipToBounds);
    StateText = VoiceText(WidgetTree, TEXT("VoicePlayerState"), TEXT("음성 연결 중"), 12, VoiceSettingsMuted);
    VoiceRow(Identity, NameText, 2.f); VoiceRow(Identity, StateText, 0.f);
    VoiceCell(WidgetTree, Line, Identity, 190.f)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    VolumeSlider = VoiceSlider(WidgetTree, TEXT("VoicePlayerVolume"), 2.f);
    VolumeSlider->SetToolTipText(FText::FromString(TEXT("이 플레이어가 내게 들리는 음량")));
    VoiceCell(WidgetTree, Line, VolumeSlider)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    VolumeText = VoiceText(WidgetTree, TEXT("VoicePlayerVolumeText"), TEXT("100%"), 15, VoiceSettingsGold);
    VolumeText->SetJustification(ETextJustify::Right);
    VoiceCell(WidgetTree, Line, VolumeText, 59.f)->SetPadding(FMargin(0.f, 0.f, 11.f, 0.f));
    MuteCheck = VoiceCheck(WidgetTree, TEXT("VoicePlayerMute"), TEXT("음소거"));
    VoiceCell(WidgetTree, Line, MuteCheck, 86.f);
    VolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleVolume);
    VolumeSlider->OnMouseCaptureBegin.AddDynamic(this, &ThisClass::BeginVolumeCapture);
    VolumeSlider->OnControllerCaptureBegin.AddDynamic(this, &ThisClass::BeginVolumeCapture);
    VolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::EndVolumeCapture);
    VolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::EndVolumeCapture);
    MuteCheck->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleMuted);
}

void UCasinoVoiceParticipantRowWidget::UpdateParticipant(UCasinoOnlineSubsystem* Online, APlayerState* Player,
    const FString& VoiceId, bool bTalking, bool bVoiceAvailable)
{
    BuildPanel();
    if (!NameText) return;
    if (VoiceSource.Get() != Online || ParticipantId != VoiceId) EndVolumeCapture();
    VoiceSource = Online;
    ParticipantId = VoiceId;
    TGuardValue<bool> Guard(bUpdating, true);
    NameText->SetText(FText::FromString(Player ? Player->GetPlayerName() : TEXT("플레이어")));
    const bool bConnected = Online && bVoiceAvailable && !VoiceId.IsEmpty() && Online->IsVoiceConnected();
    if (!bConnected) EndVolumeCapture();
    const bool bMuted = Online && Online->IsVoicePlayerMuted(VoiceId);
    StateText->SetText(FText::FromString(!bConnected ? TEXT("음성 연결 중") : bMuted ? TEXT("음소거됨") : bTalking ? TEXT("● 말하는 중") : TEXT("음성 연결됨")));
    StateText->SetColorAndOpacity(bConnected && !bMuted && bTalking
        ? CasinoVoiceSettingsStyle::VoiceSettingsSpeaking : CasinoVoiceSettingsStyle::VoiceSettingsMuted);
    VolumeSlider->SetIsEnabled(bConnected);
    MuteCheck->SetIsEnabled(bConnected);
    if (!bDragging)
    {
        const float Volume = Online ? Online->GetVoicePlayerVolume(VoiceId) : 1.f;
        VolumeSlider->SetValue(Volume);
        VolumeText->SetText(FText::FromString(CasinoVoiceSettingsStyle::VoicePercent(Volume)));
    }
    MuteCheck->SetIsChecked(bMuted);
}

void UCasinoVoiceParticipantRowWidget::HandleVolume(float Value)
{
    if (bUpdating) return;
    VolumeText->SetText(FText::FromString(CasinoVoiceSettingsStyle::VoicePercent(Value)));
    if (VoiceSource.IsValid() && !ParticipantId.IsEmpty()) VoiceSource->SetVoicePlayerVolume(ParticipantId, Value);
}
void UCasinoVoiceParticipantRowWidget::HandleMuted(bool bMuted)
{ if (!bUpdating && VoiceSource.IsValid() && !ParticipantId.IsEmpty()) VoiceSource->SetVoicePlayerMuted(ParticipantId, bMuted); }
void UCasinoVoiceParticipantRowWidget::BeginVolumeCapture()
{ if (!bDragging && VoiceSource.IsValid()) { bDragging = true; VoiceSource->BeginVoicePreferenceEdit(); } }
void UCasinoVoiceParticipantRowWidget::EndVolumeCapture()
{ if (bDragging) { bDragging = false; if (VoiceSource.IsValid()) VoiceSource->EndVoicePreferenceEdit(); } }
void UCasinoVoiceParticipantRowWidget::NativeDestruct()
{ EndVolumeCapture(); VoiceSource.Reset(); Super::NativeDestruct(); }
void UCasinoVoiceParticipantRowWidget::StopEditing() { EndVolumeCapture(); }

void UCasinoVoiceParticipantRowWidget::PrepareDesignerPreview(const FCasinoVoiceParticipant& Sample)
{
    BuildPanel();
    if (!NameText) return;
    TGuardValue<bool> Guard(bUpdating, true);
    NameText->SetText(FText::FromString(Sample.DisplayName));
    StateText->SetText(FText::FromString(Sample.bMuted ? TEXT("음소거됨") : Sample.bTalking ? TEXT("● 말하는 중") : TEXT("음성 연결됨")));
    StateText->SetColorAndOpacity(!Sample.bMuted && Sample.bTalking ? CasinoVoiceSettingsStyle::VoiceSettingsSpeaking : CasinoVoiceSettingsStyle::VoiceSettingsMuted);
    VolumeSlider->SetValue(Sample.Volume);
    VolumeText->SetText(FText::FromString(CasinoVoiceSettingsStyle::VoicePercent(Sample.Volume)));
    MuteCheck->SetIsChecked(Sample.bMuted);
}

void UCasinoVoiceSettingsWidget::NativePreConstruct()
{ Super::NativePreConstruct(); BuildPanel(); }
TSharedRef<SWidget> UCasinoVoiceSettingsWidget::RebuildWidget()
{ BuildPanel(); return Super::RebuildWidget(); }

void UCasinoVoiceSettingsWidget::BuildPanel()
{
    using namespace CasinoVoiceSettingsStyle;
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    if (WidgetTree->RootWidget) return;
    UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
    WidgetTree->RootWidget = Body;
    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    VoiceCell(WidgetTree, Header, VoiceText(WidgetTree, TEXT("VoiceHeading"), TEXT("음성 채팅"), 20, VoiceSettingsGold));
    ConnectionText = VoiceText(WidgetTree, TEXT("VoiceConnectionText"), TEXT("로비·게임에서 연결"), 13, VoiceSettingsMuted);
    VoiceCell(WidgetTree, Header, ConnectionText, 180.f);
    VoiceRow(Body, Header, 10.f);

    auto AddVolume = [&](const TCHAR* Name, const TCHAR* Caption, float Maximum, TObjectPtr<USlider>& OutSlider, TObjectPtr<UTextBlock>& OutText)
    {
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
        VoiceCell(WidgetTree, Line, VoiceText(WidgetTree, TEXT(""), Caption), 154.f);
        OutSlider = VoiceSlider(WidgetTree, Name, Maximum);
        VoiceCell(WidgetTree, Line, OutSlider)->SetPadding(FMargin(4.f, 0.f, 16.f, 0.f));
        OutText = VoiceText(WidgetTree, TEXT(""), TEXT("100%"), 16, VoiceSettingsGold);
        OutText->SetJustification(ETextJustify::Right);
        VoiceCell(WidgetTree, Line, OutText, 62.f);
        VoiceRow(Body, Line, 12.f);
    };
    AddVolume(TEXT("VoiceOutputSlider"), TEXT("음성 전체 음량"), 1.f, OutputSlider, OutputText);
    AddVolume(TEXT("VoiceInputSlider"), TEXT("내 마이크 음량"), 2.f, InputSlider, InputText);
    OutputSlider->SetToolTipText(FText::FromString(TEXT("다른 사람의 목소리가 내게 들리는 전체 음량")));
    InputSlider->SetToolTipText(FText::FromString(TEXT("다른 사람에게 전달되는 내 마이크 입력 음량")));

    UHorizontalBox* Toggles = WidgetTree->ConstructWidget<UHorizontalBox>();
    EnabledCheck = VoiceCheck(WidgetTree, TEXT("VoiceEnabledCheck"), TEXT("음성 채팅 사용"));
    MicrophoneMuteCheck = VoiceCheck(WidgetTree, TEXT("VoiceMicrophoneMuteCheck"), TEXT("내 마이크 음소거"));
    PushToTalkCheck = VoiceCheck(WidgetTree, TEXT("VoicePushToTalkCheck"), TEXT("눌러서 말하기"));
    VoiceCell(WidgetTree, Toggles, EnabledCheck);
    VoiceCell(WidgetTree, Toggles, MicrophoneMuteCheck);
    VoiceCell(WidgetTree, Toggles, PushToTalkCheck);
    VoiceRow(Body, Toggles, 5.f);
    PushToTalkText = VoiceText(WidgetTree, TEXT("VoicePushToTalkHint"), TEXT("V 키를 누르는 동안 말하기 · 조작 설정에서 변경"), 13, VoiceSettingsMuted);
    VoiceRow(Body, PushToTalkText, 14.f);

    VoiceRow(Body, VoiceText(WidgetTree, TEXT("VoiceParticipantsHeading"), TEXT("참가자별 음량"), 17, VoiceSettingsGold), 3.f);
    VoiceRow(Body, VoiceText(WidgetTree, TEXT("VoiceParticipantsHint"), TEXT("내가 듣는 음량에만 적용됩니다. 게임 중 거리별 음량도 유지됩니다."), 12, VoiceSettingsMuted), 8.f);
    UBorder* RosterBack = WidgetTree->ConstructWidget<UBorder>();
    RosterBack->SetPadding(FMargin(8.f));
    RosterBack->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.012f, .023f, .04f, .42f), 6.f, VoiceSettingsGold.CopyWithNewOpacity(.15f), 1.f));
    USizeBox* RosterBounds = WidgetTree->ConstructWidget<USizeBox>();
    RosterBounds->SetMinDesiredHeight(180.f);
    RosterBounds->SetContent(RosterBack);
    VoiceRow(Body, RosterBounds, 0.f)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ParticipantList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("VoiceParticipantList"));
    ParticipantList->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    RosterBack->SetContent(ParticipantList);
    EmptyRosterText = VoiceText(WidgetTree, TEXT("VoiceEmptyRoster"), TEXT("함께 접속한 참가자가 여기에 표시됩니다."), 14, VoiceSettingsMuted);
    EmptyRosterText->SetJustification(ETextJustify::Center);
    Cast<UScrollBoxSlot>(ParticipantList->AddChild(EmptyRosterText))->SetPadding(FMargin(8.f, 18.f));
    OutputSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleOutput);
    InputSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleInput);
    for (USlider* Control : {OutputSlider.Get(), InputSlider.Get()})
    {
        Control->OnMouseCaptureBegin.AddDynamic(this, &ThisClass::BeginVolumeCapture);
        Control->OnControllerCaptureBegin.AddDynamic(this, &ThisClass::BeginVolumeCapture);
        Control->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::EndVolumeCapture);
        Control->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::EndVolumeCapture);
    }
    EnabledCheck->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleEnabled);
    MicrophoneMuteCheck->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleMicrophoneMuted);
    PushToTalkCheck->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandlePushToTalk);
}

void UCasinoVoiceSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct(); BuildPanel();
    VoiceSource = CasinoVoiceSettingsStyle::VoiceOnlineFor(this);
    RefreshVoiceSettings();
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshVoiceSettings, .2f, true);
}

void UCasinoVoiceSettingsWidget::NativeDestruct()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
    EndVolumeCapture();
    for (const auto& Entry : ParticipantRows) if (Entry.Value) Entry.Value->StopEditing();
    if (ParticipantList) ParticipantList->ClearChildren();
    ParticipantRows.Empty();
    // Reopening the retained settings widget restores the same empty-state child.
    if (ParticipantList && EmptyRosterText) ParticipantList->AddChild(EmptyRosterText);
    VoiceSource.Reset();
    Super::NativeDestruct();
}

void UCasinoVoiceSettingsWidget::RefreshVoiceSettings()
{
    using namespace CasinoVoiceSettingsStyle;
    if (bPreview) return;
    if (!VoiceIsShown(this))
    {
        EndVolumeCapture();
        for (const auto& Entry : ParticipantRows) if (Entry.Value) Entry.Value->StopEditing();
        return;
    }
    UCasinoOnlineSubsystem* Online = VoiceOnlineFor(this);
    if (!Online || !OutputSlider) return;
    VoiceSource = Online;
    TGuardValue<bool> Guard(bUpdating, true);
    if (!bDragging)
    {
        const float Output = Online->GetVoiceOutputVolume(), Input = Online->GetVoiceInputVolume();
        OutputSlider->SetValue(Output); InputSlider->SetValue(Input);
        OutputText->SetText(FText::FromString(VoicePercent(Output))); InputText->SetText(FText::FromString(VoicePercent(Input)));
    }
    EnabledCheck->SetIsChecked(Online->IsVoiceEnabled());
    MicrophoneMuteCheck->SetIsChecked(Online->IsMicrophoneMuted());
    PushToTalkCheck->SetIsChecked(Online->IsPushToTalkEnabled());
    PushToTalkText->SetText(FText::FromString(Online->IsPushToTalkEnabled()
        ? FString::Printf(TEXT("%s 키를 누르는 동안 말하기 · 조작 설정에서 변경"), *Online->GetPushToTalkKey().GetDisplayName().ToString())
        : TEXT("열린 마이크 · 음소거하지 않으면 계속 전달됩니다.")));
    ConnectionText->SetText(FText::FromString(!Online->IsVoiceEnabled() ? TEXT("음성 채팅 꺼짐") : Online->IsVoiceConnected() ? TEXT("● 음성 연결됨") : TEXT("음성 연결 대기 중")));
    ConnectionText->SetColorAndOpacity(Online->IsVoiceConnected() && Online->IsVoiceEnabled() ? VoiceSettingsSpeaking : VoiceSettingsMuted);

    const APlayerController* Owner = GetOwningPlayer();
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    TMap<FString, bool> TalkingById;
    for (const FCasinoVoiceParticipant& Entry : Online->GetVoiceParticipants()) TalkingById.Add(Entry.Id, Entry.bTalking);
    TSet<FString> Seen;
    if (GS) for (APlayerState* Player : GS->PlayerArray)
    {
        if (!Player || (Owner && Owner->PlayerState == Player)) continue;
        const FString Key = Player->GetPathName();
        Seen.Add(Key);
        UCasinoVoiceParticipantRowWidget* Entry = ParticipantRows.FindRef(Key);
        if (!Entry)
        {
            Entry = CreateWidget<UCasinoVoiceParticipantRowWidget>(GetOwningPlayer());
            if (!Entry) continue;
            ParticipantRows.Add(Key, Entry);
            Cast<UScrollBoxSlot>(ParticipantList->AddChild(Entry))->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
        }
        const FString Id = Online->GetVoiceIdForPlayer(Player);
        Entry->UpdateParticipant(Online, Player, Id, TalkingById.FindRef(Id), TalkingById.Contains(Id));
    }
    for (auto It = ParticipantRows.CreateIterator(); It; ++It)
        if (!Seen.Contains(It.Key()))
        {
            if (It.Value()) { It.Value()->StopEditing(); It.Value()->RemoveFromParent(); }
            It.RemoveCurrent();
        }
    EmptyRosterText->SetVisibility(ParticipantRows.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UCasinoVoiceSettingsWidget::HandleOutput(float Value)
{
    if (bUpdating) return;
    OutputText->SetText(FText::FromString(CasinoVoiceSettingsStyle::VoicePercent(Value)));
    if (VoiceSource.IsValid()) VoiceSource->SetVoiceOutputVolume(Value);
}
void UCasinoVoiceSettingsWidget::HandleInput(float Value)
{
    if (bUpdating) return;
    InputText->SetText(FText::FromString(CasinoVoiceSettingsStyle::VoicePercent(Value)));
    if (VoiceSource.IsValid()) VoiceSource->SetVoiceInputVolume(Value);
}
void UCasinoVoiceSettingsWidget::HandleEnabled(bool bEnabled)
{ if (!bUpdating && VoiceSource.IsValid()) { VoiceSource->SetVoiceEnabled(bEnabled); RefreshVoiceSettings(); } }
void UCasinoVoiceSettingsWidget::HandleMicrophoneMuted(bool bMuted)
{ if (!bUpdating && VoiceSource.IsValid()) VoiceSource->SetMicrophoneMuted(bMuted); }
void UCasinoVoiceSettingsWidget::HandlePushToTalk(bool bEnabled)
{ if (!bUpdating && VoiceSource.IsValid()) { VoiceSource->SetPushToTalkEnabled(bEnabled); RefreshVoiceSettings(); } }
void UCasinoVoiceSettingsWidget::BeginVolumeCapture()
{ if (!bDragging && VoiceSource.IsValid()) { bDragging = true; VoiceSource->BeginVoicePreferenceEdit(); } }
void UCasinoVoiceSettingsWidget::EndVolumeCapture()
{ if (bDragging) { bDragging = false; if (VoiceSource.IsValid()) VoiceSource->EndVoicePreferenceEdit(); } }

void UCasinoVoiceSettingsWidget::PrepareDesignerPreview(const TArray<FCasinoVoiceParticipant>& Samples)
{
    BuildPanel();
    if (!ParticipantList) return;
    TGuardValue<bool> Guard(bUpdating, true);
    bPreview = true;
    ParticipantList->ClearChildren();
    ParticipantRows.Empty();
    ConnectionText->SetText(FText::FromString(TEXT("● 음성 연결됨")));
    ConnectionText->SetColorAndOpacity(CasinoVoiceSettingsStyle::VoiceSettingsSpeaking);
    EnabledCheck->SetIsChecked(true);
    PushToTalkCheck->SetIsChecked(true);
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
    {
        UCasinoVoiceParticipantRowWidget* Entry = WidgetTree->ConstructWidget<UCasinoVoiceParticipantRowWidget>();
        ParticipantRows.Add(FString::FromInt(Index), Entry);
        Entry->PrepareDesignerPreview(Samples[Index]);
        Cast<UScrollBoxSlot>(ParticipantList->AddChild(Entry))->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
    }
    if (Samples.IsEmpty()) ParticipantList->AddChild(EmptyRosterText);
}

void UCasinoVoiceKeyBindingWidget::NativePreConstruct()
{ Super::NativePreConstruct(); BuildPanel(); }
TSharedRef<SWidget> UCasinoVoiceKeyBindingWidget::RebuildWidget()
{ BuildPanel(); return Super::RebuildWidget(); }

void UCasinoVoiceKeyBindingWidget::BuildPanel()
{
    using namespace CasinoVoiceSettingsStyle;
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    if (WidgetTree->RootWidget) return;
    UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
    WidgetTree->RootWidget = Body;
    UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
    VoiceCell(WidgetTree, Line, VoiceText(WidgetTree, TEXT("PushToTalkBindingTitle"), TEXT("눌러서 말하기"), 17, VoiceSettingsGold));
    KeySelector = WidgetTree->ConstructWidget<UInputKeySelector>(UInputKeySelector::StaticClass(), TEXT("PushToTalkKeySelector"));
    KeySelector->SetSelectedKey(FInputChord(EKeys::V));
    KeySelector->SetAllowGamepadKeys(false); KeySelector->SetAllowModifierKeys(false);
    KeySelector->SetEscapeKeys({EKeys::Escape});
    KeySelector->SetKeySelectionText(FText::FromString(TEXT("키 입력…")));
    KeySelector->SetNoKeySpecifiedText(FText::FromString(TEXT("V")));
    KeySelector->SetMargin(FMargin(18.f, 8.f));
    FButtonStyle KeyStyle = KeySelector->GetButtonStyle();
    KeyStyle.SetNormal(FSlateRoundedBoxBrush(VoiceSettingsPanel, 5.f, VoiceSettingsGold.CopyWithNewOpacity(.55f), 1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.09f, .14f, .19f), 5.f, VoiceSettingsGold, 1.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.13f, .19f, .23f), 5.f, VoiceSettingsGold, 1.f));
    KeySelector->SetButtonStyle(KeyStyle);
    FTextBlockStyle KeyTextStyle = KeySelector->GetTextStyle();
    KeyTextStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16)).SetColorAndOpacity(VoiceSettingsIvory);
    KeySelector->SetTextStyle(KeyTextStyle);
    VoiceCell(WidgetTree, Line, KeySelector, 150.f)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
    UButton* Reset = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PushToTalkResetKey"));
    Reset->SetStyle(KeyStyle);
    Reset->SetContent(VoiceText(WidgetTree, TEXT(""), TEXT("기본값 V"), 14, VoiceSettingsGold));
    VoiceCell(WidgetTree, Line, Reset, 112.f);
    VoiceRow(Body, Line, 8.f);
    HintText = VoiceText(WidgetTree, TEXT("PushToTalkBindingHint"), TEXT("버튼을 누른 뒤 키보드 키 입력 · Esc 취소"), 13, VoiceSettingsMuted);
    VoiceRow(Body, HintText, 0.f);
    KeySelector->OnKeySelected.AddDynamic(this, &ThisClass::HandleSelectedKey);
    KeySelector->OnIsSelectingKeyChanged.AddDynamic(this, &ThisClass::HandleSelectingKey);
    Reset->OnClicked.AddDynamic(this, &ThisClass::HandleResetKey);
}

void UCasinoVoiceKeyBindingWidget::NativeConstruct()
{
    Super::NativeConstruct(); BuildPanel();
    VoiceSource = CasinoVoiceSettingsStyle::VoiceOnlineFor(this);
    RefreshBinding();
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshBinding, .2f, true);
}

void UCasinoVoiceKeyBindingWidget::NativeDestruct()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
    if (bCapturing && VoiceSource.IsValid()) VoiceSource->SetVoiceKeyCaptureActive(false);
    bCapturing = false; VoiceSource.Reset();
    Super::NativeDestruct();
}

void UCasinoVoiceKeyBindingWidget::RefreshBinding()
{
    if (!CasinoVoiceSettingsStyle::VoiceIsShown(this))
    {
        if (bCapturing && VoiceSource.IsValid()) VoiceSource->SetVoiceKeyCaptureActive(false);
        bCapturing = false;
        return;
    }
    VoiceSource = CasinoVoiceSettingsStyle::VoiceOnlineFor(this);
    if (!VoiceSource.IsValid() || !KeySelector || KeySelector->GetIsSelectingKey()) return;
    TGuardValue<bool> Guard(bUpdating, true);
    KeySelector->SetSelectedKey(FInputChord(VoiceSource->GetPushToTalkKey()));
}

void UCasinoVoiceKeyBindingWidget::HandleSelectedKey(FInputChord Chord)
{
    if (bUpdating) return;
    const FKey Key = Chord.Key;
    const bool bValid = UCasinoOnlineSubsystem::IsValidPushToTalkKey(Key);
    if (bValid && VoiceSource.IsValid()) VoiceSource->SetPushToTalkKey(Key);
    if (HintText) HintText->SetText(FText::FromString(bValid ? TEXT("저장됨 · 다른 조작과 겹치지 않는 키를 선택하세요.") : TEXT("일반 키보드 키를 선택하세요. Esc·Tab과 보조키는 사용할 수 없습니다.")));
    if (VoiceSource.IsValid())
    {
        TGuardValue<bool> Guard(bUpdating, true);
        KeySelector->SetSelectedKey(FInputChord(VoiceSource->GetPushToTalkKey()));
    }
}

void UCasinoVoiceKeyBindingWidget::HandleSelectingKey()
{
    if (bUpdating || !KeySelector) return;
    bCapturing = KeySelector->GetIsSelectingKey();
    if (VoiceSource.IsValid()) VoiceSource->SetVoiceKeyCaptureActive(bCapturing);
}

void UCasinoVoiceKeyBindingWidget::HandleResetKey()
{
    if (VoiceSource.IsValid()) VoiceSource->SetPushToTalkKey(EKeys::V);
    RefreshBinding();
    if (HintText) HintText->SetText(FText::FromString(TEXT("기본 키 V로 복원했습니다.")));
}
