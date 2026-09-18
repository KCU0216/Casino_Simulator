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
    UFUNCTION(BlueprintPure, Category="Casino|Lobby") bool AreAllPlayersReady() const;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
};
