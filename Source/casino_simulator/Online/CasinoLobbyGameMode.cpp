#include "Online/CasinoLobbyGameMode.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "Online/CasinoOnlineSettings.h"
#include "casino_simulatorPlayerState.h"
#include "casino_simulatorPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ACasinoLobbyGameMode::ACasinoLobbyGameMode()
{
    bUseSeamlessTravel = true;
    PlayerStateClass = Acasino_simulatorPlayerState::StaticClass();
    DefaultPawnClass = ASpectatorPawn::StaticClass();
    PlayerControllerClass = Acasino_simulatorPlayerController::StaticClass();
    LobbyPlayerControllerClass = TSoftClassPtr<APlayerController>(FSoftObjectPath(
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C")));
}

void ACasinoLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    // InitGame runs after engine initialization and before player controllers are spawned.
    // Preserve a custom controller explicitly assigned by a derived GameMode BP.
    if (PlayerControllerClass == Acasino_simulatorPlayerController::StaticClass())
    {
        if (UClass* ControllerClass = LobbyPlayerControllerClass.LoadSynchronous())
        {
            PlayerControllerClass = ControllerClass;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Lobby: could not load PlayerController %s; using native fallback."),
                *LobbyPlayerControllerClass.ToSoftObjectPath().ToString());
        }
    }
    Super::InitGame(MapName, Options, ErrorMessage);
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
