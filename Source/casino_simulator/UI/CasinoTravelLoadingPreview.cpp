#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UI/CasinoTravelLoadingView.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Layout/Children.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

namespace CasinoTravelLoadingPreview
{
TSharedPtr<SWidget> FindProgressBar(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetType() == TEXT("SProgressBar")) return Widget;
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
        if (TSharedPtr<SWidget> Found = FindProgressBar(Children->GetChildAt(Index))) return Found;
    return nullptr;
}

TSharedPtr<SWidget> FindWidgetType(const TSharedRef<SWidget>& Widget, FName Type)
{
    if (Widget->GetType() == Type) return Widget;
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
        if (TSharedPtr<SWidget> Found = FindWidgetType(Children->GetChildAt(Index), Type)) return Found;
    return nullptr;
}

bool CheckTextBounds(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetType() == TEXT("STextBlock"))
    {
        const FVector2D Allocated = Widget->GetPaintSpaceGeometry().GetLocalSize();
        if (Allocated.X > 0.f && Allocated.Y > 0.f)
        {
            const FVector2D Desired = Widget->GetDesiredSize();
            if (Desired.X > Allocated.X + 1.f || Desired.Y > Allocated.Y + 1.f) return false;
        }
    }
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
        if (!CheckTextBounds(Children->GetChildAt(Index))) return false;
    return true;
}

bool CheckExitContrast(const TSharedRef<SWidget>& Widget, const TArray<FColor>& Pixels, const FIntPoint& Size)
{
    const TSharedPtr<SWidget> Button = FindWidgetType(Widget, TEXT("SButton"));
    if (!Button) return false;
    const FSlateRect Bounds = Button->GetPaintSpaceGeometry().GetRenderBoundingRect();
    if (Bounds.GetSize().X < 100.f || Bounds.GetSize().Y < 20.f) return false;
    const int32 SampleX = FMath::Clamp(FMath::FloorToInt(Bounds.Left + 6.f), 0, Size.X - 1);
    const int32 SampleY = FMath::Clamp(FMath::FloorToInt(Bounds.Top + 6.f), 0, Size.Y - 1);
    const FColor Fill = Pixels[SampleY * Size.X + SampleX];
    if (Fill.R < 100 || Fill.R <= Fill.G + 20 || Fill.G <= Fill.B + 20) return false;
    const TSharedPtr<SWidget> Label = FindWidgetType(Button.ToSharedRef(), TEXT("STextBlock"));
    if (!Label) return false;
    const FSlateRect TextBounds = Label->GetPaintSpaceGeometry().GetRenderBoundingRect();
    int32 DarkTextPixels = 0;
    for (int32 Y = FMath::Max(0, FMath::CeilToInt(TextBounds.Top)); Y < FMath::Min(Size.Y, FMath::FloorToInt(TextBounds.Bottom)); ++Y)
        for (int32 X = FMath::Max(0, FMath::CeilToInt(TextBounds.Left)); X < FMath::Min(Size.X, FMath::FloorToInt(TextBounds.Right)); ++X)
        {
            const FColor Color = Pixels[Y * Size.X + X];
            if (Color.R < 50 && Color.G < 50 && Color.B < 50) ++DarkTextPixels;
        }
    return DarkTextPixels > 10;
}

bool CheckProgressColors(const TSharedRef<SWidget>& Widget, const TArray<FColor>& Pixels, const FIntPoint& Size, float Progress)
{
    const TSharedPtr<SWidget> Bar = FindProgressBar(Widget);
    if (!Bar) return false;
    const FSlateRect Bounds = Bar->GetPaintSpaceGeometry().GetRenderBoundingRect();
    if (Bounds.GetSize().X < 100.f || Bounds.GetSize().Y < 2.f) return false;
    const auto Sample = [&](float Fraction)
    {
        const int32 X = FMath::Clamp(FMath::FloorToInt(Bounds.Left + Bounds.GetSize().X * Fraction), 0, Size.X - 1);
        const int32 Y = FMath::Clamp(FMath::FloorToInt(Bounds.Top + Bounds.GetSize().Y * .5f), 0, Size.Y - 1);
        return Pixels[Y * Size.X + X];
    };
    if (Progress > 0.f)
    {
        const FColor Fill = Sample(Progress * .5f);
        if (Fill.R < 100 || Fill.R <= Fill.G + 20 || Fill.G <= Fill.B + 20) return false;
    }
    if (Progress < 1.f)
    {
        const FColor Track = Sample(Progress + (1.f - Progress) * .5f);
        if (Track.R < 4 || Track.R >= 120 || Track.G >= 120 || Track.B >= 140 || Track.B < Track.R) return false;
    }
    return true;
}

int32 Render()
{
    if (IsRunningCommandlet() || !FSlateApplication::IsInitialized() || !GDynamicRHI || GUsingNullRHI)
    {
        UE_LOG(LogTemp, Error, TEXT("Travel loading previews require an editor with a rendering device."));
        return 0;
    }
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/TravelLoading");
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FCasinoTravelLoadingSnapshot Waiting = MakeCasinoTravelLoadingSnapshot(true, true, false, 2, 4);
    const FCasinoTravelLoadingSnapshot Complete = MakeCasinoTravelLoadingSnapshot(true, true, true, 4, 4);
    FCasinoTravelLoadingSnapshot Failure;
    Failure.Heading = FText::FromString(TEXT("게임에 입장하지 못했습니다"));
    Failure.Detail = FText::FromString(TEXT("플레이어 연결이 중단됐습니다. 메인 메뉴에서 다시 시도해 주세요."));
    Failure.Players = FText::FromString(TEXT("메인 메뉴로 돌아가 다시 참가할 수 있습니다."));
    Failure.Progress = 0.f;
    Failure.bFailed = true;
    const FCasinoTravelLoadingSnapshot States[] = {Waiting, Complete, Failure};
    const TCHAR* Names[] = {TEXT("Waiting"), TEXT("Complete"), TEXT("Failure")};
    const FIntPoint Sizes[] = {FIntPoint(1920, 1080)};
    FWidgetRenderer* Renderer = new FWidgetRenderer(false, true);
    int32 Saved = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(States); ++Index)
    {
        auto State = MakeShared<FCasinoTravelLoadingViewState, ESPMode::ThreadSafe>();
        State->Write(States[Index]);
        const TSharedRef<SWidget> Widget = SNew(SCasinoTravelLoadingView).ViewState(State);
        for (const FIntPoint& Size : Sizes)
        {
            TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, false));
            Renderer->DrawWidget(Target.Get(), Widget, FVector2D(Size), 0.f, false);
            FlushRenderingCommands();
            TArray<FColor> Pixels;
            if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels) || Pixels.Num() != Size.X * Size.Y)
            { UE_LOG(LogTemp, Error, TEXT("Travel loading preview readback failed.")); continue; }
            if (Pixels[(Size.Y / 2) * Size.X + Size.X / 2].A < 250)
            { UE_LOG(LogTemp, Error, TEXT("Travel loading preview does not cover the game viewport.")); continue; }
            TArray64<uint8> Encoded;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Encoded);
            const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"), Names[Index], Size.X, Size.Y);
            const bool bProgressColorsValid = CheckProgressColors(Widget, Pixels, Size, States[Index].Progress.GetValue());
            if (!bProgressColorsValid)
                UE_LOG(LogTemp, Error, TEXT("Travel loading preview requires a visible gold fill and muted navy track: %s"), Names[Index]);
            const bool bTextFits = CheckTextBounds(Widget);
            const bool bExitVisible = !States[Index].bFailed || CheckExitContrast(Widget, Pixels, Size);
            if (!bTextFits || !bExitVisible)
                UE_LOG(LogTemp, Error, TEXT("Travel loading preview requires unclipped text and a readable failure exit button: %s"), Names[Index]);
            if (FFileHelper::SaveArrayToFile(Encoded, *Filename))
            {
                if (bProgressColorsValid && bTextFits && bExitVisible) ++Saved;
            }
            else UE_LOG(LogTemp, Error, TEXT("Travel loading preview could not save: %s"), *Filename);
        }
    }
    BeginCleanup(Renderer);
    UE_LOG(LogTemp, Display, TEXT("Travel loading previews saved %d / 3 snapshots to %s"), Saved, *Directory);
    return Saved;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoTravelLoadingVisualPreviewTest, "Casino.UI.TravelLoading.VisualPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoTravelLoadingVisualPreviewTest::RunTest(const FString& Parameters)
{
    if (IsRunningCommandlet() || !FSlateApplication::IsInitialized() || !GDynamicRHI || GUsingNullRHI)
    {
        AddWarning(TEXT("Visual loading test requires a full editor with a rendering device."));
        return true;
    }
    TestEqual(TEXT("Waiting, completion and failure render opaque readable loading views"), CasinoTravelLoadingPreview::Render(), 3);
    return true;
}
#endif
