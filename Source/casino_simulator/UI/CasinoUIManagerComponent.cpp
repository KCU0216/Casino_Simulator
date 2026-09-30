#include "UI/CasinoUIManagerComponent.h"
#include "UI/CasinoManagedWidget.h"
#include "casino_simulatorPlayerController.h"
#include "UI/CasinoUIRoot.h"
#include "UI/casino_simulatorPlayerHUD.h"
#include "UI/InventoryWidget.h"
#include "UI/PauseMenuWidget.h"
#include "Online/CasinoOnlineSettings.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Blueprint/WidgetTree.h"

void UCasinoUIManagerComponent::EnsureUIRoot()
{
    auto* PC = GetCasinoController();
    if (!PC) return;
    if (!PC->IsLocalController() || bUITravelPending) return;
    if (!UIRoot) UIRoot = CreateWidget<UCasinoUIRoot>(PC);
    PC->UIRoot = UIRoot;
    if (UIRoot && !UIRoot->IsInViewport()) UIRoot->AddToPlayerScreen(100);
    if (UIRoot && PC->PlayerHUDWidget) UIRoot->AddHUD(PC->PlayerHUDWidget);
    if (UIRoot && !PC->MobileControlsWidget && PC->ShouldUseTouchControls() && PC->MobileControlsWidgetClass)
    {
        PC->MobileControlsWidget = CreateWidget<UUserWidget>(PC, PC->MobileControlsWidgetClass);
        UIRoot->AddHUD(PC->MobileControlsWidget);
        if (PC->MobileControlsWidget) PC->MobileControlsWidget->SetVisibility(UIScreen == ECasinoUIScreen::Playing
            ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UCasinoUIManagerComponent::RefreshCasinoUIScreen()
{
    auto* PC = GetCasinoController();
    if (!PC) return;
    if (!PC->IsLocalController() || bUITravelPending) return;
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
    if (GS) PC->OnUILoopStatusUpdated(GS->LoopStatus);
}

void UCasinoUIManagerComponent::CloseManagedWidget(UUserWidget* Widget)
{
    auto* PC = GetCasinoController();
    if (!PC) return;
    if (!IsValid(Widget)) return;
    TGuardValue<bool> ClosingGuard(bClosingWidgets, true);
    if (auto* Managed = Cast<UCasinoManagedWidget>(Widget)) Managed->NotifyClosed();
    Widget->SetVisibility(ESlateVisibility::Collapsed);
    if (Widget->WidgetTree)
        Widget->WidgetTree->ForEachWidget([this](UWidget* Child)
        {
            if (auto* ChildUserWidget = Cast<UUserWidget>(Child))
            {
                ChildUserWidget->StopAnimationsAndLatentActions();
                GetWorld()->GetTimerManager().ClearAllTimersForObject(ChildUserWidget);
            }
        });
    Widget->StopAnimationsAndLatentActions();
    GetWorld()->GetTimerManager().ClearAllTimersForObject(Widget);
    PC->OnManagedWidgetClosed(Widget);
    Widget->RemoveFromParent();
}

void UCasinoUIManagerComponent::ClearInteractionWidgets()
{
    auto* PC = GetCasinoController();
    if (!PC) return;
    TGuardValue<bool> ClosingGuard(bClosingWidgets, true);
    const auto Closing = MoveTemp(ManagedInteractions);
    ManagedInteractions.Reset();
    for (UUserWidget* Widget : Closing) CloseManagedWidget(Widget);
    PC->InventoryWidget = nullptr;
    PC->PauseMenuWidget = nullptr;
    PC->SetLocalPawnMeshesHiddenForInteraction(false);
    PC->bInteractionPromptSuppressed = false;
    if (PC->PlayerHUDWidget) PC->PlayerHUDWidget->BP_CloseInterection();
    PC->bInteractionUIOpen = false;
    PC->bWorldInteractionTargetFocused = false;
}

bool UCasinoUIManagerComponent::ShowInteractionUI(UUserWidget* Widget)
{
    auto* PC = GetCasinoController();
    if (!PC) return false;
    if (!PC->IsLocalController() || bUITravelPending || UIScreen != ECasinoUIScreen::Playing || bClosingWidgets ||
        !IsValid(Widget) || Widget->GetOwningPlayer() != PC) return false;
    EnsureUIRoot();
    if (!UIRoot) return false;
    UIRoot->AddInteraction(Widget);
    Widget->SetVisibility(ESlateVisibility::Visible);
    const bool bNewOpen = !ManagedInteractions.Contains(Widget);
    ManagedInteractions.AddUnique(Widget);
    if (bNewOpen)
        if (auto* Managed = Cast<UCasinoManagedWidget>(Widget)) Managed->NotifyOpened(this);
    if (!ManagedInteractions.Contains(Widget) || UIScreen != ECasinoUIScreen::Playing || bUITravelPending) return false;
    ApplyInteractionInput();
    return true;
}

void UCasinoUIManagerComponent::CloseInteractionUI(UUserWidget* Widget)
{
    auto* PC = GetCasinoController();
    if (!PC) return;
    if (!ManagedInteractions.Remove(Widget)) return;
    CloseManagedWidget(Widget);
    if (Widget == PC->InventoryWidget) PC->InventoryWidget = nullptr;
    if (Widget == PC->PauseMenuWidget) PC->PauseMenuWidget = nullptr;
    if (ManagedInteractions.IsEmpty()) PC->ExitInteractionUIMode();
    else ApplyInteractionInput();
    // Closing a UI does not release a server-owned seat: call CloseCurrentInteraction for that.
}

bool UCasinoUIManagerComponent::ShowScreenUI(UUserWidget* Widget, ECasinoUIScreen ExpectedScreen)
{
    auto* PC = GetCasinoController();
    if (!PC) return false;
    if (!PC->IsLocalController() || bUITravelPending || bClosingWidgets || !IsValid(Widget) || Widget->GetOwningPlayer() != PC || UIScreen != ExpectedScreen || UIScreen == ECasinoUIScreen::Playing) return false;
    if (ActiveScreenWidget != Widget) CloseManagedWidget(ActiveScreenWidget);
    ActiveScreenWidget = Widget;
    PC->ActiveScreenWidget = Widget;
    EnsureUIRoot();
    if (!UIRoot) return false;
    UIRoot->AddModal(Widget);
    Widget->SetVisibility(ESlateVisibility::Visible);
    if (auto* Managed = Cast<UCasinoManagedWidget>(Widget)) Managed->NotifyOpened(this);
    ApplyUIScreenInput();
    return true;
}

void UCasinoUIManagerComponent::SetCasinoUIScreen(ECasinoUIScreen Screen)
{
    auto* PC = GetCasinoController();
    if (!PC) return;
    if (!PC->IsLocalController() || bUITravelPending) return;
    EnsureUIRoot();
    if (PC->MobileControlsWidget) PC->MobileControlsWidget->SetVisibility(Screen == ECasinoUIScreen::Playing
        ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!UIRoot) return;
    UIRoot->SetHUDVisible(Screen == ECasinoUIScreen::Playing || Screen == ECasinoUIScreen::Payment || Screen == ECasinoUIScreen::DayPassed);
    if (bUIScreenInitialized && UIScreen == Screen) return;
    bUIScreenInitialized = true;
    UIScreen = Screen;
    PC->UIScreen = Screen;
    UE_LOG(LogTemp, Log, TEXT("CasinoUI: %s -> %s"), *PC->GetName(), *UEnum::GetValueAsString(Screen));
    if (Screen != ECasinoUIScreen::Playing) ClearInteractionWidgets();
    CloseManagedWidget(ActiveScreenWidget);
    ActiveScreenWidget = nullptr;
    PC->ActiveScreenWidget = nullptr;
    if (const auto* Class = PC->ScreenWidgetClasses.Find(Screen); Class && *Class)
        ShowScreenUI(CreateWidget<UUserWidget>(PC, *Class), Screen);
    ApplyUIScreenInput();
    PC->OnUIScreenChanged(Screen, UIObservedGameState ? UIObservedGameState->LoopStatus : FCasinoLoopStatus());
}

void UCasinoUIManagerComponent::ApplyUIScreenInput()
{
    auto* PC = GetCasinoController();
    if (!PC) return;
    const bool bPlaying = UIScreen == ECasinoUIScreen::Playing;
    PC->bDailyPaymentControlLocked = !bPlaying && UIScreen != ECasinoUIScreen::MainMenu && UIScreen != ECasinoUIScreen::Lobby;
    PC->ResetIgnoreMoveInput();
    PC->ResetIgnoreLookInput();
    PC->SetIgnoreMoveInput(!bPlaying);
    PC->SetIgnoreLookInput(!bPlaying);
    PC->bShowMouseCursor = !bPlaying && UIScreen != ECasinoUIScreen::DayIntro && UIScreen != ECasinoUIScreen::Loading;
    if (bPlaying) PC->SetInputMode(FInputModeGameOnly());
    else
    {
        FInputModeUIOnly Mode;
        if (ActiveScreenWidget && ActiveScreenWidget->IsFocusable()) Mode.SetWidgetToFocus(ActiveScreenWidget->TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
    }
}
Acasino_simulatorPlayerController* UCasinoUIManagerComponent::GetCasinoController() const
{
    return Cast<Acasino_simulatorPlayerController>(GetOwner());
}
void UCasinoUIManagerComponent::UpdateUI()
{
    auto* PC = GetCasinoController();
    if (!PC || !PC->IsLocalController()) return;
    if (bUITravelPending)
    {
        if (GetWorld() == UITravelOrigin.Get() || GetWorld()->IsInSeamlessTravel()) return;
        bUITravelPending = false;
    }
    auto* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();
    if (!bUIScreenInitialized || GS != UIObservedGameState || UIObservedWorld.Get() != GetWorld()) RefreshCasinoUIScreen();
    EnsureUIRoot();
}
void UCasinoUIManagerComponent::ApplyInteractionInput()
{
    auto* PC = GetCasinoController();
    if (!PC || UIScreen != ECasinoUIScreen::Playing || ManagedInteractions.IsEmpty()) return;
    PC->SetIsInteractionUIOpen(true);
    PC->bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    auto* Widget = ManagedInteractions.Last().Get();
    if (Widget && Widget->IsFocusable()) Mode.SetWidgetToFocus(Widget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    PC->SetInputMode(Mode);
}
UUserWidget* UCasinoUIManagerComponent::OpenInteractionUI(TSubclassOf<UUserWidget> WidgetClass)
{
    auto* PC = GetCasinoController();
    if (!PC || !PC->IsLocalController() || bUITravelPending || UIScreen != ECasinoUIScreen::Playing || !WidgetClass) return nullptr;
    UUserWidget* Widget = CachedInteractions.FindRef(WidgetClass);
    if (!IsValid(Widget))
    {
        Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
        CachedInteractions.Add(WidgetClass, Widget);
    }
    return ShowInteractionUI(Widget) ? Widget : nullptr;
}
void UCasinoUIManagerComponent::ShutdownUI()
{
    auto* PC = GetCasinoController();
    if (!PC) return;
    if (UIObservedGameState) UIObservedGameState->OnLoopChanged.RemoveDynamic(this, &ThisClass::RefreshCasinoUIScreen);
    UIObservedGameState = nullptr;
    ClearInteractionWidgets();
    CloseManagedWidget(ActiveScreenWidget);
    ActiveScreenWidget = nullptr;
    CloseManagedWidget(PC->PlayerHUDWidget);
    PC->PlayerHUDWidget = nullptr;
    CloseManagedWidget(PC->MobileControlsWidget);
    PC->MobileControlsWidget = nullptr;
    if (UIRoot) UIRoot->RemoveFromParent();
    UIRoot = nullptr;
    CachedInteractions.Reset();
    PC->UIRoot = nullptr;
    PC->ActiveScreenWidget = nullptr;
    bUIScreenInitialized = false;
    UIScreen = ECasinoUIScreen::Loading;
    PC->UIScreen = UIScreen;
}
void UCasinoUIManagerComponent::PrepareTravel()
{
    bUITravelPending = true;
    UITravelOrigin = GetWorld();
    ShutdownUI();
}
