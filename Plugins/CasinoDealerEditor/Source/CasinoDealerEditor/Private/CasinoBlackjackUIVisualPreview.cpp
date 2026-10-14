#include "CasinoBlackjackUIVisualPreview.h"

#include "CasinoWidgetStyle.h"
#include "Blackjack/BlackjackBlueprintLibrary.h"
#include "Blackjack/BlackjackUIBlueprintLibrary.h"
#include "Blackjack/BlackjackTypes.h"
#include "UI/CasinoKeypadStyle.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DynamicRHI.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderDeferredCleanup.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureCompiler.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCasinoBlackjackPreview, Log, All);

namespace CasinoBlackjackPreview
{
IConsoleCommand* PreviewCommand = nullptr;
IConsoleCommand* KeypadPreviewCommand = nullptr;
const TCHAR* HUDPath = TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackHUD.WBP_BlackjackHUD_C");
const TCHAR* CardPath = TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackCardUI.WBP_BlackjackCardUI_C");

void Prepare(UUserWidget* Widget)
{
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    UBlueprintGeneratedClass::UnbindDynamicDelegates(Widget->GetClass(), Widget);
    if (!Widget->WidgetTree) return;
    Widget->WidgetTree->ForEachWidget([](UWidget* Child)
    {
        Child->SetDesignerFlags(EWidgetDesignFlags::Designing);
        for (TFieldIterator<FProperty> It(Child->GetClass()); It; ++It)
        {
            if (FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
                Delegate->ClearDelegate(Child, Delegate->ContainerPtrToValuePtr<void>(Child));
            else if (FDelegateProperty* Single = CastField<FDelegateProperty>(*It))
                Single->ContainerPtrToValuePtr<FScriptDelegate>(Child)->Unbind();
        }
        if (UUserWidget* Nested = Cast<UUserWidget>(Child)) Prepare(Nested);
    });
}

UUserWidget* Create(const TCHAR* Path, FString& Error)
{
    UClass* Class = LoadClass<UUserWidget>(nullptr, Path);
    if (!Class) { Error = TEXT("Blackjack preview class could not load: ") + FString(Path); return nullptr; }
    UUserWidget* Widget = NewObject<UUserWidget>(GetTransientPackage(), Class, NAME_None, RF_Transient);
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    if (!Widget->Initialize() || !Widget->WidgetTree || !Widget->WidgetTree->RootWidget)
    { Error = TEXT("Blackjack preview could not initialize: ") + FString(Path); return nullptr; }
    Prepare(Widget);
    return Widget;
}

template<typename T>
T* Find(UUserWidget* Root, const TCHAR* Name)
{
    return Root && Root->WidgetTree ? Cast<T>(Root->WidgetTree->FindWidget(Name)) : nullptr;
}

bool Text(UUserWidget* Root, const TCHAR* Name, const TCHAR* Value, FString& Error)
{
    UTextBlock* Label = Find<UTextBlock>(Root, Name);
    if (!Label) { Error = TEXT("Blackjack preview label missing: ") + FString(Name); return false; }
    Label->SetText(FText::FromString(Value));
    return true;
}

bool Cards(UHorizontalBox* Parent, const TArray<FBlackjackCard>& CardsToShow, FString& Error)
{
    if (!Parent) { Error = TEXT("Blackjack preview card row missing."); return false; }
    Parent->ClearChildren();
    for (int32 CardIndex = 0; CardIndex < CardsToShow.Num(); ++CardIndex)
    {
        const FBlackjackCard& Card = CardsToShow[CardIndex];
        UUserWidget* Widget = Create(CardPath, Error);
        if (!Widget) return false;
        UImage* Image = Find<UImage>(Widget, TEXT("Image_Card"));
        const FObjectPropertyBase* TableProperty = FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("CardVisualDataTable"));
        const FObjectPropertyBase* BackProperty = FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("CardBackTexture"));
        const FProperty* DeckProperty = FindFProperty<FProperty>(Widget->GetClass(), TEXT("DeckStyle"));
        const FNumericProperty* DeckNumber = CastField<FNumericProperty>(DeckProperty);
        if (const FEnumProperty* Enum = CastField<FEnumProperty>(DeckProperty)) DeckNumber = Enum->GetUnderlyingProperty();
        UDataTable* Table = TableProperty ? Cast<UDataTable>(TableProperty->GetObjectPropertyValue_InContainer(Widget)) : nullptr;
        if (!Image || !Table || !BackProperty || !DeckNumber)
        { Error = TEXT("Blackjack card preview lacks the shipped visual contract."); return false; }
        const EBlackjackDeckStyle Deck = static_cast<EBlackjackDeckStyle>(DeckNumber->GetSignedIntPropertyValue(
            DeckProperty->ContainerPtrToValuePtr<void>(Widget)));
        FBlackjackCardVisualData Visual;
        if (!UBlackjackBlueprintLibrary::FindBlackjackCardVisualData(Table, Deck, Card, Visual))
        { Error = TEXT("Blackjack preview card texture was not found in the shipped data table."); return false; }
        UTexture2D* Texture = Card.bFaceUp ? Visual.FrontTexture.Get() : Cast<UTexture2D>(BackProperty->GetObjectPropertyValue_InContainer(Widget));
        if (!Texture) { Error = TEXT("Blackjack preview card face/back is missing."); return false; }
        Image->SetBrushFromTexture(Texture, false);
        UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Widget);
        Slot->SetVerticalAlignment(VAlign_Top);
        // Match the presentation helper's compact fan after RenderCards.
        Slot->SetPadding(FMargin(0.f, 0.f, CardsToShow.Num() > 3 && CardIndex + 1 < CardsToShow.Num() ? -44.f : 0.f, 0.f));
    }
    return true;
}

UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Name, const FString& Value,
    int32 FontSize, FLinearColor Color)
{
    UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*Name));
    Label->SetText(FText::FromString(Value));
    CasinoWidgetStyle::TextStyle(Label, FontSize, Color);
    Label->SetAutoWrapText(true);
    UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Label);
    Slot->SetHorizontalAlignment(HAlign_Fill);
    Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
    return Label;
}

bool OtherSeats(UUserWidget* HUD, bool Stress, bool Settled, FString& Error)
{
    UVerticalBox* List = Find<UVerticalBox>(HUD, TEXT("OtherPlayersList"));
    UBorder* Panel = Find<UBorder>(HUD, TEXT("OtherPlayersPanel"));
    if (!List || !Panel) { Error = TEXT("Blackjack preview other-seat panel missing."); return false; }
    List->ClearChildren();
    UScrollBox* Scroll = HUD->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PreviewBlackjackScroll"));
    Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
    Scroll->SetScrollbarPadding(FMargin(8.f, 0.f, 0.f, 0.f));
    FScrollBarStyle BarStyle = Scroll->GetWidgetBarStyle();
    BarStyle.NormalThumbImage.TintColor = FSlateColor(CasinoWidgetStyle::Gold);
    BarStyle.HoveredThumbImage.TintColor = FSlateColor(CasinoWidgetStyle::Ivory);
    Scroll->SetWidgetBarStyle(BarStyle);
    UVerticalBoxSlot* ScrollSlot = List->AddChildToVerticalBox(Scroll);
    ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FString Prefix = FString::Printf(TEXT("PreviewSeat%d"), Index);
        const bool Active = Index == 0 && !Settled;
        UBorder* Frame = HUD->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*(Prefix + TEXT("Frame"))));
        Frame->SetBrush(CasinoWidgetStyle::Brush(FLinearColor(.027f, .039f, .059f, .92f), 8.f,
            Active ? FLinearColor(.94f, .67f, .29f) : FLinearColor(.12f, .16f, .22f, .6f)));
        Frame->SetPadding(FMargin(12.f, 10.f));
        Frame->SetHorizontalAlignment(HAlign_Fill); Frame->SetVerticalAlignment(VAlign_Top);
        Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        UVerticalBox* Row = HUD->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*Prefix));
        Frame->SetContent(Row);
        UScrollBoxSlot* FrameSlot = CastChecked<UScrollBoxSlot>(Scroll->AddChild(Frame));
        FrameSlot->SetHorizontalAlignment(HAlign_Fill); FrameSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        UTextBlock* Header = AddText(HUD->WidgetTree, Row, Prefix + TEXT("Header"),
            FString::Printf(TEXT("%d번 자리 · %s"), Index + 2,
                Index == 0 && Stress ? TEXT("아주 긴 플레이어 이름 테스트") : Index == 0 ? TEXT("플레이어 2") : Index == 1 ? TEXT("CASINO KING") : TEXT("플레이어 4")), 16, CasinoWidgetStyle::Ivory);
        Header->SetAutoWrapText(false);
        Header->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Header->SetToolTipText(Header->GetText());
        UTextBlock* Wager = AddText(HUD->WidgetTree, Row, Prefix + TEXT("Wager"),
            Stress && Index == 0 ? TEXT("총 베팅 100,000,000원 · 보험 5,000,000원") : TEXT("총 베팅 5,000원"),
            13, FLinearColor(.64f, .69f, .76f));
        CastChecked<UVerticalBoxSlot>(Wager->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        for (int32 Hand = 0; Hand < (Stress && Index == 0 ? 2 : 1); ++Hand)
        {
            const FString HandPrefix = Prefix + FString::Printf(TEXT("Hand%d"), Hand);
            UVerticalBox* HandRow = HUD->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*HandPrefix));
            UVerticalBoxSlot* HandSlot = Row->AddChildToVerticalBox(HandRow);
            HandSlot->SetHorizontalAlignment(HAlign_Fill);
            HandSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Stress && Index == 0 && Hand == 0 ? 10.f : 0.f));
            AddText(HUD->WidgetTree, HandRow, HandPrefix + TEXT("Info"),
                FString::Printf(TEXT("%d번 핸드 · %s · %s"), Hand + 1,
                    Index == 2 ? TEXT("미공개") : TEXT("18점"), Settled ? TEXT("승리") : Active && Hand == 0 ? TEXT("현재 차례") : TEXT("대기")),
                14, Active && Hand == 0 ? FLinearColor(.94f, .67f, .29f) : CasinoWidgetStyle::Ivory);
            if (Stress && Index == 0) AddText(HUD->WidgetTree, HandRow, HandPrefix + TEXT("Wager"),
                TEXT("핸드 베팅 50,000,000원"), 12, FLinearColor(.64f, .69f, .76f));
            USizeBox* Size = HUD->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(HandPrefix + TEXT("Size"))));
            Size->SetHeightOverride(72.f);
            UVerticalBoxSlot* SizeSlot = HandRow->AddChildToVerticalBox(Size);
            SizeSlot->SetHorizontalAlignment(HAlign_Fill);
            UScaleBox* Scale = HUD->WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), FName(*(HandPrefix + TEXT("Scale"))));
            Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::DownOnly);
            Size->SetContent(Scale);
            if (USizeBoxSlot* Slot = Cast<USizeBoxSlot>(Scale->Slot))
            { Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); }
            UHorizontalBox* CardRow = HUD->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*(HandPrefix + TEXT("Cards"))));
            Scale->SetContent(CardRow);
            if (UScaleBoxSlot* Slot = Cast<UScaleBoxSlot>(CardRow->Slot))
            { Slot->SetHorizontalAlignment(HAlign_Left); Slot->SetVerticalAlignment(VAlign_Top); }
            TArray<FBlackjackCard> Sample = {
                {EBlackjackSuit::Spades, EBlackjackRank::Ten}, {EBlackjackSuit::Hearts, EBlackjackRank::Eight, Index != 2}};
            if (Stress && Index == 0) Sample = {
                {EBlackjackSuit::Clubs, EBlackjackRank::Two}, {EBlackjackSuit::Diamonds, EBlackjackRank::Three},
                {EBlackjackSuit::Spades, EBlackjackRank::Ace}, {EBlackjackSuit::Hearts, EBlackjackRank::Five},
                {EBlackjackSuit::Clubs, EBlackjackRank::Two}, {EBlackjackSuit::Diamonds, EBlackjackRank::Five}};
            if (!Cards(CardRow, Sample, Error)) return false;
        }
    }
    Panel->SetVisibility(ESlateVisibility::Visible);
    return true;
}

bool Sample(UUserWidget* HUD, const FString& Variant, FString& Error)
{
    UBlackjackUIBlueprintLibrary::PrepareBetKeypadAppearance(HUD);
    const bool Settled = Variant == TEXT("Settled") || Variant == TEXT("LongHands");
    const bool Split = Variant == TEXT("Split") || Variant == TEXT("LongHands") || Variant == TEXT("Settled");
    const bool Stress = Variant == TEXT("LongHands") || Variant == TEXT("SingleLongHand");
    const bool Insurance = Variant == TEXT("Insurance");
    const bool Betting = Variant == TEXT("Betting");
    const TCHAR* Required[] = { TEXT("BlackjackInfoOverlay"), TEXT("Hand2Panel"), TEXT("BetKeypadLayer"),
        TEXT("ActionButtonPanel"), TEXT("OtherPlayersPanel") };
    for (const TCHAR* Name : Required) if (!Find<UWidget>(HUD, Name))
    { Error = TEXT("Blackjack preview panel missing: ") + FString(Name); return false; }
    Find<UWidget>(HUD, TEXT("BlackjackInfoOverlay"))->SetVisibility(Betting ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    Find<UWidget>(HUD, TEXT("Hand2Panel"))->SetVisibility(Split ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    Find<UWidget>(HUD, TEXT("BetKeypadLayer"))->SetVisibility(Betting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Find<UWidget>(HUD, TEXT("ActionButtonPanel"))->SetVisibility(Betting ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    for (int32 Index = 1; Index <= 2; ++Index)
    {
        const FString Name = FString::Printf(TEXT("BlackjackOwnHand%dResult"), Index);
        UTextBlock* Result = Find<UTextBlock>(HUD, *Name);
        if (!Result) { Error = TEXT("Blackjack preview own result missing."); return false; }
        Result->SetText(Settled ? FText::FromString(Index == 1 ? TEXT("블랙잭 승리") : TEXT("패배")) : FText::GetEmpty());
        Result->SetVisibility(Settled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    const TCHAR* Actions[] = { TEXT("HitButton"), TEXT("StandButton"), TEXT("DoubleDownButton"), TEXT("SplitButton"),
        TEXT("InsuranceButton"), TEXT("SkipInsuranceButton"), TEXT("LeaveButton") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
    {
        UButton* Button = Find<UButton>(HUD, Actions[Index]);
        if (!Button) { Error = TEXT("Blackjack preview action missing: ") + FString(Actions[Index]); return false; }
        const bool Visible = Stress || Index == 6 || (Insurance ? Index == 4 || Index == 5 : Index < 3 || Index == 3 && Split);
        Button->SetVisibility(Visible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button->SetIsEnabled(!Settled && (!Insurance || Index >= 4));
    }
    if (Betting)
    {
        Find<UWidget>(HUD, TEXT("OtherPlayersPanel"))->SetVisibility(ESlateVisibility::Collapsed);
        return Text(HUD, TEXT("TextBlock_55"), TEXT("베팅 금액 : 10,000원"), Error);
    }
    // Use the actual graph's CRLF suffix; the runtime presentation helper must
    // normalize it before measuring labels and arranging the card strips.
    if (!Text(HUD, TEXT("DealerInfoText"), Insurance ? TEXT("딜러 · A / 미공개\r\n") : TEXT("딜러 · 공개 합계: 10\r\n"), Error)
        || !Text(HUD, TEXT("MyHandInfoText"), Insurance ? TEXT("내 카드 · 보험 선택") : Split ? TEXT("내 카드 · 스플릿 핸드") : TEXT("내 카드 · 내 차례"), Error)
        || !Text(HUD, TEXT("Text_Hand1Value"), Stress ? TEXT("핸드 1 · 합계: 24\r\n") : Split ? TEXT("핸드 1 · 합계: 18\r\n") : TEXT("핸드 1 · 합계: 17\r\n"), Error)
        || !Text(HUD, TEXT("Text_Hand2Value"), TEXT("핸드 2 · 합계: 21"), Error)) return false;
    if (!Cards(Find<UHorizontalBox>(HUD, TEXT("DealerCardPanel")), {
        {EBlackjackSuit::Hearts, Insurance ? EBlackjackRank::Ace : EBlackjackRank::Ten},
        {EBlackjackSuit::Clubs, EBlackjackRank::None, false}}, Error)) return false;
    TArray<FBlackjackCard> Own = {{EBlackjackSuit::Spades, EBlackjackRank::Ten}, {EBlackjackSuit::Hearts, EBlackjackRank::Seven}};
    if (Split) Own = {{EBlackjackSuit::Spades, EBlackjackRank::Eight}, {EBlackjackSuit::Hearts, EBlackjackRank::Ten}};
    if (Stress) Own = {{EBlackjackSuit::Clubs, EBlackjackRank::Two}, {EBlackjackSuit::Diamonds, EBlackjackRank::Three},
        {EBlackjackSuit::Spades, EBlackjackRank::Ace}, {EBlackjackSuit::Hearts, EBlackjackRank::Five},
        {EBlackjackSuit::Clubs, EBlackjackRank::Two}, {EBlackjackSuit::Diamonds, EBlackjackRank::Five},
        {EBlackjackSuit::Hearts, EBlackjackRank::Six}};
    if (!Cards(Find<UHorizontalBox>(HUD, TEXT("Hand1Cards")), Own, Error)) return false;
    if (Split && !Cards(Find<UHorizontalBox>(HUD, TEXT("Hand2Cards")), {
        {EBlackjackSuit::Clubs, EBlackjackRank::Eight}, {EBlackjackSuit::Diamonds, EBlackjackRank::Three},
        {EBlackjackSuit::Hearts, EBlackjackRank::King}}, Error)) return false;
    UBlackjackUIBlueprintLibrary::PrepareBetKeypadAppearance(HUD);
    return OtherSeats(HUD, Stress, Settled, Error);
}

bool CheckCardGeometry(UUserWidget* HUD, FString& Error)
{
    const auto Contains = [](const FSlateRect& Outer, const FSlateRect& Inner)
    {
        constexpr float Tolerance = 1.5f;
        return Inner.Left >= Outer.Left - Tolerance && Inner.Top >= Outer.Top - Tolerance
            && Inner.Right <= Outer.Right + Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
    };
    UBorder* Info = Find<UBorder>(HUD, TEXT("BlackjackInfoPanel"));
    // FWidgetRenderer paints its virtual window without a normal Slate tick.
    // Paint-space geometry records this draw; tick-space geometry is still empty.
    const FSlateRect PanelBounds = Info->GetPaintSpaceGeometry().GetRenderBoundingRect();
    const TCHAR* Rows[] = {TEXT("DealerCardPanel"), TEXT("Hand1Cards"), TEXT("Hand2Cards")};
    const TCHAR* Areas[] = {TEXT("CasinoBlackjackDealerCardSize"), TEXT("CasinoBlackjackHand1CardSize"), TEXT("CasinoBlackjackHand2CardSize")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rows); ++Index)
    {
        if (Index == 2 && Find<UWidget>(HUD, TEXT("Hand2Panel"))->GetVisibility() == ESlateVisibility::Collapsed) continue;
        UHorizontalBox* CardsPanel = Find<UHorizontalBox>(HUD, Rows[Index]);
        UWidget* Area = Find<UWidget>(HUD, Areas[Index]);
        if (!CardsPanel || !Area || CardsPanel->GetChildrenCount() == 0)
        { Error = TEXT("Blackjack card geometry check lacks a populated row."); return false; }
        const FSlateRect AreaBounds = Area->GetPaintSpaceGeometry().GetRenderBoundingRect();
        if (AreaBounds.GetSize().Y < 1.f || !Contains(PanelBounds, AreaBounds))
        {
            Error = FString::Printf(TEXT("Blackjack %s area [%.1f,%.1f,%.1f,%.1f] exceeds panel [%.1f,%.1f,%.1f,%.1f]."),
                Areas[Index], AreaBounds.Left, AreaBounds.Top, AreaBounds.Right, AreaBounds.Bottom,
                PanelBounds.Left, PanelBounds.Top, PanelBounds.Right, PanelBounds.Bottom);
            return false;
        }
        for (UWidget* Card : CardsPanel->GetAllChildren())
        {
            const FSlateRect CardBounds = Card->GetPaintSpaceGeometry().GetRenderBoundingRect();
            if (CardBounds.GetSize().Y < 1.f || !Contains(AreaBounds, CardBounds))
            {
                Error = FString::Printf(TEXT("Blackjack %s card [%.1f,%.1f,%.1f,%.1f] exceeds strip [%.1f,%.1f,%.1f,%.1f]."),
                    Rows[Index], CardBounds.Left, CardBounds.Top, CardBounds.Right, CardBounds.Bottom,
                    AreaBounds.Left, AreaBounds.Top, AreaBounds.Right, AreaBounds.Bottom);
                return false;
            }
        }
    }
    const TCHAR* Labels[] = {TEXT("DealerInfoText"), TEXT("MyHandInfoText"), TEXT("Text_Hand1Value"),
        TEXT("Text_Hand2Value"), TEXT("BlackjackOwnHand1Result"), TEXT("BlackjackOwnHand2Result")};
    for (const TCHAR* Name : Labels)
    {
        UWidget* Label = Find<UWidget>(HUD, Name);
        if (!Label || Label->GetVisibility() == ESlateVisibility::Collapsed) continue;
        if (FString(Name).Contains(TEXT("2")) && Find<UWidget>(HUD, TEXT("Hand2Panel"))->GetVisibility() == ESlateVisibility::Collapsed) continue;
        if (!Contains(PanelBounds, Label->GetPaintSpaceGeometry().GetRenderBoundingRect()))
        { Error = FString::Printf(TEXT("Blackjack %s label exceeds its info panel."), Name); return false; }
    }
    return true;
}

void MatchDesignerState(UUserWidget* Widget)
{
    // Design mode prevents gameplay construction but deliberately forces every
    // UMG control visible/enabled. Mirror this snapshot through the designer's
    // visibility property while keeping the safety of design mode.
    const auto Hidden = [](UWidget* Child)
    {
        const ESlateVisibility Visibility = Child->GetVisibility();
        Child->bHiddenInDesigner = Visibility == ESlateVisibility::Collapsed || Visibility == ESlateVisibility::Hidden;
    };
    Hidden(Widget);
    if (!Widget->WidgetTree) return;
    Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
    {
        Hidden(Child);
        if (UUserWidget* Nested = Cast<UUserWidget>(Child)) MatchDesignerState(Nested);
    });
}

void FinishPreviewResources(UUserWidget* Widget)
{
    TArray<UTexture*> Textures;
    const auto Gather = [&](auto&& Self, UUserWidget* UserWidget) -> void
    {
        if (!UserWidget->WidgetTree) return;
        UserWidget->WidgetTree->ForEachWidget([&](UWidget* Child)
        {
            if (UImage* Image = Cast<UImage>(Child))
                if (UTexture* Texture = Cast<UTexture>(Image->GetBrush().GetResourceObject())) Textures.AddUnique(Texture);
            if (UUserWidget* Nested = Cast<UUserWidget>(Child)) Self(Self, Nested);
        });
    };
    Gather(Gather, Widget);
    FTextureCompilingManager::Get().FinishCompilation(Textures);
    for (UTexture* Texture : Textures)
    {
        Texture->SetForceMipLevelsToBeResident(30.f);
        Texture->WaitForStreaming();
    }
    FlushRenderingCommands();
}

void MatchSlateEnabledState(UUserWidget* Widget)
{
    if (TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget()) SlateWidget->SetEnabled(Widget->GetIsEnabled());
    if (!Widget->WidgetTree) return;
    Widget->WidgetTree->ForEachWidget([](UWidget* Child)
    {
        if (TSharedPtr<SWidget> SlateWidget = Child->GetCachedWidget()) SlateWidget->SetEnabled(Child->GetIsEnabled());
        if (UUserWidget* Nested = Cast<UUserWidget>(Child)) MatchSlateEnabledState(Nested);
    });
}

bool Render(FWidgetRenderer& Renderer, UUserWidget* HUD, const FIntPoint& Size, const FString& Filename, FString& Error, bool CheckCenter)
{
    const float DPI = FMath::Max(.01f, GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size));
    TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, false));
    if (!Target.IsValid()) { Error = TEXT("Blackjack preview render target could not be created."); return false; }
    Target->InitCustomFormat(Size.X, Size.Y, PF_FloatRGBA, true);
    Target->UpdateResourceImmediate(true);
    MatchDesignerState(HUD);
    FinishPreviewResources(HUD);
    // UMG keeps weak Slate references. Retain the root after DrawWidget's
    // temporary virtual window is destroyed so its painted geometry survives.
    const TSharedRef<SWidget> SlateRoot = HUD->TakeWidget();
    MatchSlateEnabledState(HUD);
    Renderer.DrawWidget(Target.Get(), SlateRoot, DPI, FVector2D(Size), 0.f, false);
    FlushRenderingCommands();
    FImage Pixels;
    if (!FImageUtils::GetRenderTargetImage(Target.Get(), Pixels))
    { Error = TEXT("Blackjack preview render readback failed."); return false; }
    // Slate readback is premultiplied. Save straight alpha so the PNG can be
    // composited over a game screenshot without making the panels too dark.
    FImage LinearPixels;
    Pixels.CopyTo(LinearPixels, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
    for (FLinearColor& Pixel : LinearPixels.AsRGBA32F())
        if (Pixel.A > .0001f) { Pixel.R /= Pixel.A; Pixel.G /= Pixel.A; Pixel.B /= Pixel.A; }
    FImage Encoded;
    LinearPixels.CopyTo(Encoded, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    const TArrayView64<const FColor> Colors = Encoded.AsBGRA8();
    const FColor Center = Colors[(Size.Y / 2) * Size.X + Size.X / 2];
    if (CheckCenter && Center.A != 0)
    { Error = TEXT("Blackjack preview covered the dealer/table center."); return false; }
    int64 VisibleCount = 0;
    for (const FColor& Color : Colors) if (Color.A > 0) ++VisibleCount;
    if (VisibleCount < Size.X * Size.Y / 100)
    { Error = TEXT("Blackjack preview unexpectedly rendered empty."); return false; }
    if (!FImageUtils::SaveImageByExtension(*Filename, Encoded))
    { Error = TEXT("Blackjack preview image could not save: ") + Filename; return false; }
    // Preserve the PNG even when geometry fails so the failing state is reviewable.
    if (CheckCenter && !CheckCardGeometry(HUD, Error)) return false;
    HUD->ReleaseSlateResources(true);
    return true;
}
}

bool RenderCasinoBlackjackPreviews(FString& OutError)
{
    OutError.Reset();
    if (IsRunningCommandlet() || !FSlateApplication::IsInitialized() || !GDynamicRHI)
    { OutError = TEXT("Blackjack previews require the editor with a real rendering device."); return false; }
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup/BlackjackPreviews");
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    { OutError = TEXT("Blackjack preview directory could not create."); return false; }
    FWidgetRenderer* Renderer = new FWidgetRenderer(false, true);
    ON_SCOPE_EXIT { FlushRenderingCommands(); BeginCleanup(Renderer); };
    const FIntPoint Sizes[] = { FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(1440, 1080), FIntPoint(1280, 720) };
    const TCHAR* Variants[] = { TEXT("Playing"), TEXT("Split"), TEXT("Insurance"), TEXT("LongHands"),
        TEXT("SingleLongHand"), TEXT("Settled"), TEXT("Betting") };
    for (const FIntPoint& Size : Sizes)
    {
        for (const TCHAR* Variant : Variants)
        {
            TStrongObjectPtr<UUserWidget> HUD(CasinoBlackjackPreview::Create(CasinoBlackjackPreview::HUDPath, OutError));
            if (!HUD.IsValid() || !CasinoBlackjackPreview::Sample(HUD.Get(), Variant, OutError)) return false;
            const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"), Variant, Size.X, Size.Y);
            // Betting uses the preexisting shared keypad, which intentionally
            // covers its active entry area; only gameplay overlays check center.
            if (!CasinoBlackjackPreview::Render(*Renderer, HUD.Get(), Size, Filename, OutError,
                FString(Variant) != TEXT("Betting"))) return false;
        }
    }
    UE_LOG(LogCasinoBlackjackPreview, Display, TEXT("Saved 28 blackjack UI previews with bounded card/result geometry in %s"), *Directory);
    return true;
}

bool RenderCasinoKeypadPreviews(FString& OutError)
{
    OutError.Reset();
    if (IsRunningCommandlet() || !FSlateApplication::IsInitialized() || !GDynamicRHI)
    { OutError = TEXT("Keypad previews require an editor rendering device."); return false; }
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup/KeypadPreviews");
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    { OutError = TEXT("Could not create keypad preview directory."); return false; }
    FWidgetRenderer* Renderer = new FWidgetRenderer(false, true);
    ON_SCOPE_EXIT { FlushRenderingCommands(); BeginCleanup(Renderer); };
    const TCHAR* Paths[] = {TEXT("/Game/1_BluePrint/Widget/WBP_Calculate.WBP_Calculate_C"),
        TEXT("/Game/LJH/UI/WBP_SlotMachineControl.WBP_SlotMachineControl_C")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Paths); ++Index)
    {
        for (const FIntPoint& Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720)})
        {
            TStrongObjectPtr<UUserWidget> Widget(CasinoBlackjackPreview::Create(Paths[Index], OutError));
            if (!Widget.IsValid()) return false;
            if (UWidget* Grid = Widget->GetWidgetFromName(TEXT("SlotKeypadGrid")))
                Grid->SetVisibility(ESlateVisibility::Visible);
            ApplyCasinoKeypadStyle(Widget.Get());
            Widget->WidgetTree->ForEachWidget([](UWidget* Child)
            {
                Child->bHiddenInDesigner = Child->GetVisibility() == ESlateVisibility::Collapsed
                    || Child->GetVisibility() == ESlateVisibility::Hidden;
            });
            const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"),
                Index == 0 ? TEXT("Shared") : TEXT("Slot"), Size.X, Size.Y);
            if (!CasinoBlackjackPreview::Render(*Renderer, Widget.Get(), Size, Filename, OutError, false)) return false;
        }
    }
    UE_LOG(LogCasinoBlackjackPreview, Display, TEXT("Saved 4 shared/slot keypad previews; no game actions executed."));
    return true;
}

void RegisterCasinoBlackjackPreviewCommands()
{
    CasinoBlackjackPreview::PreviewCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("CasinoBlackjack.RenderPreviews"), TEXT("Render designer-only blackjack HUD/card previews; no gameplay actions run."),
        FConsoleCommandDelegate::CreateLambda([]
        {
            FString Error;
            if (!RenderCasinoBlackjackPreviews(Error)) UE_LOG(LogCasinoBlackjackPreview, Error, TEXT("%s"), *Error);
        }));
    CasinoBlackjackPreview::KeypadPreviewCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("CasinoKeypad.RenderPreviews"), TEXT("Render shared/slot keypads in design mode without betting or spinning."),
        FConsoleCommandDelegate::CreateLambda([]
        {
            FString Error;
            if (!RenderCasinoKeypadPreviews(Error)) UE_LOG(LogCasinoBlackjackPreview, Error, TEXT("%s"), *Error);
        }));
}

void UnregisterCasinoBlackjackPreviewCommands()
{
    if (CasinoBlackjackPreview::PreviewCommand)
        IConsoleManager::Get().UnregisterConsoleObject(CasinoBlackjackPreview::PreviewCommand);
    CasinoBlackjackPreview::PreviewCommand = nullptr;
    if (CasinoBlackjackPreview::KeypadPreviewCommand)
        IConsoleManager::Get().UnregisterConsoleObject(CasinoBlackjackPreview::KeypadPreviewCommand);
    CasinoBlackjackPreview::KeypadPreviewCommand = nullptr;
}
