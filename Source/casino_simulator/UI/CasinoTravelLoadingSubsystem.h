#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CasinoTravelLoadingSubsystem.generated.h"

class SWidget;
class UGameViewportClient;
struct FCasinoTravelLoadingViewState;
struct FWorldContext;

/** Persists across world travel; blocking map loads and network waiting use separate Slate hosts. */
UCLASS()
class CASINO_SIMULATOR_API UCasinoTravelLoadingSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void BeginLoading(int32 ExpectedPlayers);
    void UpdatePlayerReadiness(int32 ReadyPlayers, int32 ExpectedPlayers, bool bAllReady);
    void SetLocalWorldReady(bool bReady);
    void SetPreparingSpawns(bool bPreparing);
    /** Shows completion briefly. Ignored until both the server barrier and this client are ready. */
    void FinishLoading();
    void FailLoading(const FString& Reason);
    void CancelLoading();
    void SetFailureExitHandler(FSimpleDelegate Handler);
    bool IsLoadingActive() const { return bActive; }
    bool HasLoadingFailed() const { return bFailed; }
    /** Game-thread check; never queried from the movie player's Slate loading thread. */
    bool AreLocalLoadingTasksComplete() const;

private:
    friend class FCasinoTravelLoadingIsolationTest;
    void HandlePreLoadMap(const FWorldContext& Context, const FString& MapName);
    void HandlePostLoadMap(UWorld* World);
    void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Reason);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Reason);
    bool TickLoading(float DeltaSeconds);
    bool OwnsWorld(const UWorld* World) const;
    void AttachViewportOverlay();
    void RemoveViewportOverlay();
    void RefreshPresentation();
    void RequestFailureExit();

    TSharedPtr<FCasinoTravelLoadingViewState, ESPMode::ThreadSafe> ViewState;
    TSharedPtr<SWidget> ViewportWidget;
    TSharedPtr<SWidget> MovieWidget;
    TWeakObjectPtr<UGameViewportClient> AttachedViewport;
    FDelegateHandle PreLoadHandle;
    FDelegateHandle PostLoadHandle;
    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
    FTSTicker::FDelegateHandle TickerHandle;
    FSimpleDelegate FailureExitHandler;
    int32 Expected = 0;
    int32 Ready = 0;
    bool bActive = false;
    bool bMapLoaded = false;
    bool bLocalReady = false;
    bool bServerReady = false;
    bool bPreparingSpawns = false;
    bool bFailed = false;
    bool bFailureInputApplied = false;
    bool bMoviePrepared = false;
    double CompletionDeadline = 0.0;
};
