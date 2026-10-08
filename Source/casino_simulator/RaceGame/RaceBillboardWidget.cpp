#include "RaceBillboardWidget.h"
#include "RaceManager.h"
#include "RaceRunner.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void URaceBillboardWidget::RefreshFromManager(ARaceManager* Manager)
{
    if (!IsValid(Manager)) return;
    if (RaceManager != Manager)
    {
        RaceManager = Manager;
        bPhaseInitialized = false;
    }
    if (!bPhaseInitialized || CurrentPhase != Manager->Phase)
    {
        bPhaseInitialized = true;
        CurrentPhase = Manager->Phase;
        OnRacePhaseChanged(CurrentPhase);
        OnRaceDataUpdated();
    }

    // Runner stats and portraits may replicate after the phase notification.
    const auto& Runners = Manager->GetRunners();
    for (int32 Index = 0; Index < 6; ++Index)
    {
        auto* Profile = Cast<UTextBlock>(GetWidgetFromName(
            FName(*FString::Printf(TEXT("RaceLane%dProfile"), Index + 1))));
        auto* Portrait = Cast<UImage>(GetWidgetFromName(
            FName(*FString::Printf(TEXT("RaceLane%dPortrait"), Index + 1))));
        if (!Profile && !Portrait) continue;

        ARaceRunner* Runner = Runners.IsValidIndex(Index) ? Runners[Index] : nullptr;
        const bool bHasRunner = IsValid(Runner);
        if (Profile)
        {
            const FText Text = bHasRunner
                ? FText::Format(NSLOCTEXT("Race", "LaneProfile", "이름 : {0}\n나이 : {1}\n배당률 : {2}"),
                    FText::FromString(Runner->Stats.Name), FText::AsNumber(Runner->Stats.Age),
                    FText::AsNumber(Runner->Stats.Odds))
                : FText::GetEmpty();
            if (!Profile->GetText().EqualTo(Text)) Profile->SetText(Text);
        }
        if (Portrait)
        {
            UTexture2D* Texture = bHasRunner ? Runner->GetRunnerPortrait() : nullptr;
            if (Portrait->GetBrush().GetResourceObject() != Texture)
                Portrait->SetBrushFromTexture(Texture);
        }
        if (UWidget* Row = GetWidgetFromName(
            FName(*FString::Printf(TEXT("RaceLane%dRow"), Index + 1))))
        {
            Row->SetVisibility(bHasRunner ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        }
    }
}
