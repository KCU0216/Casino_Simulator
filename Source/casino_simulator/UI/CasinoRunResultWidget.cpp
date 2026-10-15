#include "UI/CasinoRunResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace CasinoRunResultStyle
{
const FLinearColor Gold(.83f, .67f, .34f);
const FLinearColor Ivory(.93f, .91f, .85f);
const FLinearColor Muted(.56f, .63f, .72f);
const FLinearColor Navy(.025f, .045f, .075f, .88f);
constexpr float AnimationSeconds = 3.f;

FLinearColor PlayerColor(int32 Index)
{
    static const FLinearColor Colors[] = {
        FLinearColor(.95f,.75f,.36f), FLinearColor(.30f,.84f,.74f),
        FLinearColor(.45f,.69f,1.f), FLinearColor(.97f,.48f,.47f),
        FLinearColor(.76f,.58f,1.f), FLinearColor(.97f,.64f,.37f),
        FLinearColor(.66f,.85f,.40f), FLinearColor(.43f,.86f,.96f)};
    return Colors[Index % UE_ARRAY_COUNT(Colors)];
}

FString DisplayName(const FCasinoRunPlayerResult& Player)
{
    FString Name = Player.PlayerName.TrimStartAndEnd();
    if (Name.IsEmpty()) Name = FString::Printf(TEXT("플레이어 %d"), Player.RunPlayerId);
    return Name;
}

UTextBlock* Text(UWidgetTree* Tree, FName Name, const FText& Value, int32 Size,
    FLinearColor Color = Ivory)
{
    UTextBlock* Widget = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Widget->SetText(Value);
    Widget->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
    Widget->SetColorAndOpacity(Color);
    Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
    Widget->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    return Widget;
}

void Row(UVerticalBox* Parent, UWidget* Child, float Bottom = 10.f)
{
    UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
    Slot->SetHorizontalAlignment(HAlign_Fill);
    Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
}

void Cell(UWidgetTree* Tree, UHorizontalBox* Parent, UWidget* Child, float Width, bool bFill = false)
{
    USizeBox* Bounds = Tree->ConstructWidget<USizeBox>();
    if (!bFill) Bounds->SetWidthOverride(Width);
    Bounds->SetContent(Child);
    UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Bounds);
    Slot->SetVerticalAlignment(VAlign_Center);
    Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    Slot->SetPadding(FMargin(8.f, 6.f));
}

uint32 Signature(const FCasinoRunResults& Results)
{
    uint32 Hash = GetTypeHash(Results.Days.Num());
    for (int32 Day : Results.Days) Hash = HashCombineFast(Hash, GetTypeHash(Day));
    for (const FCasinoRunPlayerResult& Player : Results.Players)
    {
        Hash = HashCombineFast(Hash, GetTypeHash(Player.RunPlayerId));
        Hash = HashCombineFast(Hash, GetTypeHash(Player.PlayerName));
        Hash = HashCombineFast(Hash, GetTypeHash(Player.FinalBalance));
        Hash = HashCombineFast(Hash, GetTypeHash(Player.TotalContribution));
        Hash = HashCombineFast(Hash, GetTypeHash(Player.Rank));
        Hash = HashCombineFast(Hash, GetTypeHash(Player.bDisconnected));
        for (const FCasinoRunBalanceSnapshot& Snapshot : Player.DailyBalances)
        {
            Hash = HashCombineFast(Hash, GetTypeHash(Snapshot.Day));
            Hash = HashCombineFast(Hash, GetTypeHash(Snapshot.Balance));
        }
    }
    return Hash;
}
}

class SCasinoRunBalanceGraph : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCasinoRunBalanceGraph) {} SLATE_END_ARGS()
    void Construct(const FArguments&) { SetCanTick(false); }
    void SetResults(const FCasinoRunResults& InResults)
    { Results = InResults; Invalidate(EInvalidateWidgetReason::Paint); }
    void SetProgress(float Value)
    { Progress = FMath::Clamp(Value, 0.f, 1.f); Invalidate(EInvalidateWidgetReason::Paint); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(960.f, 260.f); }

    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
    {
        using namespace CasinoRunResultStyle;
        if (Results.Days.Num() < 2) return Layer;
        const FVector2D Size = Geometry.GetLocalSize();
        const FSlateRoundedBoxBrush Card(FLinearColor(.015f,.027f,.046f,.78f), 10.f, Gold.CopyWithNewOpacity(.20f), 1.f);
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), &Card, ESlateDrawEffect::None, FLinearColor(.015f,.027f,.046f,.78f));
        const float Left = 94.f, Right = FMath::Max(Left + 10.f, float(Size.X) - 190.f);
        const float Top = 32.f, Bottom = FMath::Max(Top + 10.f, float(Size.Y) - 38.f);
        const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 13);
        const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
        const FSlateFontInfo AmountFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
        const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        auto Label = [&](const FString& Value, FVector2D At, FLinearColor Color, const FSlateFontInfo& InFont)
        {
            FSlateDrawElement::MakeText(Elements, Layer + 6,
                Geometry.ToPaintGeometry(FVector2D(1.f), FSlateLayoutTransform(At)),
                Value, InFont, ESlateDrawEffect::None, Color);
        };
        auto Line = [&](const TArray<FVector2D>& Points, FLinearColor Color, float Thickness, int32 Offset = 0)
        {
            FSlateDrawElement::MakeLines(Elements, Layer + Offset, Geometry.ToPaintGeometry(),
                Points, ESlateDrawEffect::None, Color, true, Thickness);
        };
        auto Circle = [&](FVector2D Center, float Radius, FLinearColor Color, int32 Offset)
        {
            const FSlateRoundedBoxBrush Brush(Color, Radius);
            FSlateDrawElement::MakeBox(Elements, Layer + Offset,
                Geometry.ToPaintGeometry(FVector2D(Radius * 2.f), FSlateLayoutTransform(Center - FVector2D(Radius))),
                &Brush, ESlateDrawEffect::None, Color);
        };
        auto CompactAmount = [](double Value)
        {
            FString Amount;
            if (Value >= 1.e12) Amount = FString::Printf(TEXT("%.1f조"), Value / 1.e12);
            else if (Value >= 1.e8) Amount = FString::Printf(TEXT("%.1f억"), Value / 1.e8);
            else if (Value >= 1.e4) Amount = FString::Printf(TEXT("%.1f만"), Value / 1.e4);
            else Amount = FText::AsNumber(int64(FMath::RoundToDouble(Value))).ToString();
            Amount.ReplaceInline(TEXT(".0"), TEXT(""));
            return Amount;
        };
        Label(TEXT("잔액 · 원"), FVector2D(16.f, 10.f), Gold.CopyWithNewOpacity(.85f), AmountFont);
        double Maximum = 1.0;
        for (const FCasinoRunPlayerResult& Player : Results.Players)
            for (const FCasinoRunBalanceSnapshot& Snapshot : Player.DailyBalances)
                Maximum = FMath::Max(Maximum, double(Snapshot.Balance));
        const double Magnitude = FMath::Pow(10.0, FMath::FloorToDouble(FMath::LogX(10.0, Maximum)));
        const double Normalized = Maximum / Magnitude;
        Maximum = (Normalized <= 1.0 ? 1.0 : Normalized <= 2.0 ? 2.0 : Normalized <= 5.0 ? 5.0 : 10.0) * Magnitude;
        const int32 FirstDay = Results.Days[0], LastDay = Results.Days.Last();
        auto X = [&](int32 Day) { return Left + (Right - Left) * float(Day - FirstDay) / FMath::Max(1, LastDay - FirstDay); };
        auto Y = [&](double Balance) { return Bottom - (Bottom - Top) * FMath::Clamp(Balance / Maximum, 0.0, 1.0); };
        for (int32 Step = 0; Step <= 4; ++Step)
        {
            const double Value = Maximum * Step / 4.0;
            const float AtY = Y(Value);
            Line({FVector2D(Left, AtY), FVector2D(Right, AtY)}, Muted.CopyWithNewOpacity(Step == 0 ? .28f : .12f), 1.f, 1);
            const FString Amount = CompactAmount(Value);
            Label(Amount, FVector2D(Left - Measure->Measure(Amount, Font).X - 12.f, AtY - 8.f), Muted, Font);
        }
        const int32 LabelStride = FMath::Max(1, FMath::DivideAndRoundUp(Results.Days.Num(), 10));
        for (int32 Index = 0; Index < Results.Days.Num(); ++Index)
        {
            Line({FVector2D(X(Results.Days[Index]), Top), FVector2D(X(Results.Days[Index]), Bottom)}, Muted.CopyWithNewOpacity(.07f), 1.f, 1);
            if (Index % LabelStride != 0 && Index != Results.Days.Num() - 1) continue;
            const FString Day = FString::Printf(TEXT("%d일"), Results.Days[Index]);
            Label(Day, FVector2D(X(Results.Days[Index]) - Measure->Measure(Day, Font).X / 2.f, Bottom + 11.f), Muted, Font);
        }
        const float EasedProgress = Progress * Progress * (3.f - 2.f * Progress);
        const float RevealX = FMath::Lerp(Left, Right, EasedProgress);
        struct FEndpoint { FVector2D Point; float LabelY; FString Name; FLinearColor Color; double Balance; };
        TArray<FEndpoint> Endpoints;
        for (int32 Index = 0; Index < Results.Players.Num(); ++Index)
        {
            const FCasinoRunPlayerResult& Player = Results.Players[Index];
            const FLinearColor Color = PlayerColor(Index);
            TArray<FVector2D> Segment;
            FVector2D Endpoint = FVector2D::ZeroVector;
            bool bHasEndpoint = false;
            int32 PreviousDay = INDEX_NONE;
            auto Flush = [&]()
            {
                if (Segment.Num() > 1)
                {
                    Line(Segment, Color.CopyWithNewOpacity(.10f), 8.f, 2);
                    Line(Segment, Color.CopyWithNewOpacity(.24f), 4.5f, 3);
                    Line(Segment, Color, 2.2f, 4);
                }
                Segment.Reset();
            };
            for (const FCasinoRunBalanceSnapshot& Snapshot : Player.DailyBalances)
            {
                const FVector2D Point(X(Snapshot.Day), Y(double(Snapshot.Balance)));
                // Missing snapshots leave a gap rather than inventing an interpolated balance.
                if (PreviousDay != INDEX_NONE && Snapshot.Day != PreviousDay + 1) Flush();
                if (Point.X > RevealX)
                {
                    if (Segment.Num())
                    {
                        const FVector2D Previous = Segment.Last();
                        const float Alpha = (RevealX - Previous.X) / FMath::Max(.01f, float(Point.X - Previous.X));
                        Endpoint = FMath::Lerp(Previous, Point, Alpha);
                        Segment.Add(Endpoint);
                        bHasEndpoint = true;
                    }
                    break;
                }
                Segment.Add(Point);
                Endpoint = Point;
                bHasEndpoint = true;
                PreviousDay = Snapshot.Day;
                Circle(Point, 4.f, Color, 5);
                Circle(Point, 1.8f, Ivory, 5);
            }
            Flush();
            if (bHasEndpoint)
            {
                const double Balance = (Bottom - Endpoint.Y) / (Bottom - Top) * Maximum;
                Endpoints.Add({Endpoint, float(Endpoint.Y), DisplayName(Player), Color, Balance});
            }
        }
        Endpoints.Sort([](const FEndpoint& A, const FEndpoint& B) { return A.Point.Y < B.Point.Y; });
        const float Spacing = FMath::Min(40.f, (Bottom - Top) / FMath::Max(1, Endpoints.Num() - 1));
        float PreviousY = Top - Spacing;
        for (FEndpoint& End : Endpoints)
        { End.LabelY = FMath::Max(float(End.Point.Y), PreviousY + Spacing); PreviousY = End.LabelY; }
        if (Endpoints.Num() && Endpoints.Last().LabelY > Bottom)
        {
            float NextY = Bottom + Spacing;
            for (int32 Index = Endpoints.Num() - 1; Index >= 0; --Index)
            { Endpoints[Index].LabelY = FMath::Min(Endpoints[Index].LabelY, NextY - Spacing); NextY = Endpoints[Index].LabelY; }
        }
        for (FEndpoint& End : Endpoints)
        {
            const float NameX = FMath::Min(float(Size.X) - 170.f, float(End.Point.X) + 16.f);
            const FString FullName = End.Name;
            while (End.Name.Len() > 1 && Measure->Measure(End.Name + TEXT("…"), NameFont).X > 140.f)
                End.Name.LeftChopInline(1);
            if (End.Name != FullName) End.Name += TEXT("…");
            Line({End.Point, FVector2D(NameX, End.LabelY)}, End.Color.CopyWithNewOpacity(.55f), 1.f, 4);
            Circle(End.Point, 7.f, End.Color.CopyWithNewOpacity(.15f), 5);
            Circle(End.Point, 4.f, End.Color, 5);
            Circle(End.Point, 1.8f, Ivory, 5);
            const FSlateRoundedBoxBrush Badge(FLinearColor(.025f,.045f,.07f,.97f), 6.f, End.Color.CopyWithNewOpacity(.38f), 1.f);
            FSlateDrawElement::MakeBox(Elements, Layer + 5,
                Geometry.ToPaintGeometry(FVector2D(158.f, 38.f), FSlateLayoutTransform(FVector2D(NameX, End.LabelY - 19.f))),
                &Badge, ESlateDrawEffect::None, FLinearColor(.025f,.045f,.07f,.97f));
            Label(End.Name, FVector2D(NameX + 9.f, End.LabelY - 15.f), End.Color, NameFont);
            Label(CompactAmount(End.Balance) + TEXT(" 원"), FVector2D(NameX + 9.f, End.LabelY + 3.f), Ivory.CopyWithNewOpacity(.85f), AmountFont);
        }
        return Layer + 7;
    }
private:
    FCasinoRunResults Results;
    float Progress = 0.f;
};

void UCasinoRunBalanceGraph::SetResults(const FCasinoRunResults& InResults)
{ Results = InResults; if (Graph) Graph->SetResults(Results); }
void UCasinoRunBalanceGraph::SetProgress(float InProgress)
{ Progress = FMath::Clamp(InProgress, 0.f, 1.f); if (Graph) Graph->SetProgress(Progress); }
TSharedRef<SWidget> UCasinoRunBalanceGraph::RebuildWidget()
{
    SAssignNew(Graph, SCasinoRunBalanceGraph);
    Graph->SetResults(Results);
    Graph->SetProgress(Progress);
    return Graph.ToSharedRef();
}
void UCasinoRunBalanceGraph::ReleaseSlateResources(bool bReleaseChildren)
{ Super::ReleaseSlateResources(bReleaseChildren); Graph.Reset(); }

void UCasinoRunResultWidget::NativeOnInitialized()
{ Super::NativeOnInitialized(); EnsureResultsPanel(); }
void UCasinoRunResultWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EnsureResultsPanel();
    if (!bPreview) RefreshResultsFromGameState();
}
void UCasinoRunResultWidget::NativeDestruct()
{
    if (ResultsSource) ResultsSource->OnRunResultsChanged.RemoveDynamic(this, &ThisClass::HandleRunResultsChanged);
    ResultsSource = nullptr;
    SetActionVisibility(true);
    Super::NativeDestruct();
}
void UCasinoRunResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bPreview && GetWorld() && ResultsSource != GetWorld()->GetGameState<ACasinoLoopGameState>())
        RefreshResultsFromGameState();
    if (!bPreview && bHasGraph && bHasFinalResults && GraphProgress < 1.f)
    {
        GraphProgress = FMath::Min(1.f, GraphProgress + FMath::Max(0.f, InDeltaTime) / CasinoRunResultStyle::AnimationSeconds);
        if (BalanceGraph) BalanceGraph->SetProgress(GraphProgress);
    }
    ApplyPresentationStage();
}

void UCasinoRunResultWidget::EnsureResultsPanel()
{
    using namespace CasinoRunResultStyle;
    if (ResultsPanel || !WidgetTree) return;
    UVerticalBox* Rows = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("VerticalBox_33")));
    if (!Rows)
    {
        if (WidgetTree->RootWidget) return;
        Rows = WidgetTree->ConstructWidget<UVerticalBox>();
        WidgetTree->RootWidget = Rows;
    }
    if (USizeBox* Bounds = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("CasinoDayBounds"))))
    { Bounds->SetWidthOverride(1040.f); Bounds->ClearMaxDesiredHeight(); }
    if (UTextBlock* Summary = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ResultSummaryText"))))
    { Summary->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 18)); Summary->SetWrapTextAt(940.f); }
    ResultsPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CasinoRunResultsPanel"));
    ResultsPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WaitingText = Text(WidgetTree, TEXT("CasinoRunResultsWaiting"), FText::FromString(TEXT("최종 기록을 불러오는 중…")), 20, Muted);
    WaitingText->SetJustification(ETextJustify::Center);
    Row(ResultsPanel, WaitingText, 14.f);
    GraphHeading = Text(WidgetTree, TEXT("CasinoRunGraphHeading"), FText::FromString(TEXT("상납 후 잔액 변화")), 20, Gold);
    Row(ResultsPanel, GraphHeading, 4.f);
    GraphBounds = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CasinoRunGraphBounds"));
    GraphBounds->SetHeightOverride(260.f);
    BalanceGraph = WidgetTree->ConstructWidget<UCasinoRunBalanceGraph>(UCasinoRunBalanceGraph::StaticClass(), TEXT("CasinoRunBalanceGraph"));
    BalanceGraph->SetVisibility(ESlateVisibility::HitTestInvisible);
    GraphBounds->SetContent(BalanceGraph);
    Row(ResultsPanel, GraphBounds, 8.f);
    GraphLegend = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("CasinoRunGraphLegend"));
    GraphLegend->SetExplicitWrapSize(true);
    GraphLegend->SetWrapSize(940.f);
    GraphLegend->SetInnerSlotPadding(FVector2D(16.f, 5.f));
    Row(ResultsPanel, GraphLegend, 8.f);
    GraphHint = Text(WidgetTree, TEXT("CasinoRunGraphHint"), FText::GetEmpty(), 14, Muted);
    Row(ResultsPanel, GraphHint, 12.f);
    RankingPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CasinoRunRankingPanel"));
    Row(RankingPanel, Text(WidgetTree, TEXT("CasinoRunRankHeading"), FText::FromString(TEXT("최종 잔액 순위")), 20, Gold), 6.f);
    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    Cell(WidgetTree, Header, Text(WidgetTree, NAME_None, FText::FromString(TEXT("순위")), 14, Muted), 48.f);
    Cell(WidgetTree, Header, Text(WidgetTree, NAME_None, FText::FromString(TEXT("플레이어")), 14, Muted), 0.f, true);
    for (const TCHAR* Caption : {TEXT("최종 잔액"), TEXT("총 상납액")})
    {
        UTextBlock* Label = Text(WidgetTree, NAME_None, FText::FromString(Caption), 14, Muted);
        Label->SetJustification(ETextJustify::Right);
        Cell(WidgetTree, Header, Label, 178.f);
    }
    Row(RankingPanel, Header, 3.f);
    USizeBox* TableBounds = WidgetTree->ConstructWidget<USizeBox>();
    TableBounds->SetMaxDesiredHeight(210.f);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    RankingRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CasinoRunRankingRows"));
    Scroll->AddChild(RankingRows);
    TableBounds->SetContent(Scroll);
    Row(RankingPanel, TableBounds, 4.f);
    Row(RankingPanel, Text(WidgetTree, TEXT("CasinoRunRankNote"), FText::FromString(TEXT("잔액이 같으면 공동 순위 · 총 상납액은 실제 납부한 금액")), 13, Muted), 4.f);
    Row(ResultsPanel, RankingPanel, 10.f);

    // Re-add retained children so the insertion works both before and after Slate construction.
    struct FRetainedRow { UWidget* Widget; FMargin Padding; FSlateChildSize Size; EHorizontalAlignment H; EVerticalAlignment V; };
    TArray<FRetainedRow> Retained;
    int32 InsertAt = Rows->GetChildrenCount();
    for (UWidget* Child : Rows->GetAllChildren())
    {
        if (Child->GetFName() == TEXT("RestartButton") || Child->GetFName() == TEXT("MainMenuButton"))
            InsertAt = FMath::Min(InsertAt, Retained.Num());
        UVerticalBoxSlot* ChildSlot = CastChecked<UVerticalBoxSlot>(Child->Slot);
        Retained.Add({Child, ChildSlot->GetPadding(), ChildSlot->GetSize(), ChildSlot->GetHorizontalAlignment(), ChildSlot->GetVerticalAlignment()});
    }
    Rows->ClearChildren();
    for (int32 Index = 0; Index <= Retained.Num(); ++Index)
    {
        if (Index == InsertAt) Row(Rows, ResultsPanel, 8.f);
        if (!Retained.IsValidIndex(Index)) continue;
        const FRetainedRow& Old = Retained[Index];
        UVerticalBoxSlot* RestoredSlot = Rows->AddChildToVerticalBox(Old.Widget);
        RestoredSlot->SetPadding(Old.Padding); RestoredSlot->SetSize(Old.Size);
        RestoredSlot->SetHorizontalAlignment(Old.H); RestoredSlot->SetVerticalAlignment(Old.V);
    }
    ApplyPresentationStage();
}

void UCasinoRunResultWidget::RefreshResultsFromGameState()
{
    EnsureResultsPanel();
    ACasinoLoopGameState* Source = GetWorld() ? GetWorld()->GetGameState<ACasinoLoopGameState>() : nullptr;
    if (Source != ResultsSource)
    {
        if (ResultsSource) ResultsSource->OnRunResultsChanged.RemoveDynamic(this, &ThisClass::HandleRunResultsChanged);
        ResultsSource = Source;
        if (ResultsSource) ResultsSource->OnRunResultsChanged.AddUniqueDynamic(this, &ThisClass::HandleRunResultsChanged);
    }
    if (ResultsSource) ApplyResults(ResultsSource->RunResults);
    else { bHasFinalResults = false; ApplyPresentationStage(); }
}
void UCasinoRunResultWidget::HandleRunResultsChanged()
{ if (ResultsSource) ApplyResults(ResultsSource->RunResults); }
void UCasinoRunResultWidget::SetResultsForPreview(const FCasinoRunResults& Results)
{ bPreview = true; EnsureResultsPanel(); ApplyResults(Results); }
void UCasinoRunResultWidget::SetPreviewAnimationProgress(float Progress)
{
    bPreview = true;
    GraphProgress = FMath::Clamp(Progress, 0.f, 1.f);
    if (BalanceGraph) BalanceGraph->SetProgress(GraphProgress);
    ApplyPresentationStage();
}

void UCasinoRunResultWidget::ApplyResults(const FCasinoRunResults& Results)
{
    using namespace CasinoRunResultStyle;
    if (!Results.bFinalized)
    {
        bHasFinalResults = false; bHasGraph = false; ResultSignature = 0; GraphProgress = 0.f;
        ApplyPresentationStage();
        return;
    }
    const uint32 NewSignature = Signature(Results);
    if (bHasFinalResults && ResultSignature == NewSignature) return;
    bHasFinalResults = true;
    ResultSignature = NewSignature;
    bHasGraph = Results.Days.Num() >= 2;
    GraphProgress = bHasGraph ? 0.f : 1.f;
    if (!ResultsPanel) return;
    BalanceGraph->SetResults(Results);
    BalanceGraph->SetProgress(GraphProgress);
    RankingRows->ClearChildren();
    GraphLegend->ClearChildren();
    for (int32 Index = 0; Index < Results.Players.Num(); ++Index)
    {
        const FCasinoRunPlayerResult& Player = Results.Players[Index];
        const FLinearColor Color = PlayerColor(Index);
        UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>();
        const FString Rank = Player.Rank > 0 ? FString::FromInt(Player.Rank) : TEXT("—");
        Cell(WidgetTree, Cells, Text(WidgetTree, NAME_None, FText::FromString(Rank), 19, Color), 48.f);
        const FString Name = DisplayName(Player) + (Player.bDisconnected ? TEXT(" (퇴장)") : TEXT(""));
        UTextBlock* NameText = Text(WidgetTree, NAME_None, FText::FromString(Name), 17, Color);
        NameText->SetToolTipText(FText::FromString(Name));
        Cell(WidgetTree, Cells, NameText, 0.f, true);
        for (int64 Amount : {Player.FinalBalance, Player.TotalContribution})
        {
            UTextBlock* Number = Text(WidgetTree, NAME_None, FText::AsNumber(Amount), 17);
            Number->SetJustification(ETextJustify::Right);
            Number->SetToolTipText(FText::AsNumber(Amount));
            Cell(WidgetTree, Cells, Number, 178.f);
        }
        UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
        Background->SetBrushColor(Index % 2 == 0 ? Navy : FLinearColor(.045f,.065f,.095f,.6f));
        Background->SetPadding(FMargin(0.f));
        Background->SetContent(Cells);
        Row(RankingRows, Background, 3.f);
        UTextBlock* Legend = Text(WidgetTree, NAME_None, FText::FromString(TEXT("● ") + DisplayName(Player)), 14, Color);
        Legend->SetToolTipText(FText::FromString(Name));
        USizeBox* LegendBounds = WidgetTree->ConstructWidget<USizeBox>();
        LegendBounds->SetWidthOverride(220.f);
        LegendBounds->SetClipping(EWidgetClipping::ClipToBounds);
        LegendBounds->SetContent(Legend);
        GraphLegend->AddChildToWrapBox(LegendBounds);
    }
    if (Results.Players.IsEmpty())
        Row(RankingRows, Text(WidgetTree, NAME_None, FText::FromString(TEXT("기록된 참가자가 없습니다.")), 18, Muted), 8.f);
    ApplyPresentationStage();
}

void UCasinoRunResultWidget::SetActionVisibility(bool bShow)
{
    if (!WidgetTree) return;
    for (const TCHAR* Name : {TEXT("RestartButton"), TEXT("MainMenuButton")})
    {
        UWidget* Action = WidgetTree->FindWidget(Name);
        if (!Action) continue;
        const TWeakObjectPtr<UWidget> Key(Action);
        if (!bShow)
        {
            if (Action->GetVisibility() != ESlateVisibility::Collapsed)
                OriginalActionVisibility.FindOrAdd(Key) = Action->GetVisibility();
            Action->SetVisibility(ESlateVisibility::Collapsed);
        }
        else if (const ESlateVisibility* Original = OriginalActionVisibility.Find(Key))
        { Action->SetVisibility(*Original); OriginalActionVisibility.Remove(Key); }
    }
}
void UCasinoRunResultWidget::ApplyPresentationStage()
{
    const bool bFinished = bHasFinalResults && (!bHasGraph || GraphProgress >= 1.f);
    if (WaitingText) WaitingText->SetVisibility(bHasFinalResults ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    const ESlateVisibility ChartVisibility = bHasFinalResults && bHasGraph ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    if (GraphHeading) GraphHeading->SetVisibility(ChartVisibility);
    if (GraphBounds) GraphBounds->SetVisibility(ChartVisibility);
    if (GraphLegend) GraphLegend->SetVisibility(ChartVisibility);
    if (GraphHint)
    {
        GraphHint->SetVisibility(ChartVisibility);
        GraphHint->SetText(FText::FromString(bFinished
            ? TEXT("매일 상납 마감 후 잔액 · 기록이 없는 날은 선을 연결하지 않음")
            : TEXT("잔액 기록을 확인하는 중…")));
    }
    if (RankingPanel) RankingPanel->SetVisibility(bFinished ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    SetActionVisibility(bFinished);
}
