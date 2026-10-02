#include "casino_simulatorPlayerController.h"
#include "UI/CasinoUIManagerComponent.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "casino_simulatorGameMode.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void Acasino_simulatorPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    UIManager->UpdateUI();
    if (IsLocalController() && !UIManager->IsTravelPending()) TryInitializePlayerHUD();
}
void Acasino_simulatorPlayerController::PreClientTravel(const FString& URL, ETravelType Type, bool bSeamless)
{
    if (IsLocalController())
    {
        UIManager->PrepareTravel();
        UnbindFromAbilitySystem();
        BindToPlayerState(nullptr);
    }
    Super::PreClientTravel(URL, Type, bSeamless);
}
void Acasino_simulatorPlayerController::EnsureUIRoot()
{ UIManager->EnsureUIRoot(); }
void Acasino_simulatorPlayerController::RefreshCasinoUIScreen()
{ UIManager->RefreshCasinoUIScreen(); }
void Acasino_simulatorPlayerController::CloseManagedWidget(UUserWidget* Widget)
{ UIManager->CloseManagedWidget(Widget); }
void Acasino_simulatorPlayerController::ClearInteractionWidgets()
{ UIManager->ClearInteractionWidgets(); }
bool Acasino_simulatorPlayerController::ShowInteractionUI(UUserWidget* Widget)
{ return UIManager->ShowInteractionUI(Widget); }
void Acasino_simulatorPlayerController::CloseInteractionUI(UUserWidget* Widget)
{ UIManager->CloseInteractionUI(Widget); }
bool Acasino_simulatorPlayerController::ShowScreenUI(UUserWidget* Widget, ECasinoUIScreen ExpectedScreen)
{ return UIManager->ShowScreenUI(Widget, ExpectedScreen); }
void Acasino_simulatorPlayerController::SetCasinoUIScreen(ECasinoUIScreen Screen)
{ UIManager->SetCasinoUIScreen(Screen); }
void Acasino_simulatorPlayerController::ApplyUIScreenInput()
{ UIManager->ApplyUIScreenInput(); }
void Acasino_simulatorPlayerController::ClientShowWorldEventAnnouncement_Implementation(const FText& Message, float Duration)
{
    if (IsLocalController() && IsValid(UIManager)) UIManager->ShowWorldEventAnnouncement(Message, Duration);
}
void Acasino_simulatorPlayerController::ReturnToMainMenu()
{
    if (!IsLocalController() || UIManager->IsTravelPending()) return;
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
