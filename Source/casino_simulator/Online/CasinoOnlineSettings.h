#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "casino_simulatorGameMode.h"
#include "CasinoOnlineSettings.generated.h"

// Project Settings > Game > Casino Online. Never store developer credentials here.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Casino Online"))
class CASINO_SIMULATOR_API UCasinoOnlineSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }
    UPROPERTY(Config, EditAnywhere, Category="Maps")
    TSoftObjectPtr<UWorld> MenuMap;
    UPROPERTY(Config, EditAnywhere, Category="Maps")
    TSoftObjectPtr<UWorld> LobbyMap;
    UPROPERTY(Config, EditAnywhere, Category="Maps")
    TSoftObjectPtr<UWorld> GameMap;
    // Explicit override prevents the lobby game mode from carrying into the match.
    UPROPERTY(Config, EditAnywhere, Category="Maps")
    TSoftClassPtr<Acasino_simulatorGameMode> GameplayGameMode = TSoftClassPtr<Acasino_simulatorGameMode>(
        FSoftObjectPath(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C")));
    UPROPERTY(Config, EditAnywhere, Category="Rooms", meta=(ClampMin="2", ClampMax="16"))
    int32 MaxPlayers = 4;
    // Change this when the network protocol / gameplay build is incompatible.
    UPROPERTY(Config, EditAnywhere, Category="Rooms")
    int32 BuildId = 1;
};

// Local user settings, shared by lobby/settings UI and saved outside the source tree.
UCLASS(Config=GameUserSettings)
class CASINO_SIMULATOR_API UCasinoVoicePreferences : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config) bool bMicrophoneMuted = false;
    UPROPERTY(Config) bool bPushToTalk = true;
    UPROPERTY(Config) float OutputVolume = 1.0f;
    UPROPERTY(Config) FString InputDeviceId;
};
