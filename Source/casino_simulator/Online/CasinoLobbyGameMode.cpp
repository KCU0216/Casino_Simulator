#include "Online/CasinoLobbyGameMode.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "Online/CasinoOnlineSettings.h"
#include "casino_simulatorPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "UObject/ConstructorHelpers.h"

ACasinoLobbyGameMode::ACasinoLobbyGameMode()
{
    bUseSeamlessTravel = true;
    PlayerStateClass = Acasino_simulatorPlayerState::StaticClass();
    DefaultPawnClass = ASpectatorPawn::StaticClass();
    static ConstructorHelpers::FClassFinder<APlayerController> PC(
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
    if (PC.Succeeded()) PlayerControllerClass = PC.Class;
}

bool ACasinoLobbyGameMode::AreAllPlayersReady() const
{
    if (!GameState || GameState->PlayerArray.IsEmpty()) return false;
    // The listen-server host starts the match instead of toggling readiness.
    const APlayerState* HostState = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        if (PC && PC->IsLocalController())
        {
            HostState = PC->GetPlayerState<Acasino_simulatorPlayerState>();
            break;
        }
    }
    if (!HostState) return false;
    bool bHostPresent = false;
    for (APlayerState* PS : GameState->PlayerArray)
    {
        if (PS == HostState)
        {
            bHostPresent = true;
            continue;
        }
        const auto* Player = Cast<Acasino_simulatorPlayerState>(PS);
        if (!Player || !Player->bLobbyReady) return false;
    }
    return bHostPresent;
}

void ACasinoLobbyGameMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty()) return;
    const auto* Online = GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>();
    if (!Online || Online->State != ECasinoOnlineState::InRoom)
        ErrorMessage = TEXT("Room is starting or closing.");
    else if (GetNumPlayers() >= GetDefault<UCasinoOnlineSettings>()->MaxPlayers)
        ErrorMessage = TEXT("Room is full.");
}
