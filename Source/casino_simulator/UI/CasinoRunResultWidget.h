#pragma once

#include "CoreMinimal.h"
#include "UI/CasinoManagedWidget.h"
#include "Components/Widget.h"
#include "CasinoRunResultWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class USizeBox;
class UWrapBox;
class SCasinoRunBalanceGraph;

/** A presentation-only graph. Run history is supplied only after the server finalizes it. */
UCLASS()
class CASINO_SIMULATOR_API UCasinoRunBalanceGraph : public UWidget
{
    GENERATED_BODY()
public:
    void SetResults(const FCasinoRunResults& Results);
    void SetProgress(float InProgress);
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FCasinoRunResults Results;
    float Progress = 0.f;
    TSharedPtr<SCasinoRunBalanceGraph> Graph;
};

/** Adds the run history to WBP_Result while retaining its existing Blueprint actions. */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API UCasinoRunResultWidget : public UCasinoManagedWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Casino|Results")
    void RefreshResultsFromGameState();

    // Presentation fixtures also let editor previews inspect the real result layout.
    void SetResultsForPreview(const FCasinoRunResults& Results);
    void SetPreviewAnimationProgress(float Progress);
    float GetGraphProgress() const { return GraphProgress; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void EnsureResultsPanel();
    void ApplyResults(const FCasinoRunResults& Results);
    void ApplyPresentationStage();
    void SetActionVisibility(bool bShow);
    UFUNCTION() void HandleRunResultsChanged();

    UPROPERTY(Transient) TObjectPtr<UVerticalBox> ResultsPanel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WaitingText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> RankingPanel;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> RankingRows;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> GraphHeading;
    UPROPERTY(Transient) TObjectPtr<USizeBox> GraphBounds;
    UPROPERTY(Transient) TObjectPtr<UCasinoRunBalanceGraph> BalanceGraph;
    UPROPERTY(Transient) TObjectPtr<UWrapBox> GraphLegend;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> GraphHint;
    UPROPERTY(Transient) TObjectPtr<ACasinoLoopGameState> ResultsSource;
    TMap<TWeakObjectPtr<UWidget>, ESlateVisibility> OriginalActionVisibility;
    uint32 ResultSignature = 0;
    float GraphProgress = 0.f;
    bool bHasFinalResults = false;
    bool bHasGraph = false;
    bool bPreview = false;
};
