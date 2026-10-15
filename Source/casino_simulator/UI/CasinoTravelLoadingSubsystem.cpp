#include "UI/CasinoTravelLoadingSubsystem.h"

#include "UI/CasinoTravelLoadingView.h"
#include "Online/CasinoOnlineSettings.h"
#include "Async/Async.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LevelStreaming.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MoviePlayer.h"
#include "PipelineStateCache.h"
#include "ShaderPipelineCache.h"
#include "UObject/UObjectGlobals.h"

void UCasinoTravelLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ViewState = MakeShared<FCasinoTravelLoadingViewState, ESPMode::ThreadSafe>();
    PreLoadHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
    PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
    }
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickLoading), .05f);
}

void UCasinoTravelLoadingSubsystem::Deinitialize()
{
    CancelLoading();
    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadHandle);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    FailureExitHandler.Unbind();
    ViewState.Reset();
    Super::Deinitialize();
}

void UCasinoTravelLoadingSubsystem::BeginLoading(int32 ExpectedPlayers)
{
    check(IsInGameThread());
    if (IsRunningDedicatedServer()) return;
    if (bActive && !bFailed)
    {
        Expected = FMath::Max(Expected, ExpectedPlayers);
        RefreshPresentation();
        AttachViewportOverlay();
        return;
    }
    CancelLoading();
    bActive = true;
    bFailed = bMapLoaded = bLocalReady = bServerReady = bPreparingSpawns = false;
    bFailureInputApplied = false;
    CompletionDeadline = 0.0;
    Expected = FMath::Max(0, ExpectedPlayers);
    Ready = 0;
    RefreshPresentation();
    const TWeakObjectPtr<UCasinoTravelLoadingSubsystem> WeakThis(this);
    const FSimpleDelegate SafeExit = FSimpleDelegate::CreateLambda([WeakThis]
    {
        // Slate can be running on the movie-player thread. Dereference UObjects only on the game thread.
        AsyncTask(ENamedThreads::GameThread, [WeakThis]
        {
            if (auto* Subsystem = WeakThis.Get()) Subsystem->RequestFailureExit();
        });
    });
    ViewportWidget = SNew(SCasinoTravelLoadingView).ViewState(ViewState).OnExit(SafeExit);
    MovieWidget = SNew(SCasinoTravelLoadingView).ViewState(ViewState).OnExit(SafeExit);
    AttachViewportOverlay();
}

void UCasinoTravelLoadingSubsystem::UpdatePlayerReadiness(int32 ReadyPlayers, int32 ExpectedPlayers, bool bAllReady)
{
    check(IsInGameThread());
    if (!bActive || bFailed) return;
    Expected = FMath::Max(0, ExpectedPlayers);
    Ready = FMath::Clamp(ReadyPlayers, 0, Expected);
    bServerReady = bAllReady;
    RefreshPresentation();
}

void UCasinoTravelLoadingSubsystem::SetLocalWorldReady(bool bReady)
{
    check(IsInGameThread());
    if (!bActive || bFailed) return;
    bLocalReady = bReady;
    if (bReady) bMapLoaded = true; // Seamless travel does not always broadcast PostLoadMap.
    RefreshPresentation();
}

void UCasinoTravelLoadingSubsystem::SetPreparingSpawns(bool bPreparing)
{
    check(IsInGameThread());
    bPreparingSpawns = bPreparing;
    RefreshPresentation();
}

void UCasinoTravelLoadingSubsystem::FinishLoading()
{
    check(IsInGameThread());
    if (!bActive || bFailed || !bLocalReady || !bServerReady) return;
    RefreshPresentation();
    if (CompletionDeadline <= 0.0) CompletionDeadline = FPlatformTime::Seconds() + .35;
}

void UCasinoTravelLoadingSubsystem::FailLoading(const FString& Reason)
{
    check(IsInGameThread());
    if (!bActive) return;
    bFailed = true;
    bFailureInputApplied = false;
    CompletionDeadline = 0.0;
    FCasinoTravelLoadingSnapshot Snapshot;
    Snapshot.Heading = FText::FromString(TEXT("게임에 입장하지 못했습니다"));
    Snapshot.Detail = FText::FromString(Reason.IsEmpty() ? TEXT("연결이 중단됐습니다. 메인 메뉴에서 다시 시도해 주세요.") : Reason);
    Snapshot.Players = FText::FromString(TEXT("메인 메뉴로 돌아가 다시 참가할 수 있습니다."));
    Snapshot.Progress = 0.f;
    Snapshot.bFailed = true;
    ViewState->Write(Snapshot);
    AttachViewportOverlay();
}

void UCasinoTravelLoadingSubsystem::CancelLoading()
{
    check(IsInGameThread());
    bActive = false;
    CompletionDeadline = 0.0;
    RemoveViewportOverlay();
    if (bMoviePrepared && !GIsEditor && IsMoviePlayerEnabled())
    {
        if (IGameMoviePlayer* Player = GetMoviePlayer(); Player && Player->IsMovieCurrentlyPlaying()) Player->StopMovie();
    }
    bMoviePrepared = false;
    ViewportWidget.Reset();
    MovieWidget.Reset();
}

void UCasinoTravelLoadingSubsystem::SetFailureExitHandler(FSimpleDelegate Handler)
{
    FailureExitHandler = MoveTemp(Handler);
}

bool UCasinoTravelLoadingSubsystem::OwnsWorld(const UWorld* World) const
{
    return World && World->GetGameInstance() == GetGameInstance();
}

void UCasinoTravelLoadingSubsystem::HandlePreLoadMap(const FWorldContext& Context, const FString& MapName)
{
    if (!bActive || bFailed || Context.OwningGameInstance != GetGameInstance()) return;
    bMapLoaded = bLocalReady = false;
    RefreshPresentation();
    // The movie player is process-global and unsupported in PIE. Never let one PIE instance cover another.
    if (GIsEditor || IsRunningDedicatedServer() || !IsMoviePlayerEnabled()) return;
    IGameMoviePlayer* Player = GetMoviePlayer();
    if (!Player || !Player->IsInitialized() || !MovieWidget.IsValid()) return;
    FLoadingScreenAttributes Attributes;
    Attributes.WidgetLoadingScreen = MovieWidget;
    Attributes.bAutoCompleteWhenLoadingCompletes = true;
    Attributes.bWaitForManualStop = false;
    Attributes.bMoviesAreSkippable = false;
    Attributes.bAllowEngineTick = false;
    Attributes.MinimumLoadingScreenDisplayTime = 0.f;
    Player->SetupLoadingScreen(Attributes);
    bMoviePrepared = true;
    // The engine starts/stops it around LoadMap. Network readiness waits only in the normal viewport.
}

void UCasinoTravelLoadingSubsystem::HandlePostLoadMap(UWorld* World)
{
    if (!bActive || !OwnsWorld(World)) return;
    const UCasinoOnlineSettings* Settings = GetDefault<UCasinoOnlineSettings>();
    const FString PackageName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
    if (Settings && (PackageName == Settings->MenuMap.ToSoftObjectPath().GetLongPackageName()
        || PackageName == Settings->LobbyMap.ToSoftObjectPath().GetLongPackageName()))
    {
        CancelLoading();
        return;
    }
    bMapLoaded = true;
    bMoviePrepared = false;
    RemoveViewportOverlay();
    AttachViewportOverlay();
    RefreshPresentation();
}

void UCasinoTravelLoadingSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Reason)
{
    if (!bActive) return;
    if (World)
    {
        if (!OwnsWorld(World)) return;
    }
    else
    {
        const FWorldContext* Context = GetGameInstance() ? GetGameInstance()->GetWorldContext() : nullptr;
        if (!Driver || !Context || !Context->PendingNetGame || Context->PendingNetGame->NetDriver != Driver) return;
    }
    // A departed peer reduces the host's readiness roster; it does not fail every remaining player.
    if (Driver && Driver->GetNetMode() != NM_Client &&
        (Type == ENetworkFailure::ConnectionLost || Type == ENetworkFailure::ConnectionTimeout ||
        Type == ENetworkFailure::NetGuidMismatch || Type == ENetworkFailure::NetChecksumMismatch)) return;
    FailLoading(Reason);
}

void UCasinoTravelLoadingSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Reason)
{
    if (bActive && OwnsWorld(World)) FailLoading(Reason);
}

bool UCasinoTravelLoadingSubsystem::AreLocalLoadingTasksComplete() const
{
    check(IsInGameThread());
    const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World || !World->HasBegunPlay()) return false;
    // Global async-loading checks would incorrectly couple independent worlds in single-process PIE.
    if (!GIsEditor && IsAsyncLoading()) return false;
    for (const ULevelStreaming* Level : World->GetStreamingLevels())
    {
        if (Level && Level->IsStreamingStatePending()) return false;
    }
    if (!GIsEditor && (FShaderPipelineCache::NumPrecompilesRemaining() > 0 || PipelineStateCache::NumActivePrecacheRequests() > 0)) return false;
    return true;
}

bool UCasinoTravelLoadingSubsystem::TickLoading(float DeltaSeconds)
{
    if (!bActive) return true;
    AttachViewportOverlay();
    if (bFailed && !bFailureInputApplied && GetGameInstance())
    {
        if (APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController(); Controller && OwnsWorld(Controller->GetWorld()))
        {
            Controller->bShowMouseCursor = true;
            FInputModeGameAndUI Mode;
            Mode.SetWidgetToFocus(ViewportWidget);
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Controller->SetInputMode(Mode);
            bFailureInputApplied = true;
        }
    }
    if (CompletionDeadline > 0.0 && FPlatformTime::Seconds() >= CompletionDeadline) CancelLoading();
    return true;
}

void UCasinoTravelLoadingSubsystem::AttachViewportOverlay()
{
    if (!bActive || !ViewportWidget.IsValid() || !GetGameInstance()) return;
    UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport || AttachedViewport.Get() == Viewport) return;
    RemoveViewportOverlay();
    Viewport->AddViewportWidgetContent(ViewportWidget.ToSharedRef(), 10000);
    AttachedViewport = Viewport;
}

void UCasinoTravelLoadingSubsystem::RemoveViewportOverlay()
{
    if (UGameViewportClient* Viewport = AttachedViewport.Get(); Viewport && ViewportWidget.IsValid())
        Viewport->RemoveViewportWidgetContent(ViewportWidget.ToSharedRef());
    AttachedViewport.Reset();
}

void UCasinoTravelLoadingSubsystem::RefreshPresentation()
{
    if (!bActive || bFailed || !ViewState.IsValid()) return;
    FCasinoTravelLoadingSnapshot Snapshot = MakeCasinoTravelLoadingSnapshot(bMapLoaded, bLocalReady, bServerReady, Ready, Expected);
    if (bPreparingSpawns && !(bLocalReady && bServerReady))
    {
        Snapshot.Heading = FText::FromString(TEXT("시작 위치 준비 중"));
        Snapshot.Detail = FText::FromString(TEXT("모든 플레이어가 첫 위치에서 함께 시작할 수 있도록 준비하고 있습니다."));
        Snapshot.Progress = .8f + .18f * (Expected > 0 ? static_cast<float>(Ready) / Expected : 0.f);
    }
    ViewState->Write(Snapshot);
}

void UCasinoTravelLoadingSubsystem::RequestFailureExit()
{
    check(IsInGameThread());
    if (!bActive || !bFailed) return;
    // The controller installs the leave-session/main-menu action; this subsystem never changes game rules.
    // Session destruction is asynchronous and may fail. Keep the exit action available for a retry;
    // the actual menu/lobby PostLoadMap callback removes this overlay after a successful return.
    FailureExitHandler.ExecuteIfBound();
}
