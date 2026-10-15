#include "casino_simulatorPlayerController.h"
#include "UI/CasinoUIManagerComponent.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "casino_simulatorGameMode.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UI/CasinoTravelLoadingSubsystem.h"
#include "Kismet/GameplayStatics.h"

int32 Acasino_simulatorPlayerController::GetCasinoMatchTravelPlayerCount(const FString& URL)
{
    int32 OptionsStart = INDEX_NONE;
    if (!URL.FindChar(TEXT('?'), OptionsStart)) return 0;
    const FString Options = URL.Mid(OptionsStart);
    if (!UGameplayStatics::HasOption(Options, TEXT("CasinoOnlineMatch"))) return 0;
    return FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("ExpectedPlayers"), 1), 1, 16);
}

void Acasino_simulatorPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    UpdateCasinoMatchLoading();
    UIManager->UpdateUI();
    if (IsLocalController() && !UIManager->IsTravelPending()) TryInitializePlayerHUD();
}
void Acasino_simulatorPlayerController::PreClientTravel(const FString& URL, ETravelType Type, bool bSeamless)
{
    if (IsLocalController())
    {
        RemoveLegacyLobbyWidgetsForTravel();
        ClientLoadingGeneration.Invalidate();
        const int32 ExpectedPlayers = GetCasinoMatchTravelPlayerCount(URL);
        bClientMatchLoading = ExpectedPlayers > 0;
        bClientWorldReadySent = false;
        bClientSpawnReadySent = false;
        bPendingCasinoSpawn = false;
        ClientLoadingPawn.Reset();
        ClientFinalizedPawn.Reset();
        LocalLoadingReadySince = 0.0;
        if (bClientMatchLoading)
            if (auto* Loading = GetGameInstance()->GetSubsystem<UCasinoTravelLoadingSubsystem>())
            {
                Loading->BeginLoading(ExpectedPlayers);
                auto* GI = GetGameInstance();
                Loading->SetFailureExitHandler(FSimpleDelegate::CreateWeakLambda(GI, [GI]
                {
                    if (auto* Online = GI->GetSubsystem<UCasinoOnlineSubsystem>()) Online->LeaveRoom();
                }));
            }
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
void Acasino_simulatorPlayerController::ClientShowWorldEventAnnouncement_Implementation()
{
    if (IsLocalController() && IsValid(UIManager)) UIManager->ShowWorldEventAnnouncement();
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
