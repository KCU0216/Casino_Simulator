#include "Online/CasinoLobbyGameMode.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "Online/CasinoOnlineSettings.h"
#include "casino_simulatorPlayerState.h"
#include "Engine/GameInstance.h"
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
    for (APlayerState* PS : GameState->PlayerArray)
    {
        const auto* Player = Cast<Acasino_simulatorPlayerState>(PS);
        if (!Player || !Player->bLobbyReady) return false;
    }
    return true;
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
