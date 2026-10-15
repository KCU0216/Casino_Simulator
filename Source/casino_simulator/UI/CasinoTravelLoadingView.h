#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

/** Immutable copies cross the game-thread/movie-player-thread boundary; no UObjects enter this data. */
struct FCasinoTravelLoadingSnapshot
{
    FText Heading;
    FText Detail;
    FText Players;
    TOptional<float> Progress;
    bool bFailed = false;
};

struct FCasinoTravelLoadingViewState
{
    FCasinoTravelLoadingSnapshot Read() const
    {
        FScopeLock Lock(&Mutex);
        return Snapshot;
    }
    void Write(const FCasinoTravelLoadingSnapshot& Value)
    {
        FScopeLock Lock(&Mutex);
        Snapshot = Value;
    }
private:
    mutable FCriticalSection Mutex;
    FCasinoTravelLoadingSnapshot Snapshot;
};

CASINO_SIMULATOR_API FCasinoTravelLoadingSnapshot MakeCasinoTravelLoadingSnapshot(
    bool bMapLoaded, bool bLocalReady, bool bServerReady, int32 Ready, int32 Expected);

class SCasinoTravelLoadingView : public SCompoundWidget
{
public:
    using FViewStatePtr = TSharedPtr<FCasinoTravelLoadingViewState, ESPMode::ThreadSafe>;
    SLATE_BEGIN_ARGS(SCasinoTravelLoadingView) {}
        SLATE_ARGUMENT(FViewStatePtr, ViewState)
        SLATE_EVENT(FSimpleDelegate, OnExit)
    SLATE_END_ARGS()
    void Construct(const FArguments& Arguments);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
    virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
private:
    TSharedPtr<FCasinoTravelLoadingViewState, ESPMode::ThreadSafe> ViewState;
    FProgressBarStyle ProgressBarStyle;
    FButtonStyle ExitButtonStyle;
};
