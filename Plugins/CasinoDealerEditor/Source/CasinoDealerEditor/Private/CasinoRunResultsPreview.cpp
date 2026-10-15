#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "DynamicRHI.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderDeferredCleanup.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UI/CasinoRunResultWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "casino_loop_gamestate.h"

namespace CasinoRunResultsPreview
{
template <typename T = UWidget>
T* Find(UUserWidget* Widget, const TCHAR* Name)
{
    return Widget && Widget->WidgetTree ? Cast<T>(Widget->WidgetTree->FindWidget(FName(Name))) : nullptr;
}

void PrepareDesigner(UUserWidget* Widget)
{
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    UBlueprintGeneratedClass::UnbindDynamicDelegates(Widget->GetClass(), Widget);
    const auto ClearDelegates = [](UWidget* Child)
    {
        Child->SetDesignerFlags(EWidgetDesignFlags::Designing);
        for (TFieldIterator<FProperty> It(Child->GetClass()); It; ++It)
        {
            if (FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
                Delegate->ClearDelegate(Child, Delegate->ContainerPtrToValuePtr<void>(Child));
            else if (FDelegateProperty* Singlecast = CastField<FDelegateProperty>(*It))
                Singlecast->ContainerPtrToValuePtr<FScriptDelegate>(Child)->Unbind();
        }
    };
    ClearDelegates(Widget);
    if (Widget->WidgetTree)
        Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
        {
            ClearDelegates(Child);
            if (UUserWidget* Nested = Cast<UUserWidget>(Child)) PrepareDesigner(Nested);
        });
}

FCasinoRunResults SampleResults(bool bSingleDay)
{
    FCasinoRunResults Results;
    Results.bFinalized = true;
    Results.Days = bSingleDay ? TArray<int32>{1} : TArray<int32>{1, 2, 3, 4};
    const TCHAR* Names[] = {
        TEXT("친구들과 함께하는 아주 긴 플레이어 이름 테스트"),
        TEXT("PLAYER TWO WITH A VERY LONG NAME"), TEXT("플레이어 3"), TEXT("Player Four")
    };
    const int64 Last[] = {8765432100LL, 8765432100LL, 5234567890LL, 0LL};
    const int32 Ranks[] = {1, 1, 3, 4};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        FCasinoRunPlayerResult& Player = Results.Players.AddDefaulted_GetRef();
        Player.RunPlayerId = Index + 1;
        Player.PlayerName = Names[Index];
        Player.FinalBalance = Last[Index];
        Player.TotalContribution = int64(Index + 1) * 1234567890LL;
        Player.Rank = Ranks[Index];
        Player.bDisconnected = Index == 3;
        for (const int32 Day : Results.Days)
        {
            FCasinoRunBalanceSnapshot& Snapshot = Player.DailyBalances.AddDefaulted_GetRef();
            Snapshot.Day = Day;
            Snapshot.Balance = Day == Results.Days.Last() ? Player.FinalBalance
                : int64((Day + Index) % 4 + 1) * 1357900000LL;
        }
    }
    return Results;
}

bool Visible(UWidget* Widget)
{
    return Widget && Widget->GetVisibility() != ESlateVisibility::Collapsed
        && Widget->GetVisibility() != ESlateVisibility::Hidden;
}

void ApplyRuntimeVisibility(UUserWidget* Widget)
{
    // Design mode deliberately binds Slate visibility to GetVisibilityInDesigner,
    // which makes even Collapsed gameplay controls visible. Preserve designer
    // isolation from Blueprint events while painting the actual runtime stage.
    const auto Sync = [](UWidget* Child)
    {
        const TSharedPtr<SWidget> Slate = Child->GetCachedWidget();
        if (!Slate) return;
        EVisibility Visibility = EVisibility::Visible;
        switch (Child->GetVisibility())
        {
        case ESlateVisibility::Collapsed: Visibility = EVisibility::Collapsed; break;
        case ESlateVisibility::Hidden: Visibility = EVisibility::Hidden; break;
        case ESlateVisibility::HitTestInvisible: Visibility = EVisibility::HitTestInvisible; break;
        case ESlateVisibility::SelfHitTestInvisible: Visibility = EVisibility::SelfHitTestInvisible; break;
        default: break;
        }
        Slate->SetVisibility(Visibility);
    };
    Sync(Widget);
    if (Widget->WidgetTree)
        Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
        {
            Sync(Child);
            if (UUserWidget* Nested = Cast<UUserWidget>(Child)) ApplyRuntimeVisibility(Nested);
        });
}

FSlateRect Bounds(UWidget* Widget)
{
    if (!Widget) return FSlateRect();
    const FGeometry Geometry = Widget->GetPaintSpaceGeometry();
    const FVector2D Start = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D End = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
    return FSlateRect(Start.X, Start.Y, End.X, End.Y);
}

bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
{
    constexpr float Tolerance = 2.f;
    return Inner.Right > Inner.Left && Inner.Bottom > Inner.Top
        && Inner.Left >= Outer.Left - Tolerance && Inner.Top >= Outer.Top - Tolerance
        && Inner.Right <= Outer.Right + Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
}

bool Overlaps(const FSlateRect& A, const FSlateRect& B)
{
    return A.Left < B.Right - 1.f && A.Right > B.Left + 1.f
        && A.Top < B.Bottom - 1.f && A.Bottom > B.Top + 1.f;
}

bool Render(FWidgetRenderer& Renderer, UCasinoRunResultWidget* Widget, const TSharedRef<SWidget>& SlateRoot, FIntPoint Size,
    const FString& Filename, FAutomationTestBase& Test)
{
    const float Scale = FMath::Max(.01f, GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size));
    const FVector2D DrawSize(Size.X, Size.Y);
    TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
    if (!Test.TestNotNull(TEXT("Result preview render target"), Target.Get())) return false;
    ApplyRuntimeVisibility(Widget);
    Renderer.DrawWidget(Target.Get(), SlateRoot, Scale, DrawSize, 0.f, false);
    FlushRenderingCommands();
    FImage Image;
    if (!Test.TestTrue(TEXT("Result preview readback"), FImageUtils::GetRenderTargetImage(Target.Get(), Image))) return false;
    FImage Png;
    Image.CopyTo(Png, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    if (!Test.TestTrue(TEXT("Result preview PNG written"), FImageUtils::SaveImageByExtension(*Filename, Png))) return false;
    Test.AddInfo(FString::Printf(TEXT("Result preview: %s (project DPI %.3f)"), *Filename, Scale));
    return true;
}

bool CheckPresentation(UCasinoRunResultWidget* Widget, const FCasinoRunResults& Results,
    bool bComplete, FAutomationTestBase& Test)
{
    UPanelWidget* Rows = Find<UPanelWidget>(Widget, TEXT("CasinoRunRankingRows"));
    UWidget* Ranking = Find(Widget, TEXT("CasinoRunRankingPanel"));
    UWidget* Graph = Find(Widget, TEXT("CasinoRunGraphBounds"));
    UWidget* Panel = Find(Widget, TEXT("CasinoRunResultsPanel"));
    UWidget* Frame = Find(Widget, TEXT("CasinoDayFrame"));
    UWidget* Restart = Find(Widget, TEXT("RestartButton"));
    UWidget* Menu = Find(Widget, TEXT("MainMenuButton"));
    if (!Test.TestNotNull(TEXT("Result ranking rows"), Rows)
        || !Test.TestNotNull(TEXT("Result ranking panel"), Ranking)
        || !Test.TestNotNull(TEXT("Result graph bounds"), Graph)
        || !Test.TestNotNull(TEXT("Result summary panel"), Panel)
        || !Test.TestNotNull(TEXT("Existing result frame"), Frame)
        || !Test.TestNotNull(TEXT("Existing restart action"), Restart)
        || !Test.TestNotNull(TEXT("Existing main-menu action"), Menu)) return false;
    Test.TestEqual(TEXT("All four result participants have a ranking row"), Rows->GetChildrenCount(), Results.Players.Num());
    Test.TestEqual(TEXT("Graph appears only when at least two days were recorded"), Visible(Graph), Results.Days.Num() >= 2);
    Test.TestEqual(TEXT("Ranking waits until graph animation completes"), Visible(Ranking), bComplete);
    Test.TestEqual(TEXT("Restart waits until graph animation completes"), Visible(Restart), bComplete);
    Test.TestEqual(TEXT("Main menu waits until graph animation completes"), Visible(Menu), bComplete);
    Test.TestEqual(TEXT("Painted restart visibility matches result stage"),
        Restart->GetCachedWidget().IsValid() && Restart->GetCachedWidget()->GetVisibility().IsVisible(), bComplete);
    Test.TestEqual(TEXT("Painted main-menu visibility matches result stage"),
        Menu->GetCachedWidget().IsValid() && Menu->GetCachedWidget()->GetVisibility().IsVisible(), bComplete);
    UWidget* Root = Widget->WidgetTree->RootWidget;
    const FSlateRect RootRect = Bounds(Root);
    const auto Describe = [](const FSlateRect& Rect)
    { return FString::Printf(TEXT("[%.1f,%.1f-%.1f,%.1f]"), Rect.Left, Rect.Top, Rect.Right, Rect.Bottom); };
    Test.AddInfo(FString::Printf(TEXT("Painted bounds: viewport%s frame%s panel%s graph%s rank%s restart%s menu%s"),
        *Describe(RootRect), *Describe(Bounds(Frame)), *Describe(Bounds(Panel)), *Describe(Bounds(Graph)),
        *Describe(Bounds(Ranking)), *Describe(Bounds(Restart)), *Describe(Bounds(Menu))));
    Test.TestTrue(TEXT("Result frame stays inside viewport"), Contains(RootRect, Bounds(Frame)));
    Test.TestTrue(TEXT("Result content stays inside viewport"), Contains(RootRect, Bounds(Panel)));
    Test.TestTrue(TEXT("Result frame contains summary content"), Contains(Bounds(Frame), Bounds(Panel)));
    if (Results.Days.Num() >= 2)
    {
        Test.TestTrue(TEXT("Graph has usable painted width"), Bounds(Graph).Right - Bounds(Graph).Left > 300.f);
        Test.TestTrue(TEXT("Graph stays inside result content"), Contains(Bounds(Panel), Bounds(Graph)));
    }
    if (bComplete)
    {
        Test.TestTrue(TEXT("Ranking stays inside result content"), Contains(Bounds(Panel), Bounds(Ranking)));
        Test.TestTrue(TEXT("Restart action stays inside viewport"), Contains(RootRect, Bounds(Restart)));
        Test.TestTrue(TEXT("Main-menu action stays inside viewport"), Contains(RootRect, Bounds(Menu)));
        Test.TestTrue(TEXT("Result frame contains restart action"), Contains(Bounds(Frame), Bounds(Restart)));
        Test.TestTrue(TEXT("Result frame contains main-menu action"), Contains(Bounds(Frame), Bounds(Menu)));
        Test.TestFalse(TEXT("Result ranking does not cover restart"), Overlaps(Bounds(Ranking), Bounds(Restart)));
        Test.TestFalse(TEXT("Result ranking does not cover main menu"), Overlaps(Bounds(Ranking), Bounds(Menu)));
        if (Results.Days.Num() >= 2)
            Test.TestFalse(TEXT("Graph and ranking occupy separate rows"), Overlaps(Bounds(Graph), Bounds(Ranking)));
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoRunResultsPresentationTest, "Casino.UI.RunResults.Presentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoRunResultsPresentationTest::RunTest(const FString& Parameters)
{
    using namespace CasinoRunResultsPreview;
    if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized() || !GDynamicRHI
        || GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Null)
    {
        AddWarning(TEXT("Run-result presentation needs an editor rendering RHI; no visual assertions ran."));
        return true;
    }
    UClass* Class = LoadClass<UCasinoRunResultWidget>(nullptr, TEXT("/Game/Blackjack/WBP/WBP_Result.WBP_Result_C"));
    if (!TestNotNull(TEXT("Actual result Blueprint inherits the native result presenter"), Class)) return false;
    const FString SavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    const FString Directory = FPaths::ConvertRelativePathToFull(SavedRoot / TEXT("CasinoDealerSetup/RunResultsPreviews"));
    if (!TestTrue(TEXT("Preview output stays within Saved and is writable"),
        Directory.StartsWith(SavedRoot, ESearchCase::IgnoreCase) && IFileManager::Get().MakeDirectory(*Directory, true))) return false;
    FWidgetRenderer* Renderer = new FWidgetRenderer(false, true);
    ON_SCOPE_EXIT { FlushRenderingCommands(); BeginCleanup(Renderer); };
    if (!TestTrue(TEXT("Result Slate renderer available"), Renderer->GetSlateRenderer() != nullptr)) return false;

    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720)})
    {
        for (int32 Variant = 0; Variant < 3; ++Variant)
        {
            const bool bSingleDay = Variant == 0;
            const bool bComplete = Variant != 1;
            const FCasinoRunResults Results = SampleResults(bSingleDay);
            TStrongObjectPtr<UCasinoRunResultWidget> Widget(NewObject<UCasinoRunResultWidget>(
                GetTransientPackage(), Class, NAME_None, RF_Transient));
            Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
            if (!TestTrue(TEXT("Actual result designer instance initializes"), Widget->Initialize())) return false;
            PrepareDesigner(Widget.Get());
            Widget->LoopStatus.Phase = bSingleDay ? ECasinoLoopPhase::GameOver : ECasinoLoopPhase::Cleared;
            Widget->LoopStatus.CurrentDay = Results.Days.Last();
            Widget->bHasLoopStatus = true;
            // UWidget caches weak Slate references. Retain the root through the
            // paint and geometry assertions, after the renderer's virtual window
            // has been destroyed; otherwise every cached geometry becomes null.
            TSharedRef<SWidget> SlateRoot = Widget->TakeWidget();
            Widget->SetResultsForPreview(Results);
            // A single recorded day must reveal rankings/actions immediately,
            // even when graph progress has not reached its completion value.
            Widget->SetPreviewAnimationProgress(bSingleDay ? .25f : bComplete ? 1.f : .5f);
            if (UTextBlock* Title = Find<UTextBlock>(Widget.Get(), TEXT("ResultTitleText")))
                Title->SetText(FText::FromString(bSingleDay ? TEXT("목표 달성 실패") : TEXT("카지노 클리어")));
            if (UTextBlock* Summary = Find<UTextBlock>(Widget.Get(), TEXT("ResultSummaryText")))
                Summary->SetText(FText::FromString(bSingleDay ? TEXT("DAY 01 · 상납 마감 후 최종 순위") : TEXT("DAY 04 · 상납 후 잔액 기록")));
            const TCHAR* VariantName = bSingleDay ? TEXT("Day1_Failed_Ranking") : bComplete ? TEXT("Day4_Final_RankingGraph") : TEXT("Day4_GraphDrawing");
            const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"), VariantName, Size.X, Size.Y);
            if (!Render(*Renderer, Widget.Get(), SlateRoot, Size, Filename, *this)) return false;
            if (!CheckPresentation(Widget.Get(), Results, bComplete, *this)) return false;
            Widget->ReleaseSlateResources(true);
        }
    }

    // The Result screen can open before the final replicated payload arrives.
    // Exercise its real GameState event binding on one persistent widget instance.
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ACasinoLoopGameState* State = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(State);
    TStrongObjectPtr<UCasinoRunResultWidget> Live(CreateWidget<UCasinoRunResultWidget>(World, UCasinoRunResultWidget::StaticClass()));
    if (!TestNotNull(TEXT("Native result view for late-data delivery"), Live.Get())) return false;
    TSharedRef<SWidget> LiveSlateRoot = Live->TakeWidget();
    Live->RefreshResultsFromGameState();
    TestTrue(TEXT("Missing final data displays waiting state"), Visible(Find(Live.Get(), TEXT("CasinoRunResultsWaiting"))));
    TestFalse(TEXT("Waiting view does not expose ranking"), Visible(Find(Live.Get(), TEXT("CasinoRunRankingPanel"))));
    FCasinoLoopStatus FinalStatus;
    FinalStatus.Phase = ECasinoLoopPhase::Cleared;
    FinalStatus.CurrentDay = 4;
    State->SetLoopStatus(FinalStatus);
    State->SetRunResults(SampleResults(false));
    TestFalse(TEXT("Late GameState result event removes waiting state"), Visible(Find(Live.Get(), TEXT("CasinoRunResultsWaiting"))));
    UPanelWidget* LiveRows = Find<UPanelWidget>(Live.Get(), TEXT("CasinoRunRankingRows"));
    if (!TestNotNull(TEXT("Same result view receives late participant rows"), LiveRows)) return false;
    TestEqual(TEXT("Late delivery creates four ranking rows without reopening screen"), LiveRows->GetChildrenCount(), 4);
    TestFalse(TEXT("Late delivery starts graph before ranking"), Visible(Find(Live.Get(), TEXT("CasinoRunRankingPanel"))));
    Live->SetPreviewAnimationProgress(1.f);
    TestTrue(TEXT("Late delivery reveals ranking after graph"), Visible(Find(Live.Get(), TEXT("CasinoRunRankingPanel"))));
    State->SetRunResults(FCasinoRunResults());
    TestTrue(TEXT("Run reset returns same view to waiting"), Visible(Find(Live.Get(), TEXT("CasinoRunResultsWaiting"))));
    TestFalse(TEXT("Run reset hides previous ranking"), Visible(Find(Live.Get(), TEXT("CasinoRunRankingPanel"))));
    Live->ReleaseSlateResources(true);
    return true;
}

#endif
