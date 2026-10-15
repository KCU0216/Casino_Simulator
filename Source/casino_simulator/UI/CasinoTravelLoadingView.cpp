#include "UI/CasinoTravelLoadingView.h"

#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

FCasinoTravelLoadingSnapshot MakeCasinoTravelLoadingSnapshot(
    bool bMapLoaded, bool bLocalReady, bool bServerReady, int32 Ready, int32 Expected)
{
    FCasinoTravelLoadingSnapshot Value;
    const int32 SafeExpected = FMath::Max(0, Expected);
    const int32 SafeReady = FMath::Clamp(Ready, 0, SafeExpected);
    Value.Players = SafeExpected > 0
        ? FText::FromString(FString::Printf(TEXT("플레이어 준비  %d / %d"), SafeReady, SafeExpected))
        : FText::FromString(TEXT("플레이어 연결 확인 중"));
    if (!bMapLoaded)
    {
        Value.Heading = FText::FromString(TEXT("카지노로 이동 중"));
        Value.Detail = FText::FromString(TEXT("게임 공간을 불러오고 있습니다."));
        Value.Progress.Reset(); // Map loading exposes no reliable percentage.
    }
    else if (bLocalReady && bServerReady)
    {
        Value.Heading = FText::FromString(TEXT("모두 준비됐습니다"));
        Value.Detail = FText::FromString(TEXT("함께 게임을 시작합니다."));
        Value.Progress = 1.f;
    }
    else
    {
        Value.Heading = FText::FromString(bLocalReady ? TEXT("다른 플레이어를 기다리는 중") : TEXT("게임 화면 준비 중"));
        Value.Detail = FText::FromString(bLocalReady
            ? TEXT("모든 플레이어가 도착하면 함께 시작합니다.")
            : TEXT("화면과 첫 시작 위치를 준비하고 있습니다."));
        // This bar represents ready players, not an invented map-loading percentage.
        Value.Progress = SafeExpected > 0 ? .8f * static_cast<float>(SafeReady) / SafeExpected : 0.f;
    }
    return Value;
}

void SCasinoTravelLoadingView::Construct(const FArguments& Arguments)
{
    ViewState = Arguments._ViewState;
    const auto State = ViewState;
    const FLinearColor Navy(.007f, .011f, .019f, 1.f);
    const FLinearColor Gold(.76f, .49f, .15f, 1.f);
    const FLinearColor Ivory(.91f, .86f, .75f, 1.f);
    const FLinearColor Muted(.45f, .51f, .60f, 1.f);
    // The editor's default progress texture is blue; tinting it gold produces a dark fill.
    // Keep these texture-free brushes alive with the widget, including on the loading thread.
    ProgressBarStyle = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
    ProgressBarStyle.SetBackgroundImage(FSlateColorBrush(FLinearColor(.035f, .045f, .065f, 1.f)));
    ProgressBarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
    ExitButtonStyle.SetNormal(FSlateColorBrush(Gold));
    ExitButtonStyle.SetHovered(FSlateColorBrush(FLinearColor(.94f, .67f, .29f, 1.f)));
    ExitButtonStyle.SetPressed(FSlateColorBrush(FLinearColor(.58f, .33f, .075f, 1.f)));
    ExitButtonStyle.SetDisabled(FSlateColorBrush(Gold));
    ExitButtonStyle.SetNormalForeground(FSlateColor(Navy));
    ExitButtonStyle.SetHoveredForeground(FSlateColor(Navy));
    ExitButtonStyle.SetPressedForeground(FSlateColor(Navy));
    ExitButtonStyle.SetNormalPadding(FMargin(0.f));
    ExitButtonStyle.SetPressedPadding(FMargin(0.f));
    const FSimpleDelegate ExitHandler = Arguments._OnExit;
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(Navy)
        .Padding(40.f)
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
            [
                SNew(SBox).WidthOverride(640.f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 24)
                    [ SNew(STextBlock).Text(FText::FromString(TEXT("CASINO")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18)).ColorAndOpacity(Gold) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
                    [ SNew(STextBlock).Text_Lambda([State] { return State->Read().Heading; })
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 32)).ColorAndOpacity(Ivory).WrapTextAt(640.f) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 44)
                    [ SNew(STextBlock).Text_Lambda([State] { return State->Read().Detail; })
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18)).ColorAndOpacity(Muted).WrapTextAt(640.f) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)
                    [ SNew(SBox).HeightOverride(6.f)
                        [ SNew(SProgressBar).Style(&ProgressBarStyle)
                            .Percent_Lambda([State] { return State->Read().Progress; }).FillColorAndOpacity(Gold) ] ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(STextBlock).Text_Lambda([State] { return State->Read().Players; })
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16)).ColorAndOpacity(Ivory) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 28, 0, 0).HAlign(HAlign_Left)
                    [ SNew(SButton).ButtonStyle(&ExitButtonStyle)
                        .Visibility_Lambda([State] { return State->Read().bFailed ? EVisibility::Visible : EVisibility::Collapsed; })
                        .ButtonColorAndOpacity(FLinearColor::White).ContentPadding(FMargin(28.f, 12.f))
                        .OnClicked_Lambda([ExitHandler] { ExitHandler.ExecuteIfBound(); return FReply::Handled(); })
                        [ SNew(STextBlock).Text(FText::FromString(TEXT("메인 메뉴로 돌아가기")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16)).ColorAndOpacity(Navy) ] ]
                ]
            ]
        ]
    ];
}
