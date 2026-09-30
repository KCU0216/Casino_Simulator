#include "casino_simulatorPlayerController.h"
#include "UI/CasinoUIRoot.h"
#include "UI/casino_simulatorPlayerHUD.h"
#include "UI/InventoryWidget.h"
#include "UI/PauseMenuWidget.h"
#include "Online/CasinoOnlineSettings.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "casino_simulatorGameMode.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Blueprint/WidgetTree.h"

void Acasino_simulatorPlayerController::EnsureUIRoot()
{
    if (!IsLocalController() || bUITravelPending) return;
    if (!UIRoot) UIRoot = CreateWidget<UCasinoUIRoot>(this);
    if (UIRoot && !UIRoot->IsInViewport()) UIRoot->AddToPlayerScreen(100);
    if (UIRoot && PlayerHUDWidget) UIRoot->AddHUD(PlayerHUDWidget);
    if (UIRoot && !MobileControlsWidget && ShouldUseTouchControls() && MobileControlsWidgetClass)
    {
        MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
        UIRoot->AddHUD(MobileControlsWidget);
        if (MobileControlsWidget) MobileControlsWidget->SetVisibility(UIScreen == ECasinoUIScreen::Playing
            ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void Acasino_simulatorPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!IsLocalController()) return;
    if (bUITravelPending)
    {
        if (GetWorld() == UITravelOrigin.Get() || GetWorld()->IsInSeamlessTravel()) return;
        bUITravelPending = false;
    }
    // Seamless travel retains this controller; the replicated GameState arrives independently.
    auto* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();
    if (!bUIScreenInitialized || GS != UIObservedGameState || UIObservedWorld.Get() != GetWorld())
        RefreshCasinoUIScreen();
    EnsureUIRoot();
    TryInitializePlayerHUD();
}

void Acasino_simulatorPlayerController::RefreshCasinoUIScreen()
{
    if (!IsLocalController() || bUITravelPending) return;
    auto* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();
    if (GS != UIObservedGameState)
    {
        if (UIObservedGameState) UIObservedGameState->OnLoopChanged.RemoveDynamic(this, &ThisClass::RefreshCasinoUIScreen);
        UIObservedGameState = GS;
        if (GS) GS->OnLoopChanged.AddUniqueDynamic(this, &ThisClass::RefreshCasinoUIScreen);
    }
    UIObservedWorld = GetWorld();
    ECasinoUIScreen Screen = ECasinoUIScreen::Loading;
    const auto* Settings = GetDefault<UCasinoOnlineSettings>();
    const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
    if (!Settings->MenuMap.IsNull() && Map == Settings->MenuMap.GetAssetName()) Screen = ECasinoUIScreen::MainMenu;
    else if (!Settings->LobbyMap.IsNull() && Map == Settings->LobbyMap.GetAssetName()) Screen = ECasinoUIScreen::Lobby;
    else if (GS)
    {
        switch (GS->LoopStatus.Phase)
        {
        case ECasinoLoopPhase::DayIntro: Screen = ECasinoUIScreen::DayIntro; break;
        case ECasinoLoopPhase::Playing: Screen = ECasinoUIScreen::Playing; break;
        case ECasinoLoopPhase::Settling: Screen = ECasinoUIScreen::Payment; break;
        case ECasinoLoopPhase::DayPassed: Screen = ECasinoUIScreen::DayPassed; break;
        case ECasinoLoopPhase::GameOver:
        case ECasinoLoopPhase::Cleared: Screen = ECasinoUIScreen::Result; break;
        default: break;
        }
    }
    SetCasinoUIScreen(Screen);
    if (GS) OnUILoopStatusUpdated(GS->LoopStatus);
}

void Acasino_simulatorPlayerController::CloseManagedWidget(UUserWidget* Widget)
{
    if (!IsValid(Widget)) return;
    Widget->SetVisibility(ESlateVisibility::Collapsed);
    if (Widget->WidgetTree)
        Widget->WidgetTree->ForEachWidget([this](UWidget* Child)
        {
            if (auto* ChildUserWidget = Cast<UUserWidget>(Child))
            {
                ChildUserWidget->StopAnimationsAndLatentActions();
                GetWorldTimerManager().ClearAllTimersForObject(ChildUserWidget);
            }
        });
    Widget->StopAnimationsAndLatentActions();
    GetWorldTimerManager().ClearAllTimersForObject(Widget);
    OnManagedWidgetClosed(Widget);
    Widget->RemoveFromParent();
}

void Acasino_simulatorPlayerController::ClearInteractionWidgets()
{
    const auto Closing = MoveTemp(ManagedInteractions);
    ManagedInteractions.Reset();
    for (UUserWidget* Widget : Closing) CloseManagedWidget(Widget);
    InventoryWidget = nullptr;
    PauseMenuWidget = nullptr;
    SetLocalPawnMeshesHiddenForInteraction(false);
    bInteractionPromptSuppressed = false;
    if (PlayerHUDWidget) PlayerHUDWidget->BP_CloseInterection();
    bInteractionUIOpen = false;
    bWorldInteractionTargetFocused = false;
}

bool Acasino_simulatorPlayerController::ShowInteractionUI(UUserWidget* Widget)
{
    if (!IsLocalController() || bUITravelPending || UIScreen != ECasinoUIScreen::Playing ||
        !IsValid(Widget) || Widget->GetOwningPlayer() != this) return false;
    EnsureUIRoot();
    UIRoot->AddInteraction(Widget);
    Widget->SetVisibility(ESlateVisibility::Visible);
    ManagedInteractions.AddUnique(Widget);
    return true;
}

void Acasino_simulatorPlayerController::CloseInteractionUI(UUserWidget* Widget)
{
    if (!ManagedInteractions.Remove(Widget)) return;
    CloseManagedWidget(Widget);
    if (Widget == InventoryWidget) InventoryWidget = nullptr;
    if (Widget == PauseMenuWidget) PauseMenuWidget = nullptr;
    // Closing a UI does not release a server-owned seat: call CloseCurrentInteraction for that.
}

bool Acasino_simulatorPlayerController::ShowScreenUI(UUserWidget* Widget, ECasinoUIScreen ExpectedScreen)
{
    if (!IsLocalController() || bUITravelPending || !IsValid(Widget) || Widget->GetOwningPlayer() != this || UIScreen != ExpectedScreen || UIScreen == ECasinoUIScreen::Playing) return false;
    if (ActiveScreenWidget != Widget) CloseManagedWidget(ActiveScreenWidget);
    ActiveScreenWidget = Widget;
    EnsureUIRoot();
    UIRoot->AddModal(Widget);
    Widget->SetVisibility(ESlateVisibility::Visible);
    ApplyUIScreenInput();
    return true;
}

void Acasino_simulatorPlayerController::SetCasinoUIScreen(ECasinoUIScreen Screen)
{
    if (!IsLocalController() || bUITravelPending) return;
    EnsureUIRoot();
    if (MobileControlsWidget) MobileControlsWidget->SetVisibility(Screen == ECasinoUIScreen::Playing
        ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    UIRoot->SetHUDVisible(Screen == ECasinoUIScreen::Playing || Screen == ECasinoUIScreen::Payment || Screen == ECasinoUIScreen::DayPassed);
    if (bUIScreenInitialized && UIScreen == Screen) return;
    bUIScreenInitialized = true;
    UIScreen = Screen;
    UE_LOG(LogTemp, Log, TEXT("CasinoUI: %s -> %s"), *GetName(), *UEnum::GetValueAsString(Screen));
    if (Screen != ECasinoUIScreen::Playing) ClearInteractionWidgets();
    CloseManagedWidget(ActiveScreenWidget);
    ActiveScreenWidget = nullptr;
    if (const auto* Class = ScreenWidgetClasses.Find(Screen); Class && *Class)
        ShowScreenUI(CreateWidget<UUserWidget>(this, *Class), Screen);
    ApplyUIScreenInput();
    OnUIScreenChanged(Screen, UIObservedGameState ? UIObservedGameState->LoopStatus : FCasinoLoopStatus());
}

void Acasino_simulatorPlayerController::ApplyUIScreenInput()
{
    const bool bPlaying = UIScreen == ECasinoUIScreen::Playing;
    bDailyPaymentControlLocked = !bPlaying && UIScreen != ECasinoUIScreen::MainMenu && UIScreen != ECasinoUIScreen::Lobby;
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    SetIgnoreMoveInput(!bPlaying);
    SetIgnoreLookInput(!bPlaying);
    bShowMouseCursor = !bPlaying && UIScreen != ECasinoUIScreen::DayIntro && UIScreen != ECasinoUIScreen::Loading;
    if (bPlaying) SetInputMode(FInputModeGameOnly());
    else
    {
        FInputModeUIOnly Mode;
        if (ActiveScreenWidget && ActiveScreenWidget->IsFocusable()) Mode.SetWidgetToFocus(ActiveScreenWidget->TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
}

void Acasino_simulatorPlayerController::ReturnToMainMenu()
{
    if (!IsLocalController() || bUITravelPending) return;
    if (auto* Online = GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>()) Online->LeaveRoom();
}

bool Acasino_simulatorPlayerController::CanRestartCasinoRun() const
{
    const auto* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();
    return HasAuthority() && IsLocalController() && GS &&
        (GS->LoopStatus.Phase == ECasinoLoopPhase::GameOver || GS->LoopStatus.Phase == ECasinoLoopPhase::Cleared);
}

void Acasino_simulatorPlayerController::ServerRestartCasinoRun_Implementation()
{
    if (!CanRestartCasinoRun()) return;
    if (auto* GM = GetWorld()->GetAuthGameMode<Acasino_simulatorGameMode>()) GM->RestartCasinoRun(this);
}

void Acasino_simulatorPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
    if (IsLocalController())
    {
        bUITravelPending = true;
        UITravelOrigin = GetWorld();
        if (UIObservedGameState) UIObservedGameState->OnLoopChanged.RemoveDynamic(this, &ThisClass::RefreshCasinoUIScreen);
        UIObservedGameState = nullptr;
        ClearInteractionWidgets();
        CloseManagedWidget(ActiveScreenWidget);
        ActiveScreenWidget = nullptr;
        CloseManagedWidget(PlayerHUDWidget);
        PlayerHUDWidget = nullptr;
        CloseManagedWidget(MobileControlsWidget);
        MobileControlsWidget = nullptr;
        if (UIRoot) UIRoot->RemoveFromParent();
        UIRoot = nullptr;
        UnbindFromAbilitySystem();
        BindToPlayerState(nullptr);
        bUIScreenInitialized = false;
        UIScreen = ECasinoUIScreen::Loading;
    }
    Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}
