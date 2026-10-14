#include "CasinoBlackjackUILayoutMigration.h"

#include "CasinoMenuLayoutMigration.h"
#include "CasinoWidgetStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "WidgetBlueprint.h"

namespace CasinoBlackjackLayout
{
using namespace CasinoWidgetStyle;

UVerticalBoxSlot* AddRow(UVerticalBox* Parent, UWidget* Child, FMargin Padding = FMargin())
{
    Child->RemoveFromParent();
    UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
    Slot->SetHorizontalAlignment(HAlign_Fill);
    Slot->SetVerticalAlignment(VAlign_Top);
    Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    Slot->SetPadding(Padding);
    return Slot;
}

USizeBox* BoundCards(UWidgetBlueprint* Blueprint, UHorizontalBox* Cards, const TCHAR* SizeName,
    const TCHAR* ScaleName, float Height)
{
    USizeBox* Size = Decoration<USizeBox>(Blueprint, SizeName);
    UScaleBox* Scale = Decoration<UScaleBox>(Blueprint, ScaleName);
    Size->SetHeightOverride(Height);
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Cards->RemoveFromParent();
    UContentWidget* Parent = Scale;
    Parent->SetContent(Cards);
    if (UScaleBoxSlot* Slot = Cast<UScaleBoxSlot>(Cards->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Left); Slot->SetVerticalAlignment(VAlign_Center); }
    Scale->RemoveFromParent();
    Size->SetContent(Scale);
    if (USizeBoxSlot* Slot = Cast<USizeBoxSlot>(Scale->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); }
    return Size;
}

bool StyleHUD(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Canvas = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
    UOverlay* InfoOverlay = Find<UOverlay>(Blueprint, TEXT("BlackjackInfoOverlay"));
    UBorder* Info = Find<UBorder>(Blueprint, TEXT("BlackjackInfoPanel"));
    UVerticalBox* Rows = Find<UVerticalBox>(Blueprint, TEXT("InfoVerticalBox"));
    UHorizontalBox* DealerCards = Find<UHorizontalBox>(Blueprint, TEXT("DealerCardPanel"));
    UHorizontalBox* OwnHands = Find<UHorizontalBox>(Blueprint, TEXT("MyHandPanel"));
    UHorizontalBox* Actions = Find<UHorizontalBox>(Blueprint, TEXT("ActionButtonPanel"));
    UBorder* Others = Find<UBorder>(Blueprint, TEXT("OtherPlayersPanel"));
    UVerticalBox* OtherList = Find<UVerticalBox>(Blueprint, TEXT("OtherPlayersList"));
    if (!Canvas || !InfoOverlay || !Info || !Rows || !DealerCards || !OwnHands || !Actions || !Others || !OtherList)
    { Error = TEXT("Blackjack HUD is missing a required panel; no asset was saved."); return false; }

    // The center stays completely transparent so the dealer/table remain visible.
    Place(Canvas, InfoOverlay, FAnchors(.025f, .5f), FMargin(0.f, -254.f, 536.f, 508.f));
    Info->SetBrush(Brush(FLinearColor(.016f, .024f, .039f, .86f), 14.f, FLinearColor(.76f, .49f, .15f, .5f)));
    Info->SetBrushColor(FLinearColor::White);
    Info->SetPadding(FMargin(22.f, 20.f));
    if (UOverlaySlot* Slot = Cast<UOverlaySlot>(Info->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); }

    UTextBlock* Title = Decoration<UTextBlock>(Blueprint, TEXT("CasinoBlackjackTitle"));
    Title->SetText(FText::FromString(TEXT("BLACKJACK")));
    Title->SetVisibility(ESlateVisibility::HitTestInvisible);
    TextStyle(Title, 28.f, Gold);
    UTextBlock* RuleText = Decoration<UTextBlock>(Blueprint, TEXT("CasinoBlackjackRuleText"));
    RuleText->SetText(FText::FromString(TEXT("21에 가깝게 · 블랙잭 3 : 2")));
    RuleText->SetVisibility(ESlateVisibility::HitTestInvisible);
    TextStyle(RuleText, 15.f, Muted);
    UBorder* Rule = Decoration<UBorder>(Blueprint, TEXT("CasinoBlackjackCardDivider"));
    Rule->SetBrush(Brush(FLinearColor(.76f, .49f, .15f, .35f), 0.f));
    Rule->SetPadding(FMargin(0.f));
    Rule->SetVisibility(ESlateVisibility::HitTestInvisible);
    USizeBox* RuleHeight = Decoration<USizeBox>(Blueprint, TEXT("CasinoBlackjackCardDividerHeight"));
    RuleHeight->SetHeightOverride(1.f);
    Rule->RemoveFromParent(); RuleHeight->SetContent(Rule);
    UTextBlock* DealerText = Find<UTextBlock>(Blueprint, TEXT("DealerInfoText"));
    UTextBlock* OwnText = Find<UTextBlock>(Blueprint, TEXT("MyHandInfoText"));
    if (!DealerText || !OwnText) { Error = TEXT("Blackjack score labels are missing."); return false; }
    TextStyle(DealerText, 21.f, Ivory); TextStyle(OwnText, 21.f, Ivory);
    USizeBox* DealerSize = BoundCards(Blueprint, DealerCards, TEXT("CasinoBlackjackDealerCardSize"),
        TEXT("CasinoBlackjackDealerCardScale"), 128.f);
    Rows->ClearChildren();
    AddRow(Rows, Title);
    AddRow(Rows, RuleText, FMargin(0.f, 2.f, 0.f, 18.f));
    AddRow(Rows, DealerText, FMargin(0.f, 0.f, 0.f, 8.f));
    AddRow(Rows, DealerSize, FMargin(0.f, 0.f, 0.f, 16.f));
    AddRow(Rows, RuleHeight, FMargin(0.f, 0.f, 0.f, 16.f));
    AddRow(Rows, OwnText, FMargin(0.f, 0.f, 0.f, 10.f));
    AddRow(Rows, OwnHands);

    for (int32 Index = 1; Index <= 2; ++Index)
    {
        const FString HandName = FString::Printf(TEXT("Hand%dPanel"), Index);
        const FString CardName = FString::Printf(TEXT("Hand%dCards"), Index);
        const FString ScoreName = FString::Printf(TEXT("Text_Hand%dValue"), Index);
        UVerticalBox* Hand = Find<UVerticalBox>(Blueprint, *HandName);
        UHorizontalBox* Cards = Find<UHorizontalBox>(Blueprint, *CardName);
        UTextBlock* Score = Find<UTextBlock>(Blueprint, *ScoreName);
        if (!Hand || !Cards || !Score) { Error = TEXT("Blackjack split-hand contract is missing."); return false; }
        TextStyle(Score, 18.f, Gold);
        Score->SetAutoWrapText(true);
        if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Hand->Slot))
        {
            Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Top);
            Slot->SetPadding(FMargin(Index == 2 ? 12.f : 0.f, 0.f, Index == 1 ? 12.f : 0.f, 0.f));
        }
        if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Score->Slot)) Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        const FString SizeName = FString::Printf(TEXT("CasinoBlackjackHand%dCardSize"), Index);
        const FString ScaleName = FString::Printf(TEXT("CasinoBlackjackHand%dCardScale"), Index);
        USizeBox* Size = BoundCards(Blueprint, Cards, *SizeName, *ScaleName, 128.f);
        AddRow(Hand, Size);
        const FString ResultName = FString::Printf(TEXT("BlackjackOwnHand%dResult"), Index);
        if (UTextBlock* Result = Find<UTextBlock>(Blueprint, *ResultName))
        { TextStyle(Result, 18.f, Ivory); Result->SetAutoWrapText(true); }
    }

    // Each action keeps its original visibility/enabled state and click binding.
    for (UWidget* Child : Actions->GetAllChildren())
    {
        if (UButton* Button = Cast<UButton>(Child))
        {
            ButtonStyle(Button, Button->GetFName() == TEXT("HitButton"));
            Button->SetBackgroundColor(FLinearColor::White);
            FButtonStyle Style = Button->GetStyle();
            Style.NormalPadding = FMargin(18.f, 14.f);
            Style.PressedPadding = FMargin(18.f, 15.f, 18.f, 13.f);
            Button->SetStyle(Style);
            if (UTextBlock* Text = Cast<UTextBlock>(Button->GetContent()))
            { TextStyle(Text, 19.f, Ivory); Text->SetColorAndOpacity(FSlateColor::UseForeground()); }
            if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Button->Slot))
            {
                Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
                Slot->SetPadding(FMargin(4.f, 0.f));
                Slot->SetVerticalAlignment(VAlign_Fill);
            }
        }
    }
    UScaleBox* ActionScale = Decoration<UScaleBox>(Blueprint, TEXT("CasinoBlackjackActionScale"));
    ActionScale->SetStretch(EStretch::ScaleToFit);
    ActionScale->SetStretchDirection(EStretchDirection::DownOnly);
    Actions->RemoveFromParent(); ActionScale->SetContent(Actions);
    if (UScaleBoxSlot* Slot = Cast<UScaleBoxSlot>(Actions->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center); }
    Place(Canvas, ActionScale, FAnchors(.04f, 1.f, .96f, 1.f), FMargin(0.f, -98.f, 0.f, 64.f), FVector2D::ZeroVector, 10);

    Place(Canvas, Others, FAnchors(.975f, .5f), FMargin(-382.f, -254.f, 382.f, 508.f));
    Others->SetBrush(Brush(FLinearColor(.016f, .024f, .039f, .86f), 14.f, FLinearColor(.76f, .49f, .15f, .35f)));
    Others->SetBrushColor(FLinearColor::White);
    Others->SetPadding(FMargin(20.f));
    UVerticalBox* OtherBody = Decoration<UVerticalBox>(Blueprint, TEXT("CasinoBlackjackOtherBody"));
    UTextBlock* OtherTitle = Decoration<UTextBlock>(Blueprint, TEXT("CasinoBlackjackOtherTitle"));
    OtherTitle->SetText(FText::FromString(TEXT("함께 플레이 중")));
    OtherTitle->SetVisibility(ESlateVisibility::HitTestInvisible);
    TextStyle(OtherTitle, 20.f, Gold);
    OtherBody->ClearChildren();
    AddRow(OtherBody, OtherTitle, FMargin(0.f, 0.f, 0.f, 16.f));
    UVerticalBoxSlot* OtherSlot = AddRow(OtherBody, OtherList);
    OtherSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    OtherBody->RemoveFromParent(); Others->SetContent(OtherBody);

    // The shared keypad is anchored at bottom-right. Its HUD-owned amount and
    // cancel controls must use the same anchor rather than fixed 1920px coordinates.
    UCanvasPanel* Keypad = Find<UCanvasPanel>(Blueprint, TEXT("BetKeypadLayer"));
    if (!Keypad) { Error = TEXT("Blackjack keypad layer is missing."); return false; }
    UBorder* KeypadFrame = Decoration<UBorder>(Blueprint, TEXT("CasinoBlackjackKeypadFrame"));
    KeypadFrame->SetBrush(Brush(FLinearColor(.016f, .024f, .039f, .9f), 14.f, FLinearColor(.76f, .49f, .15f, .5f)));
    KeypadFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Keypad, KeypadFrame, FAnchors(1.f, 1.f), FMargin(-522.f, -626.f, 444.f, 546.f), FVector2D::ZeroVector, 90);
    Label(Blueprint, Keypad, TEXT("CasinoBlackjackBettingTitle"), TEXT("베팅 금액 입력"), FAnchors(1.f, 1.f),
        FMargin(-500.f, -604.f, 310.f, 32.f), 21.f, Ivory);
    Place(Keypad, Find<UTextBlock>(Blueprint, TEXT("CasinoBlackjackBettingTitle")), FAnchors(1.f, 1.f),
        FMargin(-500.f, -604.f, 310.f, 32.f), FVector2D::ZeroVector, 110);
    if (UButton* Cancel = Find<UButton>(Blueprint, TEXT("CancelBetButton")))
    {
        ButtonStyle(Cancel); Cancel->SetBackgroundColor(FLinearColor::White);
        FButtonStyle Style = Cancel->GetStyle();
        Style.NormalPadding = Style.PressedPadding = FMargin(5.f);
        Cancel->SetStyle(Style);
        if (UTextBlock* Caption = Cast<UTextBlock>(Cancel->GetContent()))
        { Caption->SetText(FText::FromString(TEXT("닫기"))); TextStyle(Caption, 16.f, Ivory); }
        Place(Keypad, Cancel, FAnchors(1.f, 1.f), FMargin(-156.f, -596.f, 56.f, 40.f), FVector2D::ZeroVector, 120);
    }
    if (UTextBlock* Amount = Find<UTextBlock>(Blueprint, TEXT("TextBlock_55")))
    {
        TextStyle(Amount, 26.f, Gold);
        Place(Keypad, Amount, FAnchors(1.f, 1.f), FMargin(-500.f, -552.f, 400.f, 42.f), FVector2D::ZeroVector, 110);
    }
    return true;
}

bool StyleCard(UWidgetBlueprint* Blueprint, FString& Error)
{
    USizeBox* Size = Find<USizeBox>(Blueprint, TEXT("SizeBox_Card"));
    UImage* Image = Find<UImage>(Blueprint, TEXT("Image_Card"));
    if (!Size || !Image || Blueprint->WidgetTree->RootWidget != Size)
    { Error = TEXT("Blackjack card widget is missing its size/image contract."); return false; }
    // The selected deck's textures are 242 x 348: retain that exact proportion.
    Size->SetWidthOverride(90.f);
    Size->SetHeightOverride(84.f * 348.f / 242.f + 6.f);
    FSlateBrush CardBrush = Image->GetBrush();
    CardBrush.ImageSize = FVector2D(84.f, 84.f * 348.f / 242.f);
    Image->SetBrush(CardBrush);
    if (USizeBoxSlot* Slot = Cast<USizeBoxSlot>(Image->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); Slot->SetPadding(FMargin(3.f)); }
    return true;
}
}

bool MigrateCasinoBlackjackUILayout(FString& OutError)
{
    if (!MigrateCasinoPresentationWidget(TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackHUD.WBP_BlackjackHUD"),
        CasinoBlackjackLayout::StyleHUD, OutError)) return false;
    return MigrateCasinoPresentationWidget(TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackCardUI.WBP_BlackjackCardUI"),
        CasinoBlackjackLayout::StyleCard, OutError);
}
