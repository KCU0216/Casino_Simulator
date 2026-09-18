#include "Online/CasinoOnlineSubsystem.h"
#include "Online/CasinoOnlineSettings.h"
#include "Online/CasinoLobbyGameMode.h"
#include "casino_simulatorPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "IOnlineSubsystemEOS.h"
#include "VoiceChat.h"

namespace CasinoOnline
{
    const FName RoomKey(TEXT("CASINO_ROOM"));
    const FName ProjectKey(TEXT("CASINO_PROJECT"));
    const FName BuildKey(TEXT("CASINO_BUILD_ID"));
    const FString ProjectValue(TEXT("CasinoSimulatorKCU"));
    FString MapPath(const TSoftObjectPtr<UWorld>& Map)
    {
        const FString Path = Map.ToSoftObjectPath().GetLongPackageName();
        return !Path.IsEmpty() && FPackageName::DoesPackageExist(Path) ? Path : FString();
    }
}

void UCasinoOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::MapLoaded);
    DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &ThisClass::ApplicationDeactivated);
    if (GEngine)
    {
        NetworkHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::NetworkFailure);
        TravelHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::TravelFailure);
    }
    VoiceTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ThisClass::TickVoice), 0.2f);
}

void UCasinoOnlineSubsystem::Deinitialize()
{
    if (IVoiceChatUser* Voice = VoiceUser()) Voice->TransmitToNoChannels();
    FTSTicker::GetCoreTicker().RemoveTicker(VoiceTicker);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
    FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkHandle);
        GEngine->OnTravelFailure().Remove(TravelHandle);
    }
    ClearOnlineDelegates();
    Search.Reset(); Sessions.Reset(); Identity.Reset(); OSS = nullptr;
    Super::Deinitialize();
}

bool UCasinoOnlineSubsystem::EnsureInterfaces()
{
    if (!OSS) OSS = Online::GetSubsystem(GetWorld(), FName(TEXT("EOS")));
    if (!OSS) { Error(TEXT("EOS"), TEXT("EOS is unavailable. Configure the EOS artifact in Project Settings.")); return false; }
    Identity = OSS->GetIdentityInterface();
    Sessions = OSS->GetSessionInterface();
    return Identity.IsValid() && Sessions.IsValid();
}

void UCasinoOnlineSubsystem::SetState(ECasinoOnlineState NewState)
{
    State = NewState;
    bVoiceDirty = true;
    OnChanged.Broadcast();
}

void UCasinoOnlineSubsystem::Error(const FString& Operation, const FString& Message)
{
    LastError = Message;
    OnError.Broadcast(Operation, Message);
}

bool UCasinoOnlineSubsystem::IsLoggedIn() const
{
    return Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}

FString UCasinoOnlineSubsystem::GetLocalDisplayName() const
{
    return IsLoggedIn() ? Identity->GetPlayerNickname(0) : FString();
}

void UCasinoOnlineSubsystem::Login()
{
    if (State != ECasinoOnlineState::Offline && State != ECasinoOnlineState::Ready) return;
    if (!EnsureInterfaces()) return;
    if (IsLoggedIn()) { SetState(ECasinoOnlineState::Ready); return; }
    SetState(ECasinoOnlineState::LoggingIn);
    LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0,
        FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::LoginComplete));
    if (!Identity->Login(0, FOnlineAccountCredentials(TEXT("accountportal"), TEXT(""), TEXT(""))))
    {
        Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
        SetState(ECasinoOnlineState::Offline);
        Error(TEXT("Login"), TEXT("Could not start Epic account login."));
    }
}

void UCasinoOnlineSubsystem::LoginComplete(int32 LocalUser, bool bSuccess, const FUniqueNetId&, const FString& ErrorText)
{
    Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
    bVoiceDirty = true;
    SetState(bSuccess ? ECasinoOnlineState::Ready : ECasinoOnlineState::Offline);
    if (!bSuccess) Error(TEXT("Login"), ErrorText);
}

void UCasinoOnlineSubsystem::CreateRoom(const FString& RoomName)
{
    if (State != ECasinoOnlineState::Ready || !IsLoggedIn()) return;
    const auto* Config = GetDefault<UCasinoOnlineSettings>();
    PendingLobbyPath = CasinoOnline::MapPath(Config->LobbyMap);
    if (PendingLobbyPath.IsEmpty() || CasinoOnline::MapPath(Config->MenuMap).IsEmpty())
    { Error(TEXT("CreateRoom"), TEXT("Set valid Lobby Map and Menu Map in Casino Online settings.")); return; }
    if (Sessions->GetNamedSession(NAME_GameSession))
    { Error(TEXT("CreateRoom"), TEXT("Leave the existing room first.")); return; }
    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch = false;
    Settings.bIsDedicated = false;
    Settings.bShouldAdvertise = true;
    Settings.bUsesPresence = true;
    Settings.bAllowJoinViaPresence = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bUseLobbiesIfAvailable = true;
    Settings.bUseLobbiesVoiceChatIfAvailable = true;
    Settings.NumPublicConnections = FMath::Clamp(Config->MaxPlayers, 2, 16);
    // OSS EOS replaces BuildUniqueId with the engine build ID during creation.
    // Keep our game protocol version in an independently advertised attribute.
    Settings.Set(CasinoOnline::BuildKey, Config->BuildId, EOnlineDataAdvertisementType::ViaOnlineService);
    Settings.Set(SETTING_HOST_MIGRATION, false, EOnlineDataAdvertisementType::DontAdvertise);
    Settings.Set(CasinoOnline::RoomKey, RoomName.TrimStartAndEnd().Left(48), EOnlineDataAdvertisementType::ViaOnlineService);
    Settings.Set(CasinoOnline::ProjectKey, CasinoOnline::ProjectValue, EOnlineDataAdvertisementType::ViaOnlineService);
    Settings.Set(SETTING_MAPNAME, PendingLobbyPath, EOnlineDataAdvertisementType::ViaOnlineService);
    UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Creating room. GameBuild=%d Capacity=%d"),
        Config->BuildId, Settings.NumPublicConnections);
    SetState(ECasinoOnlineState::Creating);
    CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::CreateComplete));
    if (!Sessions->CreateSession(0, NAME_GameSession, Settings)) CreateComplete(NAME_GameSession, false);
}

void UCasinoOnlineSubsystem::CreateComplete(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession) return;
    Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    UE_LOG(LogTemp, Log, TEXT("CasinoOnline: CreateRoom completed. Success=%d"), bSuccess);
    if (!bSuccess) { SetState(ECasinoOnlineState::Ready); Error(TEXT("CreateRoom"), TEXT("EOS room creation failed.")); return; }
    bSessionWasPresent = true;
    SetState(ECasinoOnlineState::InRoom);
    UGameplayStatics::OpenLevel(this, FName(*PendingLobbyPath), true,
        TEXT("listen?game=/Script/casino_simulator.CasinoLobbyGameMode"));
}

void UCasinoOnlineSubsystem::FindRooms()
{
    if (State != ECasinoOnlineState::Ready || !IsLoggedIn())
    {
        UE_LOG(LogTemp, Warning, TEXT("CasinoOnline: FindRooms ignored. State=%d LoggedIn=%d"),
            static_cast<int32>(State), IsLoggedIn());
        return;
    }
    Rooms.Reset();
    Search = MakeShared<FOnlineSessionSearch>();
    Search->bIsLanQuery = false;
    Search->MaxSearchResults = 50;
    Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    Search->QuerySettings.Set(CasinoOnline::ProjectKey, CasinoOnline::ProjectValue, EOnlineComparisonOp::Equals);
    UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Searching rooms. ExpectedGameBuild=%d"),
        GetDefault<UCasinoOnlineSettings>()->BuildId);
    SetState(ECasinoOnlineState::Searching);
    FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::FindComplete));
    if (!Sessions->FindSessions(0, Search.ToSharedRef())) FindComplete(false);
}

void UCasinoOnlineSubsystem::FindComplete(bool bSuccess)
{
    Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
    Rooms.Reset();
    const int32 ExpectedBuild = GetDefault<UCasinoOnlineSettings>()->BuildId;
    UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Search completed. Success=%d RawResults=%d ExpectedGameBuild=%d"),
        bSuccess, Search.IsValid() ? Search->SearchResults.Num() : 0, ExpectedBuild);
    if (bSuccess && Search)
    {
        for (int32 I = 0; I < Search->SearchResults.Num(); ++I)
        {
            const auto& Result = Search->SearchResults[I];
            if (!Result.IsValid())
            {
                UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] excluded: invalid session."), I);
                continue;
            }
            int32 RoomBuild = 0;
            if (!Result.Session.SessionSettings.Get(CasinoOnline::BuildKey, RoomBuild))
            {
                UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] excluded: missing CASINO_BUILD_ID (recreate room with updated build)."), I);
                continue;
            }
            if (RoomBuild != ExpectedBuild)
            {
                UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] excluded: GameBuild=%d Expected=%d EngineBuild=%d."),
                    I, RoomBuild, ExpectedBuild, Result.Session.SessionSettings.BuildUniqueId);
                continue;
            }
            if (Result.Session.NumOpenPublicConnections <= 0)
            {
                UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] excluded: no open public slots."), I);
                continue;
            }
            FCasinoRoomInfo Room;
            Room.SearchIndex = I;
            Result.Session.SessionSettings.Get(CasinoOnline::RoomKey, Room.RoomName);
            Room.HostName = Result.Session.OwningUserName;
            Room.Capacity = Result.Session.SessionSettings.NumPublicConnections;
            Room.Players = Room.Capacity - Result.Session.NumOpenPublicConnections;
            Rooms.Add(Room);
            UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] accepted. GameBuild=%d OpenSlots=%d"),
                I, RoomBuild, Result.Session.NumOpenPublicConnections);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Search visible rooms=%d"), Rooms.Num());
    SetState(ECasinoOnlineState::Ready);
    if (!bSuccess) Error(TEXT("FindRooms"), TEXT("EOS room search failed."));
}

void UCasinoOnlineSubsystem::JoinRoom(int32 SearchIndex)
{
    if (State != ECasinoOnlineState::Ready || !IsLoggedIn()) return;
    if (!Search || !Search->SearchResults.IsValidIndex(SearchIndex))
    { Error(TEXT("JoinRoom"), TEXT("Search results expired. Search again.")); return; }
    if (Sessions->GetNamedSession(NAME_GameSession)) return;
    SetState(ECasinoOnlineState::Joining);
    JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::JoinComplete));
    if (!Sessions->JoinSession(0, NAME_GameSession, Search->SearchResults[SearchIndex]))
        JoinComplete(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
}

void UCasinoOnlineSubsystem::JoinComplete(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
    if (Name != NAME_GameSession) return;
    Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    FString Address;
    APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
    if (Result != EOnJoinSessionCompleteResult::Success || !PC ||
        !Sessions->GetResolvedConnectString(NAME_GameSession, Address))
    {
        Error(TEXT("JoinRoom"), TEXT("Could not join the room or resolve its host address."));
        SetState(ECasinoOnlineState::InRoom);
        LeaveRoom();
        return;
    }
    bSessionWasPresent = true;
    SetState(ECasinoOnlineState::InRoom);
    PC->ClientTravel(Address, TRAVEL_Absolute);
}

bool UCasinoOnlineSubsystem::IsRoomHost() const
{
    const auto* Session = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
    return Session && Session->bHosting && GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

void UCasinoOnlineSubsystem::StartHostedGame()
{
    if (State != ECasinoOnlineState::InRoom || !IsRoomHost()) return;
    auto* Lobby = GetWorld()->GetAuthGameMode<ACasinoLobbyGameMode>();
    if (!Lobby || !Lobby->AreAllPlayersReady())
    { Error(TEXT("StartGame"), TEXT("All lobby players must be ready.")); return; }
    if (CasinoOnline::MapPath(GetDefault<UCasinoOnlineSettings>()->GameMap).IsEmpty())
    { Error(TEXT("StartGame"), TEXT("Set a valid Game Map in Casino Online settings.")); return; }
    ExpectedPlayers = Lobby->GetNumPlayers();
    auto* Session = Sessions->GetNamedSession(NAME_GameSession);
    Session->SessionSettings.bAllowJoinInProgress = false;
    Session->SessionSettings.bAllowJoinViaPresence = false;
    Session->SessionSettings.bShouldAdvertise = false;
    SetState(ECasinoOnlineState::Starting);
    UpdateHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(
        FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::UpdateComplete));
    if (!Sessions->UpdateSession(NAME_GameSession, Session->SessionSettings, true)) UpdateComplete(NAME_GameSession, false);
}

void UCasinoOnlineSubsystem::UpdateComplete(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession) return;
    Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
    if (!bSuccess) { SetState(ECasinoOnlineState::InRoom); Error(TEXT("StartGame"), TEXT("Could not close room admission. Retry or recreate the room.")); return; }
    StartHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(
        FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::StartComplete));
    if (!Sessions->StartSession(NAME_GameSession)) StartComplete(NAME_GameSession, false);
}

void UCasinoOnlineSubsystem::StartComplete(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession) return;
    Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
    if (!bSuccess) { SetState(ECasinoOnlineState::InRoom); Error(TEXT("StartGame"), TEXT("Session start failed. Retry or recreate the room.")); return; }
    const FString URL = CasinoOnline::MapPath(GetDefault<UCasinoOnlineSettings>()->GameMap)
        + FString::Printf(TEXT("?CasinoOnlineMatch=1?ExpectedPlayers=%d"), ExpectedPlayers);
    SetState(ECasinoOnlineState::InGame);
    if (!GetWorld()->ServerTravel(URL, true))
    { Error(TEXT("Travel"), TEXT("Server travel failed.")); LeaveRoom(); }
}

void UCasinoOnlineSubsystem::LeaveRoom()
{
    if (State == ECasinoOnlineState::Leaving) return;
    // Wait for pending create/join/search/login to finish; UI disables Leave while busy.
    if (State == ECasinoOnlineState::LoggingIn || State == ECasinoOnlineState::Creating ||
        State == ECasinoOnlineState::Joining || State == ECasinoOnlineState::Searching || State == ECasinoOnlineState::Starting) return;
    bTalkHeld = false;
    if (IVoiceChatUser* Voice = VoiceUser()) Voice->TransmitToNoChannels();
    SetState(ECasinoOnlineState::Leaving);
    if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession)) { ReturnToMenu(); return; }
    DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
        FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::DestroyComplete));
    if (!Sessions->DestroySession(NAME_GameSession)) DestroyComplete(NAME_GameSession, false);
}

void UCasinoOnlineSubsystem::DestroyComplete(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession) return;
    Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
    if (!bSuccess)
    {
        // Do not pretend the old EOS room was removed; allow a retry.
        SetState(ECasinoOnlineState::InRoom);
        Error(TEXT("LeaveRoom"), TEXT("EOS room cleanup failed. Retry Leave Room."));
        return;
    }
    ReturnToMenu();
}

void UCasinoOnlineSubsystem::ReturnToMenu()
{
    bSessionWasPresent = false; bHadVoiceChannel = false; bVoiceDirty = true;
    MutedPlayers.Reset(); Rooms.Reset(); Search.Reset();
    SetState(IsLoggedIn() ? ECasinoOnlineState::Ready : ECasinoOnlineState::Offline);
    const FString Path = CasinoOnline::MapPath(GetDefault<UCasinoOnlineSettings>()->MenuMap);
    if (!Path.IsEmpty()) UGameplayStatics::OpenLevel(this, FName(*Path), true);
    else Error(TEXT("Menu"), TEXT("Menu Map is not configured."));
}

void UCasinoOnlineSubsystem::NetworkFailure(UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString& Message)
{
    if (World != GetWorld() || State == ECasinoOnlineState::Leaving || State == ECasinoOnlineState::Ready || State == ECasinoOnlineState::Offline) return;
    Error(TEXT("Network"), Message);
    ClearOnlineDelegates();
    SetState(ECasinoOnlineState::InRoom);
    LeaveRoom();
}

void UCasinoOnlineSubsystem::TravelFailure(UWorld* World, ETravelFailure::Type, const FString& Message)
{
    if (World != GetWorld() || State == ECasinoOnlineState::Leaving || State == ECasinoOnlineState::Ready || State == ECasinoOnlineState::Offline) return;
    Error(TEXT("Travel"), Message);
    ClearOnlineDelegates();
    SetState(ECasinoOnlineState::InRoom);
    LeaveRoom();
}

void UCasinoOnlineSubsystem::ClearOnlineDelegates()
{
    if (Identity) Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
    if (!Sessions) return;
    Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
    Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
    Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
    Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
}

void UCasinoOnlineSubsystem::ApplicationDeactivated()
{
    // A key-up event can be lost during Alt-Tab. Never leave push-to-talk latched on.
    bTalkHeld = false;
    bVoiceDirty = true;
    ApplyVoiceSettings();
}

void UCasinoOnlineSubsystem::MapLoaded(UWorld* World)
{
    if (!World || World->GetGameInstance() != GetGameInstance()) return;
    bTalkHeld = false;
    bVoiceDirty = true;
    ApplyVoiceSettings();
}

IVoiceChatUser* UCasinoOnlineSubsystem::VoiceUser() const
{
    if (!OSS || OSS->GetSubsystemName() != FName(TEXT("EOS")) || !IsLoggedIn()) return nullptr;
    const auto Id = Identity->GetUniquePlayerId(0);
    return Id.IsValid() ? static_cast<IOnlineSubsystemEOS*>(OSS)->GetVoiceChatUserInterface(*Id) : nullptr;
}

bool UCasinoOnlineSubsystem::TickVoice(float)
{
    if (State == ECasinoOnlineState::InRoom && GetWorld() &&
        UGameplayStatics::HasOption(GetWorld()->URL.ToString(), TEXT("CasinoOnlineMatch")))
        SetState(ECasinoOnlineState::InGame);
    IVoiceChatUser* Voice = VoiceUser();
    const bool bConnected = Voice && !Voice->GetChannels().IsEmpty();
    if (bConnected != bHadVoiceChannel) { bVoiceDirty = true; bHadVoiceChannel = bConnected; OnChanged.Broadcast(); }
    if (bVoiceDirty && Voice) ApplyVoiceSettings();
    // EOS lobby is not migrated if the listen host leaves.
    if (bSessionWasPresent && Sessions && !Sessions->GetNamedSession(NAME_GameSession) &&
        (State == ECasinoOnlineState::InRoom || State == ECasinoOnlineState::InGame))
    { Error(TEXT("Room"), TEXT("The host closed the room.")); LeaveRoom(); }
    return true;
}

void UCasinoOnlineSubsystem::ApplyVoiceSettings()
{
    IVoiceChatUser* Voice = VoiceUser();
    if (!Voice) return;
    const auto* Pref = GetDefault<UCasinoVoicePreferences>();
    Voice->SetAudioInputDeviceMuted(Pref->bMicrophoneMuted);
    Voice->SetAudioOutputVolume(Pref->OutputVolume);
    Voice->SetInputDeviceId(Pref->InputDeviceId);
    for (const FString& Id : MutedPlayers) Voice->SetPlayerMuted(Id, true);
    if (!Pref->bMicrophoneMuted && (!Pref->bPushToTalk || bTalkHeld) &&
        (State == ECasinoOnlineState::InRoom || State == ECasinoOnlineState::Starting || State == ECasinoOnlineState::InGame))
        Voice->TransmitToAllChannels();
    else Voice->TransmitToNoChannels();
    bVoiceDirty = false;
}

void UCasinoOnlineSubsystem::SetMicrophoneMuted(bool bMuted)
{
    auto* Pref = GetMutableDefault<UCasinoVoicePreferences>();
    Pref->bMicrophoneMuted = bMuted; Pref->SaveConfig(); bVoiceDirty = true; ApplyVoiceSettings();
}
void UCasinoOnlineSubsystem::SetPushToTalkEnabled(bool bEnabled)
{
    auto* Pref = GetMutableDefault<UCasinoVoicePreferences>();
    Pref->bPushToTalk = bEnabled; Pref->SaveConfig(); bTalkHeld = false; bVoiceDirty = true; ApplyVoiceSettings();
}
void UCasinoOnlineSubsystem::SetPushToTalkHeld(bool bHeld)
{
    bTalkHeld = bHeld; bVoiceDirty = true; ApplyVoiceSettings();
}
void UCasinoOnlineSubsystem::SetVoiceOutputVolume(float Volume)
{
    if (!FMath::IsFinite(Volume)) return;
    auto* Pref = GetMutableDefault<UCasinoVoicePreferences>();
    Pref->OutputVolume = FMath::Clamp(Volume, 0.0f, 1.0f); Pref->SaveConfig(); bVoiceDirty = true; ApplyVoiceSettings();
}
void UCasinoOnlineSubsystem::SetVoiceInputDevice(const FString& DeviceId)
{
    auto* Pref = GetMutableDefault<UCasinoVoicePreferences>();
    Pref->InputDeviceId = DeviceId; Pref->SaveConfig(); bVoiceDirty = true; ApplyVoiceSettings();
}
void UCasinoOnlineSubsystem::SetVoicePlayerMuted(const FString& Id, bool bMuted)
{
    if (bMuted) MutedPlayers.Add(Id); else MutedPlayers.Remove(Id);
    if (IVoiceChatUser* Voice = VoiceUser()) Voice->SetPlayerMuted(Id, bMuted);
}
bool UCasinoOnlineSubsystem::IsMicrophoneMuted() const { return GetDefault<UCasinoVoicePreferences>()->bMicrophoneMuted; }
bool UCasinoOnlineSubsystem::IsPushToTalkEnabled() const { return GetDefault<UCasinoVoicePreferences>()->bPushToTalk; }
float UCasinoOnlineSubsystem::GetVoiceOutputVolume() const { return GetDefault<UCasinoVoicePreferences>()->OutputVolume; }
bool UCasinoOnlineSubsystem::IsVoiceConnected() const { auto* V = VoiceUser(); return V && !V->GetChannels().IsEmpty(); }
TArray<FCasinoVoiceDevice> UCasinoOnlineSubsystem::GetVoiceInputDevices() const
{
    TArray<FCasinoVoiceDevice> Result;
    if (auto* V = VoiceUser()) for (const auto& Device : V->GetAvailableInputDeviceInfos())
    { FCasinoVoiceDevice D; D.Name = Device.DisplayName; D.Id = Device.Id; Result.Add(D); }
    return Result;
}
TArray<FString> UCasinoOnlineSubsystem::GetVoicePlayers() const
{
    TArray<FString> Result;
    if (auto* V = VoiceUser()) for (const FString& Channel : V->GetChannels())
        for (const FString& Player : V->GetPlayersInChannel(Channel)) Result.AddUnique(Player);
    return Result;
}
bool UCasinoOnlineSubsystem::IsVoicePlayerTalking(const FString& Id) const
{ auto* V = VoiceUser(); return V && V->IsPlayerTalking(Id); }
FString UCasinoOnlineSubsystem::GetVoiceIdForPlayer(APlayerState* Player) const
{
    if (!Player || !Player->GetUniqueId().IsValid() || Player->GetUniqueId()->GetType() != FName(TEXT("EOS"))) return FString();
    // UE 5.7 FUniqueNetIdEOS serializes EpicAccountId|ProductUserId.
    FString Account, Product;
    Player->GetUniqueId()->ToString().Split(TEXT("|"), &Account, &Product);
    return Product;
}
