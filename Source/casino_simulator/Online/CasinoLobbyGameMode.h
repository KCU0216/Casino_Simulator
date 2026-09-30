#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CasinoLobbyGameMode.generated.h"

// Lobby map uses this mode; gameplay map keeps the existing BP_FirstPersonGameMode.
UCLASS()
class CASINO_SIMULATOR_API ACasinoLobbyGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACasinoLobbyGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    // Keep the BP (and its cinematic dependencies) unloaded during CDO construction.
    UPROPERTY(EditDefaultsOnly, Category="Casino|Lobby")
    TSoftClassPtr<APlayerController> LobbyPlayerControllerClass;
    // Requires a present listen host and readiness from every other player.
    UFUNCTION(BlueprintPure, Category="Casino|Lobby") bool AreAllPlayersReady() const;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
};
