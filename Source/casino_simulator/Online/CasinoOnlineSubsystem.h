#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Containers/Ticker.h"
#include "CasinoOnlineSubsystem.generated.h"

class IOnlineSubsystem;
class IVoiceChatUser;
class APlayerState;

UENUM(BlueprintType)
enum class ECasinoOnlineState : uint8
{
    Offline, LoggingIn, Ready, Creating, Searching, Joining, InRoom, Starting, InGame, Leaving
};

USTRUCT(BlueprintType)
struct FCasinoRoomInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 SearchIndex = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) FString RoomName;
    UPROPERTY(BlueprintReadOnly) FString HostName;
    UPROPERTY(BlueprintReadOnly) int32 Players = 0;
    UPROPERTY(BlueprintReadOnly) int32 Capacity = 0;
};

USTRUCT(BlueprintType)
struct FCasinoVoiceDevice
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString Id;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCasinoOnlineChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCasinoOnlineError, FString, Operation, FString, Message);

UCLASS()
class CASINO_SIMULATOR_API UCasinoOnlineSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintReadOnly, Category="Casino|Online") ECasinoOnlineState State = ECasinoOnlineState::Offline;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Online") TArray<FCasinoRoomInfo> Rooms;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Online") FString LastError;
    UPROPERTY(BlueprintAssignable, Category="Casino|Online") FCasinoOnlineChanged OnChanged;
    UPROPERTY(BlueprintAssignable, Category="Casino|Online") FCasinoOnlineError OnError;

    // Browser-based Epic login. No passwords or client secrets in BP pins.
    UFUNCTION(BlueprintCallable, Category="Casino|Online") void Login();
    UFUNCTION(BlueprintCallable, Category="Casino|Online") void CreateRoom(const FString& RoomName);
    UFUNCTION(BlueprintCallable, Category="Casino|Online") void FindRooms();
    UFUNCTION(BlueprintCallable, Category="Casino|Online") void JoinRoom(int32 SearchIndex);
    UFUNCTION(BlueprintCallable, Category="Casino|Online") void LeaveRoom();
    UFUNCTION(BlueprintCallable, Category="Casino|Online") void StartHostedGame();
    UFUNCTION(BlueprintPure, Category="Casino|Online") bool IsRoomHost() const;
    UFUNCTION(BlueprintPure, Category="Casino|Online") bool IsLoggedIn() const;
    UFUNCTION(BlueprintPure, Category="Casino|Online") FString GetLocalDisplayName() const;

    UFUNCTION(BlueprintCallable, Category="Casino|Voice") void SetMicrophoneMuted(bool bMuted);
    UFUNCTION(BlueprintCallable, Category="Casino|Voice") void SetPushToTalkEnabled(bool bEnabled);
    // Bind key Started -> true and Completed / Canceled -> false.
    UFUNCTION(BlueprintCallable, Category="Casino|Voice") void SetPushToTalkHeld(bool bHeld);
    UFUNCTION(BlueprintCallable, Category="Casino|Voice") void SetVoiceOutputVolume(float Volume);
    UFUNCTION(BlueprintCallable, Category="Casino|Voice") void SetVoiceInputDevice(const FString& DeviceId);
    UFUNCTION(BlueprintCallable, Category="Casino|Voice") void SetVoicePlayerMuted(const FString& VoicePlayerId, bool bMuted);
    UFUNCTION(BlueprintPure, Category="Casino|Voice") bool IsMicrophoneMuted() const;
    UFUNCTION(BlueprintPure, Category="Casino|Voice") bool IsPushToTalkEnabled() const;
    UFUNCTION(BlueprintPure, Category="Casino|Voice") float GetVoiceOutputVolume() const;
    UFUNCTION(BlueprintPure, Category="Casino|Voice") bool IsVoiceConnected() const;
    UFUNCTION(BlueprintPure, Category="Casino|Voice") TArray<FCasinoVoiceDevice> GetVoiceInputDevices() const;
    UFUNCTION(BlueprintPure, Category="Casino|Voice") TArray<FString> GetVoicePlayers() const;
    UFUNCTION(BlueprintPure, Category="Casino|Voice") bool IsVoicePlayerTalking(const FString& VoicePlayerId) const;
    UFUNCTION(BlueprintPure, Category="Casino|Voice") FString GetVoiceIdForPlayer(APlayerState* Player) const;

private:
    IOnlineSubsystem* OSS = nullptr; // Engine-owned, scoped to this PIE/game instance.
    IOnlineIdentityPtr Identity;
    IOnlineSessionPtr Sessions;
    TSharedPtr<FOnlineSessionSearch> Search;
    FDelegateHandle LoginHandle, CreateHandle, FindHandle, JoinHandle, DestroyHandle, StartHandle, UpdateHandle;
    FDelegateHandle NetworkHandle, TravelHandle, MapLoadedHandle, DeactivateHandle;
    FTSTicker::FDelegateHandle VoiceTicker;
    FString PendingLobbyPath;
    int32 ExpectedPlayers = 0;
    bool bTalkHeld = false;
    bool bVoiceDirty = true;
    bool bHadVoiceChannel = false;
    bool bSessionWasPresent = false;
    TSet<FString> MutedPlayers;

    bool EnsureInterfaces();
    void SetState(ECasinoOnlineState NewState);
    void Error(const FString& Operation, const FString& Message);
    void ClearOnlineDelegates();
    void LoginComplete(int32 LocalUser, bool bSuccess, const FUniqueNetId& Id, const FString& ErrorText);
    void CreateComplete(FName Name, bool bSuccess);
    void FindComplete(bool bSuccess);
    void JoinComplete(FName Name, EOnJoinSessionCompleteResult::Type Result);
    void DestroyComplete(FName Name, bool bSuccess);
    void UpdateComplete(FName Name, bool bSuccess);
    void StartComplete(FName Name, bool bSuccess);
    void ReturnToMenu();
    void NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Message);
    void TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Message);
    void MapLoaded(UWorld* World);
    void ApplicationDeactivated();
    IVoiceChatUser* VoiceUser() const;
    bool TickVoice(float DeltaSeconds);
    void ApplyVoiceSettings();
};
