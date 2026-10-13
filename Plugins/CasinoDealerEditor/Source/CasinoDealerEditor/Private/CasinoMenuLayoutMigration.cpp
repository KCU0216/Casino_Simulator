#include "CasinoMenuLayoutMigration.h"

#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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
    bool Allowed = false;
    for (const TCHAR* Path : AssetPaths) if (Blueprint->GetPathName() == Path) Allowed = true;
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
