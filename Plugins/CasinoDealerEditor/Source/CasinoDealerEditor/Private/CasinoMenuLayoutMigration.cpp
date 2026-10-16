#include "CasinoMenuLayoutMigration.h"
#include "CasinoWidgetStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Font.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FeedbackContext.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace
{
const FLinearColor Background(0.007f, 0.011f, 0.019f, 1.f);
const FLinearColor Panel(0.016f, 0.024f, 0.039f, 1.f);
const FLinearColor Gold(0.76f, 0.49f, 0.15f, 1.f);
const FLinearColor Ivory(0.91f, 0.86f, 0.75f, 1.f);
const FLinearColor Muted(0.39f, 0.46f, 0.55f, 1.f);
const TCHAR* AssetPaths[] = {
    TEXT("/Game/Blackjack/Level/WBP_MainMenu.WBP_MainMenu"),
    TEXT("/Game/Blackjack/Level/WBP_LobbyUI.WBP_LobbyUI"),
    TEXT("/Game/Blackjack/Level/WBP_LobbyPlayerEntry.WBP_LobbyPlayerEntry"),
    TEXT("/Game/Blackjack/Level/WBP_RoomEntry.WBP_RoomEntry")
};
const TCHAR* DayLoopPaths[] = {
    TEXT("/Game/Blackjack/WBP/WBP_DayIntro.WBP_DayIntro"),
    TEXT("/Game/Blackjack/WBP/WBP_DailyPayment.WBP_DailyPayment"),
    TEXT("/Game/Blackjack/WBP/WBP_DayPassed.WBP_DayPassed"),
    TEXT("/Game/Blackjack/WBP/WBP_Result.WBP_Result"),
    TEXT("/Game/1_BluePrint/Widget/WBP_Calculate.WBP_Calculate")
};
const TCHAR* LadderControlPath = TEXT("/Game/KCU/LadderMachine/Widget/WBP_LadderControlWidget.WBP_LadderControlWidget");

FSlateBrush Brush(FLinearColor Color, float Radius = 8.f, FLinearColor Outline = FLinearColor::Transparent)
{
    FSlateBrush Result;
    Result.DrawAs = ESlateBrushDrawType::RoundedBox;
    Result.TintColor = FSlateColor(Color);
    Result.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Outline), 1.f);
    return Result;
}

template <typename T>
T* Find(UWidgetBlueprint* Blueprint, const TCHAR* Name)
{
    return Cast<T>(Blueprint->WidgetTree->FindWidget(FName(Name)));
}

template <typename T>
T* Decoration(UWidgetBlueprint* Blueprint, const TCHAR* Name)
{
    if (T* Existing = Find<T>(Blueprint, Name))
    {
        if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Existing->GetFName()))
            Blueprint->OnVariableAdded(Existing->GetFName());
        return Existing;
    }
    T* Widget = Blueprint->WidgetTree->ConstructWidget<T>(T::StaticClass(), FName(Name));
    Widget->bIsVariable = false;
    // UE5.7 tracks every source widget, even widgets not exposed as variables.
    Blueprint->OnVariableAdded(Widget->GetFName());
    return Widget;
}

bool RegisterSourceWidgetGuids(UWidgetBlueprint* Blueprint, FString& Error)
{
    Blueprint->ForEachSourceWidget([Blueprint](UWidget* Widget)
    {
        if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
            Blueprint->OnVariableAdded(Widget->GetFName());
    });
    for (UWidgetAnimation* Animation : Blueprint->Animations) if (Animation)
    {
        if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Animation->GetFName()))
            Blueprint->OnVariableAdded(Animation->GetFName());
    }
    TSet<FGuid> SeenGuids;
    for (const TPair<FName, FGuid>& Entry : Blueprint->WidgetVariableNameToGuidMap)
    {
        if (!Entry.Value.IsValid() || SeenGuids.Contains(Entry.Value))
        {
            Error = TEXT("Menu asset contains an invalid or duplicated widget GUID: ") + Entry.Key.ToString();
            return false;
        }
        SeenGuids.Add(Entry.Value);
    }
    return true;
}

void TextStyle(UTextBlock* Text, float Size = 20.f, FLinearColor Color = Ivory)
{
    if (!Text) return;
    FSlateFontInfo Font = Text->GetFont();
    if (!Font.FontObject) Font.FontObject = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    Font.Size = Size;
    Text->SetFont(Font);
    Text->SetColorAndOpacity(FSlateColor(Color));
}

void ButtonStyle(UButton* Button, bool Primary = false)
{
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = Brush(Primary ? Gold : Panel, 8.f, Primary ? Gold : Gold * .5f);
    Style.Hovered = Brush(Primary ? FLinearColor(.94f, .67f, .29f, 1.f) : FLinearColor(.045f, .060f, .09f, 1.f), 8.f, Gold);
    Style.Pressed = Brush(Primary ? FLinearColor(.58f, .33f, .075f, 1.f) : FLinearColor(.035f, .043f, .060f, 1.f), 8.f, Gold);
    Style.Disabled = Brush(FLinearColor(.014f, .018f, .025f, 1.f), 8.f, FLinearColor(.06f, .07f, .085f, 1.f));
    Style.NormalPadding = FMargin(20.f, 15.f);
    Style.PressedPadding = FMargin(20.f, 16.f, 20.f, 14.f);
    Style.NormalForeground = FSlateColor(Primary ? Background : Ivory);
    Style.HoveredForeground = Style.NormalForeground;
    Style.PressedForeground = Style.NormalForeground;
    Style.DisabledForeground = FSlateColor(Muted);
    Button->SetStyle(Style);
    if (UTextBlock* Text = Cast<UTextBlock>(Button->GetContent()))
    {
        TextStyle(Text, 20.f, Primary ? Background : Ivory);
        Text->SetColorAndOpacity(FSlateColor::UseForeground());
    }
}

void Restyle(UWidgetBlueprint* Blueprint)
{
    TArray<UWidget*> Widgets;
    Blueprint->WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Widget : Widgets)
        if (UTextBlock* Text = Cast<UTextBlock>(Widget)) TextStyle(Text);
    for (UWidget* Widget : Widgets)
    {
        if (UButton* Button = Cast<UButton>(Widget)) ButtonStyle(Button);
        if (USlider* Slider = Cast<USlider>(Widget))
        {
            FSliderStyle Style = Slider->GetWidgetStyle();
            Style.NormalBarImage = Brush(Muted, 3.f);
            Style.HoveredBarImage = Brush(Ivory, 3.f);
            Style.DisabledBarImage = Brush(FLinearColor(.06f, .07f, .08f, 1.f), 3.f);
            Style.NormalThumbImage = Brush(Gold, 8.f);
            Style.NormalThumbImage.ImageSize = FVector2D(18.f, 24.f);
            Style.HoveredThumbImage = Style.NormalThumbImage;
            Style.HoveredThumbImage.TintColor = FSlateColor(Ivory);
            Style.DisabledThumbImage = Style.NormalThumbImage;
            Style.DisabledThumbImage.TintColor = FSlateColor(Muted);
            Style.BarThickness = 4.f;
            Slider->SetWidgetStyle(Style);
        }
        if (UCheckBox* Check = Cast<UCheckBox>(Widget))
        {
            FCheckBoxStyle Style = Check->GetWidgetStyle();
            FSlateBrush* Images[] = { &Style.UncheckedImage, &Style.UncheckedHoveredImage, &Style.UncheckedPressedImage,
                &Style.CheckedImage, &Style.CheckedHoveredImage, &Style.CheckedPressedImage,
                &Style.UndeterminedImage, &Style.UndeterminedHoveredImage, &Style.UndeterminedPressedImage };
            for (FSlateBrush* Image : Images) Image->ImageSize = FVector2D(26.f, 26.f);
            Style.CheckedImage.TintColor = FSlateColor(Gold);
            Style.CheckedHoveredImage.TintColor = FSlateColor(Ivory);
            Style.ForegroundColor = FSlateColor(Ivory);
            Style.Padding = FMargin(8.f, 4.f);
            Check->SetWidgetStyle(Style);
        }
    }
}

UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors,
    const FMargin& Offsets = FMargin(), FVector2D Alignment = FVector2D::ZeroVector, int32 Order = 0, bool Auto = false)
{
    UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
    if (Widget->GetParent() != Canvas || !Slot)
    {
        Widget->RemoveFromParent();
        Slot = Canvas->AddChildToCanvas(Widget);
    }
    Slot->SetAnchors(Anchors);
    Slot->SetOffsets(Offsets);
    Slot->SetAlignment(Alignment);
    Slot->SetZOrder(Order);
    Slot->SetAutoSize(Auto);
    return Slot;
}

void Label(UWidgetBlueprint* Blueprint, UCanvasPanel* Canvas, const TCHAR* Name, const TCHAR* Value,
    FAnchors Anchors, FMargin Offsets, float Size, FLinearColor Color = Ivory)
{
    UTextBlock* Text = Decoration<UTextBlock>(Blueprint, Name);
    Text->SetText(FText::FromString(Value));
    TextStyle(Text, Size, Color);
    Text->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Canvas, Text, Anchors, Offsets, FVector2D::ZeroVector, 5);
}

void Backdrop(UWidgetBlueprint* Blueprint, UCanvasPanel* Canvas)
{
    UBorder* Back = Decoration<UBorder>(Blueprint, TEXT("CasinoThemeBackground"));
    Back->SetBrush(Brush(Background, 0.f));
    Back->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Canvas, Back, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(), FVector2D::ZeroVector, -100);
    UBorder* Rule = Decoration<UBorder>(Blueprint, TEXT("CasinoThemeHeaderRule"));
    Rule->SetBrush(Brush(Gold * .6f, 0.f));
    Rule->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Canvas, Rule, FAnchors(.06f, .20f, .94f, .20f), FMargin(0.f, 0.f, 0.f, 1.f), FVector2D::ZeroVector, -90);
}

void ScrollList(UWidgetBlueprint* Blueprint, UCanvasPanel* Canvas, UVerticalBox* List,
    const TCHAR* ScrollName, const TCHAR* FrameName, FAnchors Anchors)
{
    UScrollBox* Scroll = Decoration<UScrollBox>(Blueprint, ScrollName);
    UBorder* Frame = Decoration<UBorder>(Blueprint, FrameName);
    Frame->SetBrush(Brush(Panel, 10.f, FLinearColor(.06f, .07f, .09f, 1.f)));
    Frame->SetPadding(FMargin(16.f));
    Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (List->GetParent() != Scroll) { List->RemoveFromParent(); Scroll->AddChild(List); }
    if (Scroll->GetParent() != Frame) { Scroll->RemoveFromParent(); Frame->SetContent(Scroll); }
    Place(Canvas, Frame, Anchors);
}

void MenuButtons(UVerticalBox* Box)
{
    for (UWidget* Child : Box->GetAllChildren())
    {
        if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Child->Slot))
        {
            Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
            Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
            Slot->SetHorizontalAlignment(HAlign_Fill);
            Slot->SetVerticalAlignment(VAlign_Center);
        }
    }
}

bool MainMenu(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Root = Find<UCanvasPanel>(Blueprint, TEXT("MainPanel"));
    UCanvasPanel* Login = Find<UCanvasPanel>(Blueprint, TEXT("LoginPanel"));
    UCanvasPanel* Main = Find<UCanvasPanel>(Blueprint, TEXT("MainMenuPanel"));
    UVerticalBox* LoginActions = Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_2"));
    UVerticalBox* Actions = Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_1"));
    UVerticalBox* Rooms = Find<UVerticalBox>(Blueprint, TEXT("RoomListBox"));
    UTextBlock* Status = Find<UTextBlock>(Blueprint, TEXT("StatusText"));
    UButton* LoginButton = Find<UButton>(Blueprint, TEXT("LoginButton"));
    if (!Root || !Login || !Main || !LoginActions || !Actions || !Rooms || !Status || !LoginButton)
    { Error = TEXT("Main menu layout contract differs from inspected version."); return false; }
    Backdrop(Blueprint, Root);
    // Online OnError/OnChanged writes this existing widget. Keep it outside the
    // switcher so failures remain visible after the login page has closed.
    Place(Root, Status, FAnchors(.06f, .92f, .94f, .92f), FMargin(0.f, 0.f, 0.f, 70.f), FVector2D::ZeroVector, 10);
    TextStyle(Status, 17.f, Gold);
    Status->SetAutoWrapText(true);
    Status->SetClipping(EWidgetClipping::ClipToBounds);
    Status->SetText(FText::GetEmpty());
    UTextBlock* LoginCaption = Decoration<UTextBlock>(Blueprint, TEXT("CasinoLoginButtonCaption"));
    LoginCaption->SetText(FText::FromString(TEXT("로그인")));
    LoginCaption->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (LoginButton->GetContent() != LoginCaption) LoginButton->SetContent(LoginCaption);
    if (UButtonSlot* Slot = Cast<UButtonSlot>(LoginCaption->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center); }
    Label(Blueprint, Login, TEXT("CasinoLoginTitle"), TEXT("CASINO SIMULATOR"), FAnchors(.10f, .24f, .90f, .24f), FMargin(0.f, 0.f, 0.f, 60.f), 42.f, Gold);
    Find<UTextBlock>(Blueprint, TEXT("CasinoLoginTitle"))->SetJustification(ETextJustify::Center);
    Label(Blueprint, Login, TEXT("CasinoLoginSubtitle"), TEXT("로그인 후 방을 만들거나 참가하세요"), FAnchors(.10f, .34f, .90f, .34f), FMargin(0.f, 0.f, 0.f, 40.f), 20.f, Muted);
    Find<UTextBlock>(Blueprint, TEXT("CasinoLoginSubtitle"))->SetJustification(ETextJustify::Center);
    MenuButtons(LoginActions);
    Place(Login, LoginActions, FAnchors(.5f, .50f), FMargin(0.f, 0.f, 360.f, 180.f), FVector2D(.5f, 0.f));
    MenuButtons(Actions);
    Place(Main, Actions, FAnchors(.07f, .36f, .33f, .85f));
    Label(Blueprint, Main, TEXT("CasinoMainTitle"), TEXT("CASINO SIMULATOR"), FAnchors(.06f, .07f, .94f, .07f), FMargin(0.f, 0.f, 0.f, 60.f), 40.f, Gold);
    Label(Blueprint, Main, TEXT("CasinoMainSubtitle"), TEXT("메인 메뉴"), FAnchors(.06f, .15f, .94f, .15f), FMargin(0.f, 0.f, 0.f, 30.f), 18.f, Muted);
    Label(Blueprint, Main, TEXT("CasinoRoomListTitle"), TEXT("참가할 방"), FAnchors(.40f, .25f, .94f, .25f), FMargin(0.f, 0.f, 0.f, 40.f), 24.f, Ivory);
    ScrollList(Blueprint, Main, Rooms, TEXT("CasinoRoomScroll"), TEXT("CasinoRoomFrame"), FAnchors(.40f, .32f, .94f, .83f));
    Label(Blueprint, Main, TEXT("CasinoRoomListHint"), TEXT("방 찾기로 목록을 새로고침하세요"), FAnchors(.40f, .86f, .94f, .86f), FMargin(0.f, 0.f, 0.f, 36.f), 17.f, Muted);
    ButtonStyle(LoginButton, true);
    if (UButton* Button = Find<UButton>(Blueprint, TEXT("LobbyButton"))) ButtonStyle(Button, true);
    return true;
}

bool Lobby(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Root = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
    UVerticalBox* Players = Find<UVerticalBox>(Blueprint, TEXT("PlayerListBox"));
    UVerticalBox* Voice = Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_58"));
    UButton* Ready = Find<UButton>(Blueprint, TEXT("ReadyButton"));
    UButton* Leave = Find<UButton>(Blueprint, TEXT("LeaveButton"));
    if (!Root || !Players || !Voice || !Ready || !Leave)
    { Error = TEXT("Lobby layout contract differs from inspected version."); return false; }
    Backdrop(Blueprint, Root);
    Label(Blueprint, Root, TEXT("CasinoLobbyTitle"), TEXT("LOBBY"), FAnchors(.06f, .07f, .67f, .07f), FMargin(0.f, 0.f, 0.f, 60.f), 40.f, Gold);
    Label(Blueprint, Root, TEXT("CasinoLobbySubtitle"), TEXT("플레이어와 음성 설정"), FAnchors(.06f, .15f, .67f, .15f), FMargin(0.f, 0.f, 0.f, 32.f), 18.f, Muted);
    ScrollList(Blueprint, Root, Players, TEXT("CasinoPlayerScroll"), TEXT("CasinoPlayerFrame"), FAnchors(.06f, .29f, .67f, .82f));
    Label(Blueprint, Root, TEXT("CasinoPlayerListTitle"), TEXT("플레이어  /  볼륨 · 음소거  /  준비 상태"), FAnchors(.06f, .24f, .67f, .24f), FMargin(0.f, 0.f, 0.f, 32.f), 18.f, Muted);
    UBorder* Frame = Decoration<UBorder>(Blueprint, TEXT("CasinoVoiceFrame"));
    Frame->SetBrush(Brush(Panel, 10.f, FLinearColor(.06f, .07f, .09f, 1.f)));
    Frame->SetPadding(FMargin(22.f));
    if (Voice->GetParent() != Frame) { Voice->RemoveFromParent(); Frame->SetContent(Voice); }
    Place(Root, Frame, FAnchors(.71f, .29f, .94f, .71f));
    Label(Blueprint, Root, TEXT("CasinoVoiceTitle"), TEXT("음성 설정"), FAnchors(.71f, .24f, .94f, .24f), FMargin(0.f, 0.f, 0.f, 36.f), 24.f, Ivory);
    for (UWidget* Child : Voice->GetAllChildren()) if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Child->Slot))
    { Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f)); }
    UTextBlock* OutputLabel = Decoration<UTextBlock>(Blueprint, TEXT("CasinoOutputVolumeLabel"));
    OutputLabel->SetText(FText::FromString(TEXT("전체 음성 볼륨"))); TextStyle(OutputLabel, 17.f, Muted);
    OutputLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (OutputLabel->GetParent() != Voice) Voice->InsertChildAt(2, OutputLabel);
    Place(Root, Ready, FAnchors(.71f, .76f, .94f, .76f), FMargin(0.f, 0.f, 0.f, 62.f));
    Place(Root, Leave, FAnchors(.94f, .07f), FMargin(0.f, 0.f, 150.f, 48.f), FVector2D(1.f, 0.f));
    ButtonStyle(Ready, true);
    Label(Blueprint, Root, TEXT("CasinoLobbyHint"), TEXT("모두 준비하면 방장이 게임을 시작할 수 있습니다"), FAnchors(.06f, .88f, .94f, .88f), FMargin(0.f, 0.f, 0.f, 36.f), 18.f, Muted);
    return true;
}

void RowFrame(UWidgetBlueprint* Blueprint, UWidget* Content, const TCHAR* FrameName)
{
    UBorder* Frame = Decoration<UBorder>(Blueprint, FrameName);
    USizeBox* Height = Decoration<USizeBox>(Blueprint, TEXT("CasinoEntryHeight"));
    Frame->SetBrush(Brush(Panel, 8.f, FLinearColor(.065f, .075f, .095f, 1.f)));
    Frame->SetPadding(FMargin(16.f, 12.f));
    Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Height->SetMinDesiredHeight(82.f);
    if (Content->GetParent() != Frame) { Content->RemoveFromParent(); Frame->SetContent(Content); }
    if (Frame->GetParent() != Height) { Frame->RemoveFromParent(); Height->SetContent(Frame); }
    Blueprint->WidgetTree->RootWidget = Height;
}

bool PlayerEntry(UWidgetBlueprint* Blueprint, FString& Error)
{
    UHorizontalBox* Row = Find<UHorizontalBox>(Blueprint, TEXT("HorizontalBox_22"));
    USizeBox* Volume = Find<USizeBox>(Blueprint, TEXT("SizeBox_0"));
    UCheckBox* Mute = Find<UCheckBox>(Blueprint, TEXT("PlayerMuteCheck"));
    if (!Row || !Volume || !Mute) { Error = TEXT("Player entry layout contract differs from inspected version."); return false; }
    Volume->SetWidthOverride(160.f);
    for (UWidget* Child : Row->GetAllChildren()) if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Child->Slot))
    {
        Slot->SetPadding(FMargin(8.f, 4.f));
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetSize(FSlateChildSize(Child->GetName() == TEXT("PlayerNameText") ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    }
    if (!Mute->GetContent())
    {
        UTextBlock* Label = Decoration<UTextBlock>(Blueprint, TEXT("CasinoPlayerMuteLabel"));
        Label->SetText(FText::FromString(TEXT("음소거"))); TextStyle(Label, 17.f); Mute->SetContent(Label);
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    if (UTextBlock* Name = Find<UTextBlock>(Blueprint, TEXT("PlayerNameText")))
    { Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); Name->SetClipping(EWidgetClipping::ClipToBounds); TextStyle(Name, 21.f); }
    if (UTextBlock* State = Find<UTextBlock>(Blueprint, TEXT("ReadyStateText")))
    { State->SetMinDesiredWidth(70.f); TextStyle(State, 17.f, Gold); }
    RowFrame(Blueprint, Row, TEXT("CasinoPlayerEntryFrame"));
    return true;
}

bool RoomEntry(UWidgetBlueprint* Blueprint, FString& Error)
{
    USizeBox* Size = Find<USizeBox>(Blueprint, TEXT("SizeBox_17"));
    UHorizontalBox* Row = Find<UHorizontalBox>(Blueprint, TEXT("HorizontalBox_29"));
    UButton* Join = Find<UButton>(Blueprint, TEXT("JoinButton"));
    UTextBlock* RoomName = Find<UTextBlock>(Blueprint, TEXT("RoomNameText"));
    UTextBlock* PlayerCount = Find<UTextBlock>(Blueprint, TEXT("PlayerCountText"));
    if (!Size || !Row || !Join || !RoomName || !PlayerCount)
    { Error = TEXT("Room entry layout contract differs from inspected version."); return false; }
    Size->SetHeightOverride(64.f);
    for (UWidget* Child : Row->GetAllChildren()) if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Child->Slot))
    {
        Slot->SetPadding(FMargin(8.f, 4.f));
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetSize(FSlateChildSize(Child == RoomName ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    }
    RoomName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    RoomName->SetClipping(EWidgetClipping::ClipToBounds);
    TextStyle(RoomName, 21.f);
    TextStyle(PlayerCount, 17.f, Gold);
    if (UTextBlock* Caption = Cast<UTextBlock>(Join->GetContent())) Caption->SetText(FText::FromString(TEXT("참가")));
    ButtonStyle(Join, true);
    UWidget* Root = Blueprint->WidgetTree->RootWidget;
    if (USizeBox* Existing = Find<USizeBox>(Blueprint, TEXT("CasinoEntryHeight")))
    {
        if (UBorder* Frame = Cast<UBorder>(Existing->GetContent())) Root = Frame->GetContent();
    }
    if (!Root) { Error = TEXT("Room entry has no designer root."); return false; }
    RowFrame(Blueprint, Root, TEXT("CasinoRoomEntryFrame"));
    return true;
}

bool Save(UWidgetBlueprint* Blueprint, FString& Error)
{
    bool Allowed = Blueprint->GetPathName() == LadderControlPath
        || Blueprint->GetPathName() == TEXT("/Game/KCU/WBP_Settings.WBP_Settings")
        || Blueprint->GetPathName() == TEXT("/Game/KCU/WBP_PauseMenu.WBP_PauseMenu");
    for (const TCHAR* Path : AssetPaths) if (Blueprint->GetPathName() == Path) Allowed = true;
    for (const TCHAR* Path : DayLoopPaths) if (Blueprint->GetPathName() == Path) Allowed = true;
    for (const TCHAR* Path : {
        TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackHUD.WBP_BlackjackHUD"),
        TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackCardUI.WBP_BlackjackCardUI"),
        TEXT("/Game/Store/UI/WBP_StoreMain.WBP_StoreMain"),
        TEXT("/Game/Store/UI/WBP_StoreItemCard.WBP_StoreItemCard"),
        TEXT("/Game/Store/UI/WBP_StoreItemEntry.WBP_StoreItemEntry"),
        TEXT("/Game/1_BluePrint/Widget/WBP_Trade.WBP_Trade")})
        if (Blueprint->GetPathName() == Path) Allowed = true;
    if (!Allowed) { Error = TEXT("Attempt to save an asset outside menu migration scope."); return false; }
    const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args))
    { Error = TEXT("Could not save menu asset: ") + Filename; return false; }
    return true;
}

struct FWidgetContract
{
    UClass* Class = nullptr;
    ESlateVisibility Visibility = ESlateVisibility::Visible;
    bool bEnabled = true;
    bool bVariable = false;
    TOptional<float> SliderValue;
    TOptional<ECheckBoxState> CheckState;
};

FString BindingContract(UWidgetBlueprint* Blueprint)
{
    FString Result;
    if (const FProperty* Bindings = FindFProperty<FProperty>(Blueprint->GetClass(), TEXT("Bindings")))
        Bindings->ExportText_InContainer(0, Result, Blueprint, nullptr, Blueprint, PPF_None);
    return Result;
}

FString GraphContract(UWidgetBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<FString> Records;
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) continue;
        Records.Add(Graph->GetName());
        for (UEdGraphNode* Node : Graph->Nodes) if (Node)
            Records.Add(Graph->GetName() + TEXT(":") + Node->NodeGuid.ToString() + TEXT(":") + Node->GetClass()->GetPathName());
    }
    Records.Sort();
    return FString::Join(Records, TEXT("\n"));
}

struct FMenuContract
{
    UClass* Parent = nullptr;
    TMap<FName, FWidgetContract> Widgets;
    FString Bindings;
    FString Graphs;
};

FMenuContract CaptureContract(UWidgetBlueprint* Blueprint)
{
    FMenuContract Result;
    Result.Parent = Blueprint->ParentClass;
    Result.Bindings = BindingContract(Blueprint);
    Result.Graphs = GraphContract(Blueprint);
    Blueprint->WidgetTree->ForEachWidget([&Result](UWidget* Widget)
    {
        FWidgetContract Record;
        Record.Class = Widget->GetClass();
        Record.Visibility = Widget->GetVisibility();
        Record.bEnabled = Widget->GetIsEnabled();
        Record.bVariable = Widget->bIsVariable;
        if (USlider* Slider = Cast<USlider>(Widget)) Record.SliderValue = Slider->GetValue();
        if (UCheckBox* Check = Cast<UCheckBox>(Widget)) Record.CheckState = Check->GetCheckedState();
        Result.Widgets.Add(Widget->GetFName(), Record);
    });
    return Result;
}

bool ValidateContract(UWidgetBlueprint* Blueprint, const FMenuContract& Before, FString& Error)
{
    if (Blueprint->ParentClass != Before.Parent || BindingContract(Blueprint) != Before.Bindings || GraphContract(Blueprint) != Before.Graphs)
    { Error = TEXT("Menu layout migration changed a Blueprint logic contract: ") + Blueprint->GetPathName(); return false; }
    for (const TPair<FName, FWidgetContract>& Entry : Before.Widgets)
    {
        UWidget* Widget = Blueprint->WidgetTree->FindWidget(Entry.Key);
        const FWidgetContract& Previous = Entry.Value;
        if (!Widget || Widget->GetClass() != Previous.Class || Widget->GetVisibility() != Previous.Visibility
            || Widget->GetIsEnabled() != Previous.bEnabled || Widget->bIsVariable != Previous.bVariable)
        { Error = TEXT("A menu widget's interaction state changed: ") + Entry.Key.ToString(); return false; }
        if (Previous.SliderValue.IsSet() && CastChecked<USlider>(Widget)->GetValue() != Previous.SliderValue.GetValue())
        { Error = TEXT("A voice slider's saved value changed: ") + Entry.Key.ToString(); return false; }
        if (Previous.CheckState.IsSet() && CastChecked<UCheckBox>(Widget)->GetCheckedState() != Previous.CheckState.GetValue())
        { Error = TEXT("A voice checkbox's saved state changed: ") + Entry.Key.ToString(); return false; }
    }
    return true;
}

// Include pins, connections, function references and defaults, not just node IDs.
FString SettingsGraphSnapshot(UWidgetBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<FString> Records;
    for (UEdGraph* Graph : Graphs) if (Graph)
    {
        Records.Add(Graph->GetPathName());
        for (UEdGraphNode* Node : Graph->Nodes) if (Node)
        {
            TSet<UObject*> SingleNode;
            SingleNode.Add(Node);
            FString Export;
            FEdGraphUtilities::ExportNodesToText(SingleNode, Export);
            Records.Add(Graph->GetName() + TEXT(":") + Export);
        }
    }
    Records.Sort();
    return FString::Join(Records, TEXT("\n"));
}

FString SettingsDefaultsSnapshot(UWidgetBlueprint* Blueprint)
{
    FString Result;
    const auto Append = [&Result](UObject* Object, const TCHAR* Name)
    {
        if (const FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), Name))
        {
            FString Value;
            Property->ExportText_InContainer(0, Value, Object, nullptr, Object, PPF_None);
            Result += Object->GetName() + TEXT(".") + Name + TEXT("=") + Value + TEXT("\n");
        }
    };
    Append(Blueprint, TEXT("NewVariables"));
    TArray<UWidget*> Widgets;
    Blueprint->WidgetTree->GetAllWidgets(Widgets);
    Widgets.Sort([](const UWidget& A, const UWidget& B) { return A.GetName() < B.GetName(); });
    for (UWidget* Widget : Widgets)
    {
        // Ignore new decorative widgets but capture every interactive control.
        if (!Cast<UButton>(Widget) && !Cast<UComboBoxString>(Widget) && !Cast<USlider>(Widget)
            && !Cast<UCheckBox>(Widget) && !Cast<UWidgetSwitcher>(Widget)) continue;
        const TCHAR* Names[] = { TEXT("DefaultOptions"), TEXT("SelectedOption"), TEXT("Value"),
            TEXT("MinValue"), TEXT("MaxValue"), TEXT("StepSize"), TEXT("MouseUsesStep"),
            TEXT("RequiresControllerLock"), TEXT("CheckedState"), TEXT("ActiveWidgetIndex"),
            TEXT("bIsFocusable"), TEXT("EnableGamepadNavigationMode"), TEXT("Navigation"),
            TEXT("OnClicked"), TEXT("OnValueChanged"), TEXT("OnCheckStateChanged"),
            TEXT("OnSelectionChanged"), TEXT("OnOpening"), TEXT("OnGenerateWidgetEvent") };
        for (const TCHAR* Name : Names) Append(Widget, Name);
    }
    return Result;
}

void SettingsComboStyle(UComboBoxString* Combo)
{
    FComboBoxStyle Style = Combo->GetWidgetStyle();
    FButtonStyle& Button = Style.ComboButtonStyle.ButtonStyle;
    Button.Normal = Brush(Background, 8.f, Gold * .5f);
    Button.Hovered = Brush(FLinearColor(.045f, .060f, .09f, 1.f), 8.f, Gold);
    Button.Pressed = Brush(Panel, 8.f, Gold);
    Button.Disabled = Brush(Panel, 8.f, Muted * .4f);
    Button.NormalForeground = Button.HoveredForeground = Button.PressedForeground = FSlateColor(Ivory);
    Button.DisabledForeground = FSlateColor(Muted);
    Button.NormalPadding = Button.PressedPadding = FMargin(0.f);
    Style.ComboButtonStyle.DownArrowImage.TintColor = FSlateColor(Gold);
    Style.ComboButtonStyle.MenuBorderBrush = Brush(Panel, 8.f, Gold * .5f);
    Style.ComboButtonStyle.MenuBorderPadding = FMargin(8.f);
    Combo->SetWidgetStyle(Style);
    Combo->SetContentPadding(FMargin(16.f, 10.f));
    FTableRowStyle Item = Combo->GetItemStyle();
    Item.EvenRowBackgroundBrush = Item.OddRowBackgroundBrush = Brush(Panel, 4.f);
    Item.EvenRowBackgroundHoveredBrush = Item.OddRowBackgroundHoveredBrush = Brush(FLinearColor(.045f, .060f, .09f, 1.f), 4.f);
    Item.ActiveBrush = Item.InactiveBrush = Brush(Gold * .25f, 4.f);
    Item.ActiveHoveredBrush = Item.InactiveHoveredBrush = Brush(Gold * .4f, 4.f);
    Item.SelectorFocusedBrush = Brush(FLinearColor::Transparent, 4.f, Gold);
    Item.TextColor = Item.SelectedTextColor = FSlateColor(Ivory);
    Combo->SetItemStyle(Item);
    // These UMG properties have no runtime setter; assign only the source asset.
    if (FStructProperty* Property = FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("Font")))
    {
        FSlateFontInfo& Font = *Property->ContainerPtrToValuePtr<FSlateFontInfo>(Combo);
        Font.Size = 20;
    }
    if (FStructProperty* Property = FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("ForegroundColor")))
        *Property->ContainerPtrToValuePtr<FSlateColor>(Combo) = FSlateColor(Ivory);
}

void SettingsRow(UWidgetBlueprint* Blueprint, UVerticalBox* Column, UHorizontalBox* Row)
{
    const bool HasControls = Row->GetChildrenCount() > 0;
    if (!HasControls)
    {
        if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Row->Slot))
        { Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); Slot->SetPadding(FMargin()); }
        return;
    }
    USizeBox* Height = Decoration<USizeBox>(Blueprint, *(TEXT("CasinoSettingsRow_") + Row->GetName()));
    const int32 Index = Column->GetChildIndex(Row);
    Height->SetHeightOverride(64.f);
    if (Row->GetParent() != Height)
    {
        Row->RemoveFromParent();
        Height->SetContent(Row);
        Column->InsertChildAt(Index, Height);
    }
    if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Height->Slot))
    { Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); Slot->SetPadding(FMargin()); Slot->SetHorizontalAlignment(HAlign_Fill); }
    TArray<UWidget*> Children = Row->GetAllChildren();
    for (UWidget* Child : Children)
    {
        if (USlider* Slider = Cast<USlider>(Child))
        {
            USizeBox* Width = Decoration<USizeBox>(Blueprint, *(TEXT("CasinoSettingsControl_") + Slider->GetName()));
            const int32 ChildIndex = Row->GetChildIndex(Slider);
            Slider->RemoveFromParent(); Width->SetContent(Slider); Row->InsertChildAt(ChildIndex, Width);
            Width->SetWidthOverride(360.f); Width->SetHeightOverride(48.f);
        }
        if (USizeBox* Box = Cast<USizeBox>(Child))
        {
            Box->ClearMinDesiredWidth(); Box->ClearMinDesiredHeight();
            Box->ClearMaxDesiredWidth(); Box->ClearMaxDesiredHeight();
            Box->SetHeightOverride(Box->GetContent() ? 48.f : 0.f);
            Box->SetWidthOverride(Cast<UTextBlock>(Box->GetContent()) ? 250.f : Box->GetContent() ? 360.f : 0.f);
            if (USizeBoxSlot* Slot = Cast<USizeBoxSlot>(Box->GetContent() ? Box->GetContent()->Slot : nullptr))
            { Slot->SetVerticalAlignment(VAlign_Center); Slot->SetHorizontalAlignment(Cast<UCheckBox>(Box->GetContent()) ? HAlign_Left : HAlign_Fill); }
        }
        if (USpacer* Spacer = Cast<USpacer>(Child)) Spacer->SetSize(FVector2D(24.f, 0.f));
    }
    for (UWidget* Child : Row->GetAllChildren()) if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Child->Slot))
    {
        Slot->SetSize(FSlateChildSize(Cast<USpacer>(Child) ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
        Slot->SetPadding(FMargin()); Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetHorizontalAlignment(HAlign_Fill);
    }
}

bool SettingsPage(UWidgetBlueprint* Blueprint, const TCHAR* PageName, const TCHAR* ColumnName,
    const TCHAR* FooterName, const TCHAR* Title, const TCHAR* Subtitle, FString& Error)
{
    UCanvasPanel* Page = Find<UCanvasPanel>(Blueprint, PageName);
    UVerticalBox* Column = Find<UVerticalBox>(Blueprint, ColumnName);
    UHorizontalBox* Footer = Find<UHorizontalBox>(Blueprint, FooterName);
    if (!Page || !Column || !Footer) { Error = TEXT("Settings page layout differs: ") + FString(PageName); return false; }
    Label(Blueprint, Page, *(TEXT("CasinoSettingsTitle_") + FString(PageName)), Title,
        FAnchors(.06f, .07f, .94f, .07f), FMargin(0.f, 0.f, 0.f, 60.f), 40.f, Gold);
    Label(Blueprint, Page, *(TEXT("CasinoSettingsSubtitle_") + FString(PageName)), Subtitle,
        FAnchors(.06f, .15f, .94f, .15f), FMargin(0.f, 0.f, 0.f, 32.f), 18.f, Muted);
    Place(Page, Footer, FAnchors(.06f, .79f, .94f, .79f), FMargin(0.f, 0.f, 0.f, 56.f));
    for (UWidget* Child : Footer->GetAllChildren())
    {
        if (USizeBox* Box = Cast<USizeBox>(Child)) { Box->SetWidthOverride(160.f); Box->SetHeightOverride(56.f); }
        if (USpacer* Spacer = Cast<USpacer>(Child)) Spacer->SetSize(FVector2D(16.f, 0.f));
        if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Child->Slot))
        {
            Slot->SetSize(FSlateChildSize(Cast<USpacer>(Child) ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
            Slot->SetVerticalAlignment(VAlign_Center);
            Slot->SetPadding(FMargin(0.f, 0.f, Cast<USpacer>(Child) ? 0.f : 12.f, 0.f));
        }
    }
    UBorder* Frame = Decoration<UBorder>(Blueprint, *(TEXT("CasinoSettingsFrame_") + FString(PageName)));
    Frame->SetBrush(Brush(Panel, 10.f, FLinearColor(.06f, .07f, .09f, 1.f)));
    Frame->SetPadding(FMargin(28.f)); Frame->SetVerticalAlignment(VAlign_Top);
    Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (Column->GetParent() != Frame) { Column->RemoveFromParent(); Frame->SetContent(Column); }
    Place(Page, Frame, FAnchors(.06f, .27f, .94f, .72f));
    for (UWidget* Child : Column->GetAllChildren())
    {
        UHorizontalBox* Row = Cast<UHorizontalBox>(Child);
        if (!Row) if (USizeBox* Height = Cast<USizeBox>(Child)) Row = Cast<UHorizontalBox>(Height->GetContent());
        if (Row) SettingsRow(Blueprint, Column, Row);
    }
    Label(Blueprint, Page, *(TEXT("CasinoSettingsHint_") + FString(PageName)), TEXT("변경한 설정은 적용 버튼으로 저장하세요"),
        FAnchors(.06f, .88f, .94f, .88f), FMargin(0.f, 0.f, 0.f, 32.f), 17.f, Muted);
    return true;
}

bool SettingsLayout(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Root = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
    UCanvasPanel* PanelRoot = Find<UCanvasPanel>(Blueprint, TEXT("SettingsPanel"));
    UCanvasPanel* Menu = Find<UCanvasPanel>(Blueprint, TEXT("SettingsMenuPanel"));
    UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(Blueprint, TEXT("SettingsSwitcher"));
    UVerticalBox* Categories = Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_0"));
    UButton* Back = Find<UButton>(Blueprint, TEXT("BackButton_Settings"));
    if (!Root || !PanelRoot || !Menu || !Switcher || !Categories || !Back || Switcher->GetChildrenCount() != 4)
    { Error = TEXT("Settings menu layout contract differs."); return false; }
    Backdrop(Blueprint, Root);
    Place(Root, PanelRoot, FAnchors(0.f, 0.f, 1.f, 1.f));
    Place(PanelRoot, Switcher, FAnchors(0.f, 0.f, 1.f, 1.f));
    Label(Blueprint, Menu, TEXT("CasinoSettingsMenuTitle"), TEXT("SETTINGS"),
        FAnchors(.06f, .07f, .94f, .07f), FMargin(0.f, 0.f, 0.f, 60.f), 40.f, Gold);
    Label(Blueprint, Menu, TEXT("CasinoSettingsMenuSubtitle"), TEXT("환경 설정"),
        FAnchors(.06f, .15f, .94f, .15f), FMargin(0.f, 0.f, 0.f, 32.f), 18.f, Muted);
    Place(Menu, Back, FAnchors(.94f, .07f), FMargin(0.f, 0.f, 160.f, 48.f), FVector2D(1.f, 0.f));
    MenuButtons(Categories);
    Place(Menu, Categories, FAnchors(.07f, .32f, .33f, .75f));
    UBorder* Frame = Decoration<UBorder>(Blueprint, TEXT("CasinoSettingsOverviewFrame"));
    Frame->SetBrush(Brush(Panel, 10.f, FLinearColor(.06f, .07f, .09f, 1.f)));
    Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Menu, Frame, FAnchors(.40f, .29f, .94f, .78f), FMargin(), FVector2D::ZeroVector, -1);
    const TCHAR* Titles[] = { TEXT("그래픽"), TEXT("오디오"), TEXT("조작") };
    const TCHAR* Hints[] = { TEXT("화면 모드 · 해상도 · 그래픽 품질 · 프레임 제한"),
        TEXT("마스터 볼륨"), TEXT("마우스 감도 · Y축 반전") };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const float Y = .34f + Index * .14f;
        Label(Blueprint, Menu, *FString::Printf(TEXT("CasinoSettingsOverviewTitle%d"), Index), Titles[Index],
            FAnchors(.43f, Y, .91f, Y), FMargin(0.f, 0.f, 0.f, 36.f), 24.f, Ivory);
        Label(Blueprint, Menu, *FString::Printf(TEXT("CasinoSettingsOverviewHint%d"), Index), Hints[Index],
            FAnchors(.43f, Y + .045f, .91f, Y + .045f), FMargin(0.f, 0.f, 0.f, 40.f), 17.f, Muted);
        Find<UTextBlock>(Blueprint, *FString::Printf(TEXT("CasinoSettingsOverviewHint%d"), Index))->SetAutoWrapText(true);
    }
    if (!SettingsPage(Blueprint, TEXT("GraphicPanel"), TEXT("VerticalBox_3"), TEXT("HorizontalBox_Footer"),
        TEXT("GRAPHICS"), TEXT("그래픽 설정"), Error)
        || !SettingsPage(Blueprint, TEXT("AudioPanel"), TEXT("VerticalBox_4"), TEXT("HorizontalBox_Footer_1"),
        TEXT("AUDIO"), TEXT("오디오 설정"), Error)
        || !SettingsPage(Blueprint, TEXT("KeySettingsPanel"), TEXT("VerticalBox"), TEXT("HorizontalBox_Footer_2"),
        TEXT("CONTROLS"), TEXT("조작 설정"), Error)) return false;
    TArray<UWidget*> Widgets;
    Blueprint->WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Widget : Widgets)
    {
        if (UComboBoxString* Combo = Cast<UComboBoxString>(Widget)) SettingsComboStyle(Combo);
        if (UButton* Button = Cast<UButton>(Widget))
            ButtonStyle(Button, Button->GetName().EndsWith(TEXT("Apply")));
    }
    return true;
}

bool PauseMenuLayout(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Root = Find<UCanvasPanel>(Blueprint, TEXT("CanvasPanel_11"));
    UCanvasPanel* Menu = Find<UCanvasPanel>(Blueprint, TEXT("CanvasPanel_53"));
    UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(Blueprint, TEXT("WidgetSwitcher_0"));
    UVerticalBox* Actions = Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_0"));
    UButton* Continue = Find<UButton>(Blueprint, TEXT("Button_Continue"));
    UWidget* Settings = Find<UWidget>(Blueprint, TEXT("WBP_Settings"));
    if (!Root || !Menu || !Switcher || !Actions || !Continue || !Settings
        || Switcher->GetChildrenCount() != 2 || Switcher->GetChildAt(0) != Menu || Switcher->GetChildAt(1) != Settings)
    { Error = TEXT("In-game menu layout contract differs from the inspected version."); return false; }
    Place(Root, Switcher, FAnchors(0.f, 0.f, 1.f, 1.f));
    // Only the background brushes are translucent; controls remain legible.
    UBorder* Shade = Decoration<UBorder>(Blueprint, TEXT("CasinoPauseBackdrop"));
    FLinearColor ShadeColor = Background; ShadeColor.A = .16f;
    Shade->SetBrush(Brush(ShadeColor, 0.f));
    Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Root, Shade, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(), FVector2D::ZeroVector, -100);
    UBorder* Frame = Decoration<UBorder>(Blueprint, TEXT("CasinoPauseFrame"));
    FLinearColor FrameColor = Panel; FrameColor.A = .78f;
    Frame->SetBrush(Brush(FrameColor, 10.f, Gold * .5f));
    Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Menu, Frame, FAnchors(.04f, .10f, .34f, .90f), FMargin(), FVector2D::ZeroVector, -1);
    Label(Blueprint, Menu, TEXT("CasinoPauseTitle"), TEXT("GAME MENU"),
        FAnchors(.065f, .16f, .315f, .16f), FMargin(0.f, 0.f, 0.f, 56.f), 36.f, Gold);
    Label(Blueprint, Menu, TEXT("CasinoPauseSubtitle"), TEXT("게임 메뉴"),
        FAnchors(.065f, .23f, .315f, .23f), FMargin(0.f, 0.f, 0.f, 30.f), 18.f, Muted);
    MenuButtons(Actions);
    Place(Menu, Actions, FAnchors(.065f, .34f, .315f, .80f));
    ButtonStyle(Continue, true);
    Label(Blueprint, Menu, TEXT("CasinoPauseHint"), TEXT("Tab · 메뉴 닫기"),
        FAnchors(.065f, .84f, .315f, .84f), FMargin(0.f, 0.f, 0.f, 32.f), 17.f, Muted);
    return true;
}
}

bool MigrateCasinoMenuLayouts(FString& OutError)
{
    OutError.Reset();
    const SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
    const int32 ErrorsBefore = GWarn ? GWarn->GetNumErrors() : 0;
    TArray<UWidgetBlueprint*> Blueprints;
    TArray<FMenuContract> OriginalContracts;
    for (const TCHAR* Path : AssetPaths)
    {
        UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, Path);
        if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->GeneratedClass)
        { OutError = TEXT("Could not load menu WidgetBlueprint: ") + FString(Path); return false; }
        if (!RegisterSourceWidgetGuids(Blueprint, OutError)) return false;
        Blueprints.Add(Blueprint); OriginalContracts.Add(CaptureContract(Blueprint));
    }
    for (UWidgetBlueprint* Blueprint : Blueprints) { Blueprint->Modify(); Blueprint->WidgetTree->Modify(); Restyle(Blueprint); }
    if (!MainMenu(Blueprints[0], OutError) || !Lobby(Blueprints[1], OutError)
        || !PlayerEntry(Blueprints[2], OutError) || !RoomEntry(Blueprints[3], OutError)) return false;
    for (int32 Index = 0; Index < Blueprints.Num(); ++Index)
    {
        UWidgetBlueprint* Blueprint = Blueprints[Index];
        if (!RegisterSourceWidgetGuids(Blueprint, OutError)) return false;
        if (!ValidateContract(Blueprint, OriginalContracts[Index], OutError)) return false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        if (Results.NumErrors > 0 || Blueprint->Status != BS_UpToDate)
        { OutError = TEXT("Menu Blueprint did not compile cleanly: ") + Blueprint->GetPathName(); return false; }
        if (!ValidateContract(Blueprint, OriginalContracts[Index], OutError)) return false;
    }
    if (FDebug::GetNumEnsureFailures() != EnsuresBefore || (GWarn && GWarn->GetNumErrors() > ErrorsBefore))
    {
        OutError = TEXT("Menu migration logged an ensure or engine error; no menu assets were saved.");
        return false;
    }
    // Validate all four before the first save; no game/voice graphs are edited.
    for (UWidgetBlueprint* Blueprint : Blueprints) if (!Save(Blueprint, OutError)) return false;
    return true;
}

bool MigrateCasinoSettingsLayout(FString& OutError)
{
    OutError.Reset();
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/KCU/WBP_Settings.WBP_Settings"));
    if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->GeneratedClass)
    { OutError = TEXT("Could not load settings WidgetBlueprint."); return false; }
    const SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
    const int32 ErrorsBefore = GWarn ? GWarn->GetNumErrors() : 0;
    const FMenuContract Before = CaptureContract(Blueprint);
    const FString GraphsBefore = SettingsGraphSnapshot(Blueprint);
    const FString DefaultsBefore = SettingsDefaultsSnapshot(Blueprint);
    const TMap<FName, FGuid> GuidsBefore = Blueprint->WidgetVariableNameToGuidMap;
    Blueprint->Modify(); Blueprint->WidgetTree->Modify();
    Restyle(Blueprint);
    if (!SettingsLayout(Blueprint, OutError) || !RegisterSourceWidgetGuids(Blueprint, OutError)) return false;
    const auto Validate = [&]()
    {
        if (!ValidateContract(Blueprint, Before, OutError)) return false;
        if (SettingsGraphSnapshot(Blueprint) != GraphsBefore || SettingsDefaultsSnapshot(Blueprint) != DefaultsBefore)
        { OutError = TEXT("Settings graph wiring or control defaults changed; asset was not saved."); return false; }
        for (const TPair<FName, FGuid>& Entry : GuidsBefore)
            if (Blueprint->WidgetVariableNameToGuidMap.FindRef(Entry.Key) != Entry.Value)
            { OutError = TEXT("Existing settings widget GUID changed: ") + Entry.Key.ToString(); return false; }
        return true;
    };
    if (!Validate()) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    if (Results.NumErrors > 0 || Blueprint->Status != BS_UpToDate)
    { OutError = TEXT("Settings Blueprint did not compile cleanly."); return false; }
    if (!Validate()) return false;
    if (FDebug::GetNumEnsureFailures() != EnsuresBefore || (GWarn && GWarn->GetNumErrors() > ErrorsBefore))
    { OutError = TEXT("Settings migration logged an engine error; asset was not saved."); return false; }
    return Save(Blueprint, OutError);
}

bool MigrateCasinoPauseMenuLayout(FString& OutError)
{
    OutError.Reset();
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/KCU/WBP_PauseMenu.WBP_PauseMenu"));
    if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->GeneratedClass)
    { OutError = TEXT("Could not load in-game menu WidgetBlueprint."); return false; }
    const SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
    const int32 ErrorsBefore = GWarn ? GWarn->GetNumErrors() : 0;
    const FMenuContract Before = CaptureContract(Blueprint);
    const FString GraphsBefore = SettingsGraphSnapshot(Blueprint);
    const FString DefaultsBefore = SettingsDefaultsSnapshot(Blueprint);
    const TMap<FName, FGuid> GuidsBefore = Blueprint->WidgetVariableNameToGuidMap;
    Blueprint->Modify(); Blueprint->WidgetTree->Modify();
    Restyle(Blueprint);
    if (!PauseMenuLayout(Blueprint, OutError) || !RegisterSourceWidgetGuids(Blueprint, OutError)) return false;
    const auto Validate = [&]()
    {
        if (!ValidateContract(Blueprint, Before, OutError)) return false;
        if (SettingsGraphSnapshot(Blueprint) != GraphsBefore || SettingsDefaultsSnapshot(Blueprint) != DefaultsBefore)
        { OutError = TEXT("In-game menu logic or control defaults changed; asset was not saved."); return false; }
        for (const TPair<FName, FGuid>& Entry : GuidsBefore)
            if (Blueprint->WidgetVariableNameToGuidMap.FindRef(Entry.Key) != Entry.Value)
            { OutError = TEXT("Existing in-game menu widget GUID changed: ") + Entry.Key.ToString(); return false; }
        return true;
    };
    if (!Validate()) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    if (Results.NumErrors > 0 || Blueprint->Status != BS_UpToDate)
    { OutError = TEXT("In-game menu Blueprint did not compile cleanly."); return false; }
    if (!Validate()) return false;
    if (FDebug::GetNumEnsureFailures() != EnsuresBefore || (GWarn && GWarn->GetNumErrors() > ErrorsBefore))
    { OutError = TEXT("In-game menu migration logged an engine error; asset was not saved."); return false; }
    return Save(Blueprint, OutError);
}

bool MigrateCasinoPresentationWidget(const TCHAR* AssetPath,
    TFunctionRef<bool(UWidgetBlueprint*, FString&)> ApplyLayout, FString& OutError,
    const TSet<FName>& AddedBindingTargets)
{
    OutError.Reset();
    bool Allowed = FString(AssetPath) == LadderControlPath
        || FString(AssetPath) == TEXT("/Game/KCU/WBP_Settings.WBP_Settings");
    for (const TCHAR* Path : DayLoopPaths) if (FString(AssetPath) == Path) Allowed = true;
    for (const TCHAR* Path : {
        TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackHUD.WBP_BlackjackHUD"),
        TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackCardUI.WBP_BlackjackCardUI"),
        TEXT("/Game/Store/UI/WBP_StoreMain.WBP_StoreMain"),
        TEXT("/Game/Store/UI/WBP_StoreItemCard.WBP_StoreItemCard"),
        TEXT("/Game/Store/UI/WBP_StoreItemEntry.WBP_StoreItemEntry"),
        TEXT("/Game/1_BluePrint/Widget/WBP_Trade.WBP_Trade")})
        if (FString(AssetPath) == Path) Allowed = true;
    if (!Allowed) { OutError = TEXT("Presentation asset is outside the requested scope."); return false; }
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, AssetPath);
    if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->GeneratedClass)
    { OutError = TEXT("Could not load presentation WidgetBlueprint: ") + FString(AssetPath); return false; }
    const SIZE_T EnsuresBefore = FDebug::GetNumEnsureFailures();
    const int32 ErrorsBefore = GWarn ? GWarn->GetNumErrors() : 0;
    const FMenuContract Before = CaptureContract(Blueprint);
    const FString GraphsBefore = SettingsGraphSnapshot(Blueprint);
    const FString DefaultsBefore = SettingsDefaultsSnapshot(Blueprint);
    const TMap<FName, FGuid> GuidsBefore = Blueprint->WidgetVariableNameToGuidMap;
    Blueprint->Modify(); Blueprint->WidgetTree->Modify();
    if (!ApplyLayout(Blueprint, OutError) || !RegisterSourceWidgetGuids(Blueprint, OutError)) return false;
    const auto Validate = [&]()
    {
        // New decorative labels may bind to existing item data. Every original
        // binding must still serialize exactly as it did before this operation.
        TArray<FDelegateEditorBinding> ExistingBindings;
        for (const FDelegateEditorBinding& Binding : Blueprint->Bindings)
        {
            if (!Before.Widgets.Contains(FName(Binding.ObjectName)))
            {
                if (!AddedBindingTargets.Contains(FName(Binding.ObjectName)))
                { OutError = TEXT("Unexpected new presentation binding: ") + Binding.ObjectName; return false; }
            }
            else ExistingBindings.Add(Binding);
        }
        FString SerializedBindings;
        if (const FArrayProperty* Property = FindFProperty<FArrayProperty>(Blueprint->GetClass(), TEXT("Bindings")))
            Property->ExportTextItem_Direct(SerializedBindings, &ExistingBindings, nullptr, Blueprint, PPF_None);
        if (SerializedBindings != Before.Bindings)
        { OutError = TEXT("Original presentation bindings changed."); return false; }
        FMenuContract Expected = Before;
        Expected.Bindings = BindingContract(Blueprint);
        if (!ValidateContract(Blueprint, Expected, OutError)) return false;
        if (SettingsGraphSnapshot(Blueprint) != GraphsBefore || SettingsDefaultsSnapshot(Blueprint) != DefaultsBefore)
        { OutError = TEXT("Presentation graph wiring or control defaults changed; asset was not saved."); return false; }
        for (const TPair<FName, FGuid>& Entry : GuidsBefore)
            if (Blueprint->WidgetVariableNameToGuidMap.FindRef(Entry.Key) != Entry.Value)
            { OutError = TEXT("Existing presentation widget GUID changed: ") + Entry.Key.ToString(); return false; }
        return true;
    };
    if (!Validate()) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    if (Results.NumErrors > 0 || Blueprint->Status != BS_UpToDate)
    { OutError = TEXT("Presentation Blueprint did not compile cleanly: ") + FString(AssetPath); return false; }
    if (!Validate()) return false;
    if (FDebug::GetNumEnsureFailures() != EnsuresBefore || (GWarn && GWarn->GetNumErrors() > ErrorsBefore))
    { OutError = TEXT("Presentation migration logged an engine error; asset was not saved."); return false; }
    return Save(Blueprint, OutError);
}

namespace
{
void DayRow(UWidget* Widget, float Bottom = 18.f)
{
    if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Widget->Slot))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
    }
}

void DayText(UWidgetBlueprint* Blueprint, const TCHAR* Name, int32 Size, FLinearColor Color)
{
    if (UTextBlock* Text = Find<UTextBlock>(Blueprint, Name))
    {
        TextStyle(Text, Size, Color);
        Text->SetJustification(ETextJustify::Center);
        Text->SetAutoWrapText(true);
        DayRow(Text, Size >= 40 ? 24.f : 18.f);
    }
}

bool StyleDayLoop(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Canvas = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
    const FString Name = Blueprint->GetName();
    const bool Payment = Name == TEXT("WBP_DailyPayment");
    const bool Result = Name == TEXT("WBP_Result");
    const TCHAR* BoxName = Payment ? TEXT("VerticalBox_51") : Result ? TEXT("VerticalBox_33")
        : Name == TEXT("WBP_DayIntro") ? TEXT("VerticalBox_45") : TEXT("VerticalBox_18");
    UVerticalBox* Rows = Find<UVerticalBox>(Blueprint, BoxName);
    if (!Canvas || !Rows) { Error = TEXT("Day screen has an unexpected widget tree: ") + Name; return false; }
    Restyle(Blueprint);
    // Retain existing designer objects while removing the full-screen menu presentation.
    for (const TCHAR* BackgroundName : {TEXT("CasinoThemeBackground"), TEXT("CasinoThemeHeaderRule")})
        if (UBorder* BackgroundWidget = Find<UBorder>(Blueprint, BackgroundName))
            BackgroundWidget->SetBrush(Brush(FLinearColor::Transparent, 0.f));
    for (const TCHAR* HeaderName : {TEXT("CasinoDayBrand"), TEXT("CasinoDaySection")})
        if (UTextBlock* Header = Find<UTextBlock>(Blueprint, HeaderName)) Header->SetText(FText::GetEmpty());

    for (UWidget* Row : Rows->GetAllChildren()) DayRow(Row);
    UBorder* Frame = Decoration<UBorder>(Blueprint, TEXT("CasinoDayFrame"));
    FLinearColor FrameColor = Panel; FrameColor.A = .78f;
    Frame->SetBrush(Brush(FrameColor, 10.f, Gold * .5f));
    Frame->SetBrushColor(FLinearColor::White);
    Frame->SetPadding(FMargin(40.f, 32.f, 40.f, 20.f));
    Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Rows->RemoveFromParent();
    Frame->SetContent(Rows);
    Rows->SetRenderTransform(FWidgetTransform());
    Frame->SetHorizontalAlignment(HAlign_Fill);
    Frame->SetVerticalAlignment(VAlign_Center);
    USizeBox* Bounds = Decoration<USizeBox>(Blueprint, TEXT("CasinoDayBounds"));
    Bounds->SetWidthOverride(Payment ? 680.f : 600.f);
    Frame->RemoveFromParent();
    Bounds->SetContent(Frame);
    UScaleBox* Scale = Decoration<UScaleBox>(Blueprint, TEXT("CasinoDayScale"));
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Bounds->RemoveFromParent();
    Scale->SetContent(Bounds);
    if (UScaleBoxSlot* Slot = Cast<UScaleBoxSlot>(Bounds->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center); }
    Place(Canvas, Scale, FAnchors(.08f, .08f, .92f, .92f), FMargin(), FVector2D::ZeroVector, 10);
    DayText(Blueprint, TEXT("DayText"), 40, Gold);
    DayText(Blueprint, TEXT("TargetText"), 22, Ivory);
    DayText(Blueprint, TEXT("TitleText"), 36, Gold);
    DayText(Blueprint, TEXT("SummaryText"), 22, Ivory);
    DayText(Blueprint, TEXT("HintText"), 18, Muted);
    DayText(Blueprint, TEXT("ResultTitleText"), 36, Gold);
    DayText(Blueprint, TEXT("ResultSummaryText"), 20, Ivory);
    if (Result)
    {
        for (const TCHAR* ButtonName : {TEXT("RestartButton"), TEXT("MainMenuButton")})
        {
            UButton* Button = Find<UButton>(Blueprint, ButtonName);
            if (!Button) { Error = TEXT("Result button missing."); return false; }
            if (!Button->GetContent())
            {
                UTextBlock* Caption = Decoration<UTextBlock>(Blueprint,
                    Button == Find<UButton>(Blueprint, TEXT("RestartButton")) ? TEXT("CasinoRestartCaption") : TEXT("CasinoMenuCaption"));
                Caption->SetText(FText::FromString(FString(ButtonName) == TEXT("RestartButton") ? TEXT("다시 도전") : TEXT("메인 메뉴")));
                Caption->SetJustification(ETextJustify::Center);
                Button->SetContent(Caption);
            }
            ButtonStyle(Button, FString(ButtonName) == TEXT("RestartButton"));
            DayRow(Button, 12.f);
        }
    }
    if (Payment)
    {
        UButton* Blocker = Find<UButton>(Blueprint, TEXT("InputBlocker"));
        UWidget* Calculator = Blueprint->WidgetTree->FindWidget(TEXT("WBP_Calculate"));
        if (!Blocker || !Calculator) { Error = TEXT("Payment input widgets missing."); return false; }
        FButtonStyle Transparent = Blocker->GetStyle();
        Transparent.Normal = Transparent.Hovered = Transparent.Pressed = Transparent.Disabled = Brush(FLinearColor::Transparent, 0.f);
        Blocker->SetStyle(Transparent);
        Place(Canvas, Blocker, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(), FVector2D::ZeroVector, -50);
        DayText(Blueprint, TEXT("TextBlock_51"), 34, Gold);
        DayText(Blueprint, TEXT("ErrorText"), 18, FLinearColor(.85f, .29f, .24f, 1.f));
        DayText(Blueprint, TEXT("WaitingText"), 22, Ivory);
        DayText(Blueprint, TEXT("PaymentProgressText"), 20, Gold);
        USizeBox* KeypadBounds = Decoration<USizeBox>(Blueprint, TEXT("CasinoPaymentKeypadBounds"));
        UVerticalBox* Input = Find<UVerticalBox>(Blueprint, TEXT("PaymentInputPanel"));
        if (!Input) { Error = TEXT("Payment input panel missing."); return false; }
        if (Calculator->GetParent() != KeypadBounds)
        {
            const int32 Index = Input->GetChildIndex(Calculator);
            Calculator->RemoveFromParent();
            Input->InsertChildAt(Index, KeypadBounds);
            KeypadBounds->SetContent(Calculator);
        }
        KeypadBounds->SetHeightOverride(340.f);
        DayRow(KeypadBounds);
    }
    return true;
}

bool StylePaymentCalculator(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Canvas = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
    UWidget* Grid = Blueprint->WidgetTree->FindWidget(TEXT("GridPanel_35"));
    UButton* Back = Find<UButton>(Blueprint, TEXT("Button_BG"));
    UButton* Submit = Find<UButton>(Blueprint, TEXT("Button_OK"));
    if (!Canvas || !Grid || !Back || !Submit) { Error = TEXT("Payment calculator structure missing."); return false; }
    Restyle(Blueprint);
    Place(Canvas, Grid, FAnchors(1.f, 1.f), FMargin(-300.f, -300.f, 400.f, 400.f), FVector2D(.5f, .5f), 0);
    if (UGridPanel* Keys = Cast<UGridPanel>(Grid))
    {
        for (int32 Index = 0; Index < 4; ++Index)
        { Keys->SetColumnFill(Index, 0.f); Keys->SetRowFill(Index, 0.f); }
        for (UWidget* Cell : Keys->GetAllChildren())
        {
            if (USizeBox* Size = Cast<USizeBox>(Cell))
            { Size->SetWidthOverride(100.f); Size->SetHeightOverride(100.f); }
            if (UGridSlot* Slot = Cast<UGridSlot>(Cell->Slot)) Slot->SetPadding(FMargin(0.f));
        }
    }
    FButtonStyle Transparent = Back->GetStyle();
    Transparent.Normal = Transparent.Hovered = Transparent.Pressed = Transparent.Disabled = Brush(FLinearColor::Transparent, 0.f);
    Back->SetStyle(Transparent);
    ButtonStyle(Submit, true);
    return true;
}
}

bool MigrateCasinoDayLoopLayouts(FString& OutError)
{
    if (!MigrateCasinoPaymentKeypadLayout(OutError)) return false;
    for (int32 Index = 0; Index < 4; ++Index)
        if (!MigrateCasinoPresentationWidget(DayLoopPaths[Index], StyleDayLoop, OutError)) return false;
    return true;
}

bool MigrateCasinoPaymentKeypadLayout(FString& OutError)
{
    return MigrateCasinoPresentationWidget(DayLoopPaths[4], StylePaymentCalculator, OutError);
}

bool MigrateCasinoLadderControls(FString& OutError)
{
    return MigrateCasinoPresentationWidget(LadderControlPath, [](UWidgetBlueprint* Blueprint, FString& Error)
    {
        UWidget* Bet = Blueprint->WidgetTree->FindWidget(TEXT("넘패드"));
        if (!Bet || !Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_2")))
        { Error = TEXT("Ladder controls have an unexpected layout."); return false; }
        for (const TCHAR* Name : {TEXT("SelectRailLeft"), TEXT("SelectRailRight"), TEXT("RailCountLeft"), TEXT("RailCountRight")})
        {
            UButton* Button = Find<UButton>(Blueprint, Name);
            if (!Button) { Error = TEXT("Ladder direction button missing: ") + FString(Name); return false; }
            UTextBlock* Arrow = Cast<UTextBlock>(Button->GetContent());
            if (!Arrow)
            {
                const FString ArrowName = FString(TEXT("CasinoArrow_")) + Name;
                Arrow = Decoration<UTextBlock>(Blueprint, *ArrowName);
                Button->SetContent(Arrow);
            }
            Arrow->SetText(FText::FromString(FString(Name).EndsWith(TEXT("Left")) ? TEXT("\u25C0") : TEXT("\u25B6")));
            TextStyle(Arrow, 32.f, Ivory);
            Arrow->SetColorAndOpacity(FSlateColor::UseForeground());
            Arrow->SetJustification(ETextJustify::Center);
            FButtonStyle Style = Button->GetStyle();
            Style.Normal = Style.Hovered = Style.Pressed = Style.Disabled = Brush(FLinearColor::Transparent, 0.f);
            Style.NormalForeground = FSlateColor(Ivory);
            Style.HoveredForeground = FSlateColor(Gold);
            Style.PressedForeground = Style.DisabledForeground = FSlateColor(Muted);
            Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
            Button->SetStyle(Style);
            if (UButtonSlot* Slot = Cast<UButtonSlot>(Arrow->Slot))
            { Slot->SetPadding(FMargin(0.f)); Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center); }
        }
        // Keep layout measurements intact: the fill rows determine the bet field's position.
        const TCHAR* Controls[] = {TEXT("HorizontalBox_0"), TEXT("HorizontalBox_1"), TEXT("SizeBox_7"), TEXT("SizeBox_0"), TEXT("SizeBox_2")};
        const float MoveY[] = {32.f, 16.f, 8.f, -8.f, -16.f};
        if (UTextBlock* RefreshCaption = Find<UTextBlock>(Blueprint, TEXT("TextBlock_63")))
        {
            FSlateFontInfo Font = RefreshCaption->GetFont(); Font.Size = 20;
            RefreshCaption->SetFont(Font);
        }
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Controls); ++Index)
        {
            UWidget* Widget = Blueprint->WidgetTree->FindWidget(Controls[Index]);
            if (!Widget) { Error = TEXT("Ladder control row missing."); return false; }
            FWidgetTransform Transform;
            Transform.Scale = FVector2D(.85f, .85f);
            Transform.Translation = FVector2D(0.f, MoveY[Index]);
            Widget->SetRenderTransformPivot(FVector2D(.5f, .5f));
            Widget->SetRenderTransform(Transform);
        }
        return true;
    }, OutError);
}

namespace CasinoWidgetStyle
{
FSlateBrush Brush(FLinearColor Color, float Radius, FLinearColor Outline) { return ::Brush(Color, Radius, Outline); }
void TextStyle(UTextBlock* Text, float Size, FLinearColor Color) { ::TextStyle(Text, Size, Color); }
void ButtonStyle(UButton* Button, bool Primary) { ::ButtonStyle(Button, Primary); }
UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors,
    const FMargin& Offsets, FVector2D Alignment, int32 Order, bool Auto)
{ return ::Place(Canvas, Widget, Anchors, Offsets, Alignment, Order, Auto); }
void Label(UWidgetBlueprint* Blueprint, UCanvasPanel* Canvas, const TCHAR* Name, const TCHAR* Value,
    FAnchors Anchors, FMargin Offsets, float Size, FLinearColor Color)
{ ::Label(Blueprint, Canvas, Name, Value, Anchors, Offsets, Size, Color); }
}
