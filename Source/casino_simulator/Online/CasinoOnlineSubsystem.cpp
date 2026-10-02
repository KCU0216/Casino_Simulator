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
#include "HAL/IConsoleManager.h"
#include "Engine/PendingNetGame.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Pawn.h"
#include "casino_loop_gamestate.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Input/Events.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace CasinoOnline
{
    const FName RoomKey(TEXT("CASINO_ROOM"));
    const FName ProjectKey(TEXT("CASINO_PROJECT"));
    const FName BuildKey(TEXT("CASINO_BUILD_ID"));
    const FName AdmissionKey(TEXT("CASINO_ADMISSION_OPEN"));
    const FString ProjectValue(TEXT("CasinoSimulatorKCU"));
    bool IsAdmissionOpen(const FOnlineSessionSettings& Settings)
    {
        bool bOpen = false;
        return Settings.Get(AdmissionKey, bOpen) && bOpen;
    }
    FString MapPath(const TSoftObjectPtr<UWorld>& Map)
    {
        const FString Path = Map.ToSoftObjectPath().GetLongPackageName();
        return !Path.IsEmpty() && FPackageName::DoesPackageExist(Path) ? Path : FString();
    }
}

namespace CasinoVoice
{
    float DistanceGain(float Distance, float FullDistance, float MaxDistance)
    {
        if (!FMath::IsFinite(Distance) || Distance < 0.0f) return 0.0f;
        FullDistance = FMath::Max(0.0f, FullDistance);
        MaxDistance = FMath::Max(FullDistance + 1.0f, MaxDistance);
        if (Distance <= FullDistance) return 1.0f;
        return FMath::Clamp((MaxDistance - Distance) / (MaxDistance - FullDistance), 0.0f, 1.0f);
    }

    // Slate receives key releases even when a lobby/menu uses UIOnly input mode.
    // Never consume the event: regular game/UI input still receives it.
    class FTalkInput : public IInputProcessor
    {
    public:
        explicit FTalkInput(UCasinoOnlineSubsystem* InOnline) : Online(InOnline) {}
        void Tick(float, FSlateApplication& App, TSharedRef<ICursor>) override
        {
            if (Online.IsValid() && !IsOurWindow(App)) Online->SetPushToTalkHeld(false);
        }
        bool HandleKeyDownEvent(FSlateApplication& App, const FKeyEvent& Event) override
        {
            if (Online.IsValid() && IsOurWindow(App) && Event.GetKey() == Online->GetPushToTalkKey() && !Event.IsRepeat())
                Online->SetPushToTalkHeld(true);
            return false;
        }
        bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            if (Online.IsValid() && Event.GetKey() == Online->GetPushToTalkKey()) Online->SetPushToTalkHeld(false);
            return false;
        }
    private:
        bool IsOurWindow(FSlateApplication& App) const
        {
            UGameInstance* GI = Online.IsValid() ? Online->GetGameInstance() : nullptr;
            UGameViewportClient* Viewport = GI ? GI->GetGameViewportClient() : nullptr;
            const auto Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            return Widget.IsValid() && App.FindWidgetWindow(Widget.ToSharedRef()) == App.GetActiveTopLevelWindow();
        }
        TWeakObjectPtr<UCasinoOnlineSubsystem> Online;
    };
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
    if (VoiceInputProcessor && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(VoiceInputProcessor);
    VoiceInputProcessor.Reset();
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
    FOnlineSessionSetting BuildSetting;
    BuildSetting.AdvertisementType = EOnlineDataAdvertisementType::ViaOnlineService;
    BuildSetting.Data.SetValue(static_cast<int64>(Config->BuildId));
    Settings.Set(CasinoOnline::BuildKey, BuildSetting);
    Settings.Set(CasinoOnline::AdmissionKey, true, EOnlineDataAdvertisementType::ViaOnlineService);
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
    Search->QuerySettings.Set(CasinoOnline::AdmissionKey, true, EOnlineComparisonOp::Equals);
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
            // EOS deserializes all integer lobby attributes as Int64.
            int64 RoomBuild = 0;
            const FOnlineSessionSetting* BuildSetting = Result.Session.SessionSettings.Settings.Find(CasinoOnline::BuildKey);
            if (!BuildSetting)
            {
                UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] excluded: missing CASINO_BUILD_ID (recreate room with updated build)."), I);
                continue;
            }
            if (BuildSetting->Data.GetType() == EOnlineKeyValuePairDataType::Int64)
            {
                BuildSetting->Data.GetValue(RoomBuild);
            }
            else if (BuildSetting->Data.GetType() == EOnlineKeyValuePairDataType::Int32)
            {
                int32 Value = 0;
                BuildSetting->Data.GetValue(Value);
                RoomBuild = Value;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("CasinoOnline: Result[%d] excluded: CASINO_BUILD_ID has non-integer type."), I);
                continue;
            }
            if (RoomBuild != ExpectedBuild)
            {
                UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] excluded: GameBuild=%lld Expected=%d EngineBuild=%d."),
                    I, RoomBuild, ExpectedBuild, Result.Session.SessionSettings.BuildUniqueId);
                continue;
            }
            if (Result.Session.NumOpenPublicConnections <= 0)
            {
                UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] excluded: no open public slots."), I);
                continue;
            }
            if (!CasinoOnline::IsAdmissionOpen(Result.Session.SessionSettings))
            {
                UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] excluded: room admission closed or missing."), I);
                continue;
            }
            FCasinoRoomInfo Room;
            Room.SearchIndex = I;
            Result.Session.SessionSettings.Get(CasinoOnline::RoomKey, Room.RoomName);
            Room.HostName = Result.Session.OwningUserName;
            Room.Capacity = Result.Session.SessionSettings.NumPublicConnections;
            Room.Players = Room.Capacity - Result.Session.NumOpenPublicConnections;
            Rooms.Add(Room);
            UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Result[%d] accepted. GameBuild=%lld OpenSlots=%d"),
                I, RoomBuild, Result.Session.NumOpenPublicConnections);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Search visible rooms=%d"), Rooms.Num());
    const FString SessionId = MoveTemp(PendingJoinSessionId);
    PendingJoinSessionId.Reset();
    SetState(ECasinoOnlineState::Ready);
    if (!SessionId.IsEmpty())
    {
        for (const auto& Room : Rooms)
        {
            if (Search->SearchResults[Room.SearchIndex].GetSessionIdStr() == SessionId)
            {
                JoinVerifiedRoom(Room.SearchIndex);
                return;
            }
        }
        Error(TEXT("JoinRoom"), TEXT("This room has started, is full, or is no longer available. Refresh the room list."));
        return;
    }
    if (!bSuccess) Error(TEXT("FindRooms"), TEXT("EOS room search failed."));
}

void UCasinoOnlineSubsystem::JoinRoom(int32 SearchIndex)
{
    if (State != ECasinoOnlineState::Ready || !IsLoggedIn()) return;
    if (!Search || !Search->SearchResults.IsValidIndex(SearchIndex))
    { Error(TEXT("JoinRoom"), TEXT("Search results expired. Search again.")); return; }
    if (Sessions->GetNamedSession(NAME_GameSession)) return;
    // Re-query EOS: the user may be clicking a lobby result captured before the host started.
    PendingJoinSessionId = Search->SearchResults[SearchIndex].GetSessionIdStr();
    FindRooms();
}

void UCasinoOnlineSubsystem::JoinVerifiedRoom(int32 SearchIndex)
{
    if (State != ECasinoOnlineState::Ready || Sessions->GetNamedSession(NAME_GameSession)) return;
    if (!Search || !Search->SearchResults.IsValidIndex(SearchIndex) ||
        !CasinoOnline::IsAdmissionOpen(Search->SearchResults[SearchIndex].Session.SessionSettings)) return;
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

bool UCasinoOnlineSubsystem::CanStartHostedGame() const
{
    if (State != ECasinoOnlineState::InRoom || !IsRoomHost()) return false;
    const auto* Lobby = GetWorld()->GetAuthGameMode<ACasinoLobbyGameMode>();
    return Lobby && Lobby->AreAllPlayersReady();
}

void UCasinoOnlineSubsystem::StartHostedGame()
{
    if (State != ECasinoOnlineState::InRoom || !IsRoomHost()) return;
    auto* Lobby = GetWorld()->GetAuthGameMode<ACasinoLobbyGameMode>();
    if (!CanStartHostedGame())
    { Error(TEXT("StartGame"), TEXT("All other lobby players must be ready.")); return; }
    if (CasinoOnline::MapPath(GetDefault<UCasinoOnlineSettings>()->GameMap).IsEmpty())
    { Error(TEXT("StartGame"), TEXT("Set a valid Game Map in Casino Online settings.")); return; }
    UClass* MatchMode = GetDefault<UCasinoOnlineSettings>()->GameplayGameMode.LoadSynchronous();
    if (!MatchMode || MatchMode->HasAnyClassFlags(CLASS_Abstract) ||
        !MatchMode->IsChildOf(Acasino_simulatorGameMode::StaticClass()))
    { Error(TEXT("StartGame"), TEXT("Set a concrete Casino gameplay GameMode in Casino Online settings.")); return; }
#if WITH_EDITOR
    // PIE otherwise falls back to reconnecting, which closed match admission rejects.
    if (GetWorld()->WorldType == EWorldType::PIE)
        if (IConsoleVariable* AllowTravel = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel")))
            AllowTravel->Set(1, ECVF_SetByCode);
#endif
    ExpectedPlayers = Lobby->GetNumPlayers();
    auto* Session = Sessions->GetNamedSession(NAME_GameSession);
    Session->SessionSettings.Set(CasinoOnline::AdmissionKey, false, EOnlineDataAdvertisementType::ViaOnlineService);
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
        + TEXT("?game=") + GetDefault<UCasinoOnlineSettings>()->GameplayGameMode.ToSoftObjectPath().ToString()
        + FString::Printf(TEXT("?SeamlessTravel?CasinoOnlineMatch=1?ExpectedPlayers=%d"), ExpectedPlayers);
    UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Starting match travel: %s"), *URL);
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
    PendingJoinSessionId.Reset();
    MutedPlayers.Reset(); Rooms.Reset(); Search.Reset();
    SetState(IsLoggedIn() ? ECasinoOnlineState::Ready : ECasinoOnlineState::Offline);
    const FString Path = CasinoOnline::MapPath(GetDefault<UCasinoOnlineSettings>()->MenuMap);
    // Failure delegates run before the engine schedules its default-map fallback.
    // Open the menu from the next ticker iteration, after that fallback is scheduled.
    if (!Path.IsEmpty()) bMenuTravelPending = true;
    else Error(TEXT("Menu"), TEXT("Menu Map is not configured."));
}

void UCasinoOnlineSubsystem::NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Message)
{
    // Pending connections can fail without a World. Match their driver to this game instance.
    if (!World)
    {
        const FWorldContext* Context = GetGameInstance()->GetWorldContext();
        if (!Context || !Context->PendingNetGame || Context->PendingNetGame->NetDriver != Driver) return;
    }
    else if (World != GetWorld()) return;
    // A departing/rejected peer must not destroy the listen host's room.
    if (Driver && Driver->GetNetMode() != NM_Client &&
        (Type == ENetworkFailure::ConnectionLost || Type == ENetworkFailure::ConnectionTimeout ||
         Type == ENetworkFailure::NetGuidMismatch || Type == ENetworkFailure::NetChecksumMismatch)) return;
    if ( State == ECasinoOnlineState::Leaving || State == ECasinoOnlineState::Ready || State == ECasinoOnlineState::Offline) return;
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
    if (!VoiceInputProcessor && FSlateApplication::IsInitialized() && !IsRunningDedicatedServer())
    {
        VoiceInputProcessor = MakeShared<CasinoVoice::FTalkInput>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(VoiceInputProcessor);
    }
    if (bMenuTravelPending)
    {
        bMenuTravelPending = false;
        const FString Path = CasinoOnline::MapPath(GetDefault<UCasinoOnlineSettings>()->MenuMap);
        if (!Path.IsEmpty()) UGameplayStatics::OpenLevel(this, FName(*Path), true);
        return true;
    }
    if (State == ECasinoOnlineState::InRoom && GetWorld() &&
        UGameplayStatics::HasOption(GetWorld()->URL.ToString(), TEXT("CasinoOnlineMatch")))
        SetState(ECasinoOnlineState::InGame);
    IVoiceChatUser* Voice = VoiceUser();
    const bool bConnected = Voice && !Voice->GetChannels().IsEmpty();
    if (bConnected != bHadVoiceChannel)
    {
        bVoiceDirty = true; bHadVoiceChannel = bConnected;
        UE_LOG(LogTemp, Log, TEXT("CasinoVoice: Connected=%d Channels=%d"), bConnected, Voice ? Voice->GetChannels().Num() : 0);
        OnChanged.Broadcast();
    }
    if (bVoiceDirty && Voice) ApplyVoiceSettings();
    if (bConnected) UpdateVoicePlayerVolumes();
    TArray<FString> PlayerIds = GetVoicePlayers();
    PlayerIds.Sort();
    if (PlayerIds != LastVoicePlayerIds) { LastVoicePlayerIds = MoveTemp(PlayerIds); OnChanged.Broadcast(); }
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
    Voice->SetAudioInputDeviceMuted(!Pref->bVoiceEnabled || Pref->bMicrophoneMuted);
    Voice->SetAudioOutputDeviceMuted(!Pref->bVoiceEnabled);
    Voice->SetAudioOutputVolume(Pref->OutputVolume);
    Voice->SetInputDeviceId(Pref->InputDeviceId);
    for (const FString& Id : MutedPlayers) Voice->SetPlayerMuted(Id, true);
    if (Pref->bVoiceEnabled && !Pref->bMicrophoneMuted && (!Pref->bPushToTalk || bTalkHeld) &&
        (State == ECasinoOnlineState::InRoom || State == ECasinoOnlineState::Starting || State == ECasinoOnlineState::InGame))
        Voice->TransmitToAllChannels();
    else Voice->TransmitToNoChannels();
    bVoiceDirty = false;
    UpdateVoicePlayerVolumes();
}

void UCasinoOnlineSubsystem::UpdateVoicePlayerVolumes()
{
    IVoiceChatUser* Voice = VoiceUser();
    UWorld* World = GetWorld();
    if (!Voice || !World) return;
    const auto* Pref = GetDefault<UCasinoVoicePreferences>();
    const APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
    const APawn* Listener = PC ? PC->GetPawn() : nullptr;
    const AGameStateBase* GS = World->GetGameState();
    const bool bUseDistance = State == ECasinoOnlineState::InGame ||
        (Cast<ACasinoLoopGameState>(GS) && State != ECasinoOnlineState::Ready && State != ECasinoOnlineState::Offline);
    for (const FString& Id : GetVoicePlayers())
    {
        float Gain = bUseDistance ? 0.0f : 1.0f;
        if (bUseDistance && Listener && GS)
        {
            for (APlayerState* PS : GS->PlayerArray)
            {
                if (PS && GetVoiceIdForPlayer(PS) == Id)
                {
                    if (const APawn* Speaker = PS->GetPawn())
                        Gain = CasinoVoice::DistanceGain(FVector::Dist(Listener->GetActorLocation(), Speaker->GetActorLocation()),
                            Pref->FullVolumeDistance, Pref->MaxVoiceDistance);
                    break;
                }
            }
        }
        Voice->SetPlayerVolume(Id, GetVoicePlayerVolume(Id) * Gain);
        Voice->SetPlayerMuted(Id, !Pref->bVoiceEnabled || MutedPlayers.Contains(Id));
    }
}

void UCasinoOnlineSubsystem::SetVoiceEnabled(bool bEnabled)
{
    auto* Pref = GetMutableDefault<UCasinoVoicePreferences>();
    Pref->bVoiceEnabled = bEnabled; Pref->SaveConfig(); bTalkHeld = false; bVoiceDirty = true; ApplyVoiceSettings();
    OnChanged.Broadcast();
}
bool UCasinoOnlineSubsystem::IsVoiceEnabled() const { return GetDefault<UCasinoVoicePreferences>()->bVoiceEnabled; }
void UCasinoOnlineSubsystem::SetVoicePlayerVolume(const FString& Id, float Volume)
{
    if (Id.IsEmpty() || !FMath::IsFinite(Volume)) return;
    auto* Pref = GetMutableDefault<UCasinoVoicePreferences>();
    Pref->PlayerVolumes.Add(Id, FMath::Clamp(Volume, 0.0f, 2.0f)); Pref->SaveConfig();
    UpdateVoicePlayerVolumes(); OnChanged.Broadcast();
}
float UCasinoOnlineSubsystem::GetVoicePlayerVolume(const FString& Id) const
{
    const float* Volume = GetDefault<UCasinoVoicePreferences>()->PlayerVolumes.Find(Id);
    return Volume && FMath::IsFinite(*Volume) ? FMath::Clamp(*Volume, 0.0f, 2.0f) : 1.0f;
}
void UCasinoOnlineSubsystem::SetPushToTalkKey(FKey Key)
{
    if (!Key.IsValid() || Key.IsGamepadKey() || Key.IsMouseButton() || Key == EKeys::AnyKey) return;
    auto* Pref = GetMutableDefault<UCasinoVoicePreferences>();
    Pref->PushToTalkKey = Key; Pref->SaveConfig(); SetPushToTalkHeld(false);
}
FKey UCasinoOnlineSubsystem::GetPushToTalkKey() const { return GetDefault<UCasinoVoicePreferences>()->PushToTalkKey; }
void UCasinoOnlineSubsystem::SetVoiceDistances(float FullDistance, float MaxDistance)
{
    if (!FMath::IsFinite(FullDistance) || !FMath::IsFinite(MaxDistance)) return;
    auto* Pref = GetMutableDefault<UCasinoVoicePreferences>();
    Pref->FullVolumeDistance = FMath::Max(0.0f, FullDistance);
    Pref->MaxVoiceDistance = FMath::Max(Pref->FullVolumeDistance + 1.0f, MaxDistance);
    Pref->SaveConfig(); UpdateVoicePlayerVolumes();
}
float UCasinoOnlineSubsystem::GetVoiceFullVolumeDistance() const { return GetDefault<UCasinoVoicePreferences>()->FullVolumeDistance; }
float UCasinoOnlineSubsystem::GetVoiceMaxDistance() const { return GetDefault<UCasinoVoicePreferences>()->MaxVoiceDistance; }

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
    if (bTalkHeld == bHeld) return;
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
    if (Id.IsEmpty()) return;
    if (bMuted) MutedPlayers.Add(Id); else MutedPlayers.Remove(Id);
    UpdateVoicePlayerVolumes();
    OnChanged.Broadcast();
}
bool UCasinoOnlineSubsystem::IsVoicePlayerMuted(const FString& Id) const { return MutedPlayers.Contains(Id); }
TArray<FCasinoVoiceParticipant> UCasinoOnlineSubsystem::GetVoiceParticipants() const
{
    TArray<FCasinoVoiceParticipant> Result;
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    const APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
    for (const FString& Id : GetVoicePlayers())
    {
        FCasinoVoiceParticipant Entry;
        Entry.Id = Id; Entry.DisplayName = TEXT("Player");
        Entry.Volume = GetVoicePlayerVolume(Id); Entry.bMuted = IsVoicePlayerMuted(Id);
        Entry.bTalking = IsVoicePlayerTalking(Id);
        if (GS) for (APlayerState* PS : GS->PlayerArray)
        {
            if (PS && GetVoiceIdForPlayer(PS) == Id)
            {
                Entry.DisplayName = PS->GetPlayerName();
                Entry.bIsLocalPlayer = PC && PC->PlayerState == PS;
                break;
            }
        }
        Result.Add(Entry);
    }
    return Result;
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

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoRoomAdmissionTest, "Casino.Online.RoomAdmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoRoomAdmissionTest::RunTest(const FString& Parameters)
{
    FOnlineSessionSettings Settings;
    TestFalse(TEXT("Old rooms without an admission attribute are excluded"), CasinoOnline::IsAdmissionOpen(Settings));
    Settings.Set(CasinoOnline::AdmissionKey, true, EOnlineDataAdvertisementType::ViaOnlineService);
    TestTrue(TEXT("Lobby explicitly allows admission"), CasinoOnline::IsAdmissionOpen(Settings));
    Settings.Set(CasinoOnline::AdmissionKey, false, EOnlineDataAdvertisementType::ViaOnlineService);
    TestFalse(TEXT("Starting/playing rooms are excluded even with open player slots"), CasinoOnline::IsAdmissionOpen(Settings));
    Settings.Set(CasinoOnline::AdmissionKey, FString(TEXT("true")), EOnlineDataAdvertisementType::ViaOnlineService);
    TestFalse(TEXT("Malformed admission attributes fail closed"), CasinoOnline::IsAdmissionOpen(Settings));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoVoiceDistanceTest, "Casino.Voice.DistanceAttenuation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCasinoVoiceDistanceTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Nearby player at full volume"), CasinoVoice::DistanceGain(100.0f, 500.0f, 2000.0f), 1.0f);
    TestEqual(TEXT("Five meter boundary"), CasinoVoice::DistanceGain(500.0f, 500.0f, 2000.0f), 1.0f);
    TestEqual(TEXT("Halfway through fade"), CasinoVoice::DistanceGain(1250.0f, 500.0f, 2000.0f), 0.5f);
    TestEqual(TEXT("Twenty meter cutoff"), CasinoVoice::DistanceGain(2000.0f, 500.0f, 2000.0f), 0.0f);
    TestEqual(TEXT("Beyond cutoff remains silent"), CasinoVoice::DistanceGain(4000.0f, 500.0f, 2000.0f), 0.0f);
    TestEqual(TEXT("Personal volume multiplies distance gain"), 0.4f * CasinoVoice::DistanceGain(1250.0f, 500.0f, 2000.0f), 0.2f);
    TestEqual(TEXT("Invalid distance is silent"), CasinoVoice::DistanceGain(-1.0f, 500.0f, 2000.0f), 0.0f);
    return true;
}
#endif
