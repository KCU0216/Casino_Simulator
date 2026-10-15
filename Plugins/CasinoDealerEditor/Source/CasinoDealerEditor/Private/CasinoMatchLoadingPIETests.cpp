#if WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Online/CasinoLobbyGameMode.h"
#include "Online/CasinoOnlineSettings.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"
#include "UI/CasinoTravelLoadingSubsystem.h"
#include "UI/CasinoRunResultWidget.h"
#include "UI/CasinoUIManagerComponent.h"
#include "Blueprint/WidgetTree.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorGameMode.h"
#include "casino_simulatorPlayerController.h"

namespace CasinoMatchLoadingPIE
{
struct FSettings
{
    EPlayNetMode Mode = PIE_Standalone;
    bool bOneProcess = true;
    bool bSeparateServer = false;
    int32 Players = 1;
    int32 Seamless = 0;
    bool bRestored = false;
    FString ServerOptions;
    TWeakObjectPtr<AWorldSettings> LobbyWorldSettings;
    TSubclassOf<AGameModeBase> OriginalLobbyMode;
    TArray<FNetDriverDefinition> Drivers;
    FDelegateHandle AdmissionHandle;

    static FString& AdditionalOptions(ULevelEditorPlaySettings* Settings)
    {
        // UE 5.7 exposes only a getter for this reflected editor setting.
        auto* Property = FindFProperty<FStrProperty>(ULevelEditorPlaySettings::StaticClass(), TEXT("AdditionalServerGameOptions"));
        check(Property);
        return *Property->ContainerPtrToValuePtr<FString>(Settings);
    }

    explicit FSettings(int32 PlayerCount)
    {
        auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->GetPlayNetMode(Mode);
        Settings->GetRunUnderOneProcess(bOneProcess);
        Settings->GetPlayNumberOfClients(Players);
        bSeparateServer = Settings->bLaunchSeparateServer;
        Settings->GetAdditionalServerGameOptions(ServerOptions);
        Settings->SetPlayNetMode(PIE_ListenServer);
        Settings->SetRunUnderOneProcess(true);
        Settings->SetPlayNumberOfClients(PlayerCount);
        Settings->bLaunchSeparateServer = false;
        AdditionalOptions(Settings) = TEXT("?game=/Script/casino_simulator.CasinoLobbyGameMode");
        Drivers = GEngine->NetDriverDefinitions;
        auto* Driver = GEngine->NetDriverDefinitions.FindByPredicate([](const auto& Entry) { return Entry.DefName == TEXT("GameNetDriver"); });
        if (!Driver)
        {
            FNetDriverDefinition Entry;
            Entry.DefName = TEXT("GameNetDriver");
            Driver = &GEngine->NetDriverDefinitions.Add_GetRef(Entry);
        }
        Driver->DriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
        Driver->DriverClassNameFallback = Driver->DriverClassName;
        if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel")))
        {
            Seamless = Variable->GetInt();
            Variable->Set(1, ECVF_SetByCode);
        }
        // Admit only this temporary local PIE lobby. No online session is created,
        // advertised, joined, or modified, and no persistent settings are saved.
        AdmissionHandle = FGameModeEvents::OnGameModeInitializedEvent().AddLambda([](AGameModeBase* GameMode)
        {
            if (!GameMode || GameMode->GetWorld()->WorldType != EWorldType::PIE
                || !Cast<ACasinoLobbyGameMode>(GameMode)) return;
            if (auto* Online = GameMode->GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>())
                Online->State = ECasinoOnlineState::InRoom;
        });
    }
    ~FSettings() { Restore(); }
    void Restore()
    {
        if (bRestored) return;
        bRestored = true;
        FGameModeEvents::OnGameModeInitializedEvent().Remove(AdmissionHandle);
        if (LobbyWorldSettings.IsValid()) LobbyWorldSettings->DefaultGameMode = OriginalLobbyMode;
        auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->SetPlayNetMode(Mode);
        Settings->SetRunUnderOneProcess(bOneProcess);
        Settings->SetPlayNumberOfClients(Players);
        Settings->bLaunchSeparateServer = bSeparateServer;
        AdditionalOptions(Settings) = ServerOptions;
        if (GEngine) GEngine->NetDriverDefinitions = Drivers;
        if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel")))
            Variable->Set(Seamless, ECVF_SetByCode);
    }
};

class FSetLobbyMode final : public IAutomationLatentCommand
{
public:
    FSetLobbyMode(TSharedPtr<FSettings> InSettings, FAutomationTestBase* InTest)
        : Settings(MoveTemp(InSettings)), Test(InTest) {}
    bool Update() override
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        const FString Lobby = GetDefault<UCasinoOnlineSettings>()->LobbyMap.ToSoftObjectPath().GetLongPackageName();
        if (!World || World->WorldType != EWorldType::Editor || World->GetOutermost()->GetName() != Lobby)
        {
            Test->AddError(TEXT("The expected editor lobby world was not loaded before PIE."));
            return true;
        }
        AWorldSettings* WorldSettings = World->GetWorldSettings();
        if (!Test->TestNotNull(TEXT("Editor lobby world settings"), WorldSettings)) return true;
        Settings->LobbyWorldSettings = WorldSettings;
        Settings->OriginalLobbyMode = WorldSettings->DefaultGameMode;
        // Listen PIE duplicates these settings instead of honoring the server URL
        // game override. Restore after PIE; do not Modify, dirty, or save the map.
        WorldSettings->DefaultGameMode = ACasinoLobbyGameMode::StaticClass();
        return true;
    }
private:
    TSharedPtr<FSettings> Settings;
    FAutomationTestBase* Test;
};

class FRestore final : public IAutomationLatentCommand
{
public:
    explicit FRestore(TSharedPtr<FSettings> InSettings) : Settings(MoveTemp(InSettings)) {}
    bool Update() override { Settings->Restore(); return true; }
private:
    TSharedPtr<FSettings> Settings;
};

class FTravel final : public IAutomationLatentCommand
{
public:
    FTravel(FAutomationTestBase* InTest, int32 InExpectedPlayers)
        : Test(InTest), ExpectedPlayers(InExpectedPlayers) {}
    bool Update() override
    {
        if (Started == 0.0) Started = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - Started > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("%d-player loading travel timed out at stage %d."), ExpectedPlayers, Stage));
            RestoreLag();
            return true;
        }
        FindWorlds();
        if (!Server.IsValid() || (ExpectedPlayers > 1 && !Client.IsValid())) return false;
        if (Stage == 0)
        {
            if (!Server->GetAuthGameMode<ACasinoLobbyGameMode>() || Server->GetNumPlayerControllers() != ExpectedPlayers) return false;
            auto* Owner = FindLocalController(ExpectedPlayers > 1 ? Client.Get() : Server.Get());
            if (!Owner || !Owner->PlayerState || !Owner->GetPawn()) return false;
#if DO_ENABLE_NET_TEST
            DelayedDriver = ExpectedPlayers > 1 ? Client->GetNetDriver() : nullptr;
            if (DelayedDriver.IsValid())
            {
                OriginalLag = DelayedDriver->PacketSimulationSettings;
                auto Delayed = OriginalLag;
                Delayed.PktLag = 1500;
                Delayed.PktLagVariance = 0;
                DelayedDriver->SetPacketSimulationSettings(Delayed);
            }
#endif
            const auto* Config = GetDefault<UCasinoOnlineSettings>();
            const FString URL = Config->GameMap.ToSoftObjectPath().GetLongPackageName()
                + TEXT("?game=") + Config->GameplayGameMode.ToSoftObjectPath().ToString()
                + FString::Printf(TEXT("?SeamlessTravel?CasinoOnlineMatch=1?ExpectedPlayers=%d"), ExpectedPlayers);
            if (auto* Loading = Server->GetGameInstance()->GetSubsystem<UCasinoTravelLoadingSubsystem>()) Loading->BeginLoading(ExpectedPlayers);
            if (!Test->TestTrue(TEXT("Actual lobby initiates seamless travel"), Server->ServerTravel(URL, true))) return true;
            Stage = 1;
            return false;
        }
        if (UWorld::RemovePIEPrefix(Server->GetOutermost()->GetName())
            != GetDefault<UCasinoOnlineSettings>()->GameMap.ToSoftObjectPath().GetLongPackageName()) return false;
        auto* State = Server->GetGameState<ACasinoLoopGameState>();
        auto* Mode = Server->GetAuthGameMode<Acasino_simulatorGameMode>();
        if (!State || !Mode || !State->MatchLoadingStatus.Generation.IsValid()) return false;
        const auto& Status = State->MatchLoadingStatus;
        if (Status.Stage == ECasinoMatchLoadingStage::Failed)
        {
            Test->AddError(Status.FailureReason.ToString());
            RestoreLag();
            return true;
        }
        if (Status.ExpectedPlayers != ExpectedPlayers)
        {
            Test->AddError(TEXT("Controller replacement changed the expected player roster."));
            RestoreLag();
            return true;
        }
        if (Status.IsPending())
        {
            if (State->LoopStatus.Phase != ECasinoLoopPhase::Waiting)
            {
                Test->AddError(TEXT("Day intro or gameplay began before every client confirmed readiness."));
                RestoreLag();
                return true;
            }
            bObservedWaiting |= Status.Stage == ECasinoMatchLoadingStage::WaitingForPlayers && Status.ReadyPlayers < ExpectedPlayers;
            bObservedSpawnBarrier |= Status.Stage == ECasinoMatchLoadingStage::PreparingSpawns;
            return false;
        }
        if (Status.Stage != ECasinoMatchLoadingStage::Complete) return false;
        if (CompletionSeenAt == 0.0) CompletionSeenAt = FPlatformTime::Seconds();
        auto* Host = FindLocalController(Server.Get());
        if (!Host || !Host->GetPawn()) return false;
        auto* HostLoading = Server->GetGameInstance()->GetSubsystem<UCasinoTravelLoadingSubsystem>();
        if (!HostLoading) return false;
        if (HostLoading->IsLoadingActive())
        {
            if (FPlatformTime::Seconds() - CompletionSeenAt > 3.0)
            {
                Test->AddError(TEXT("Host loading overlay remained active after the server completed every spawn acknowledgement."));
                RestoreLag();
                return true;
            }
            return false;
        }
        if (ExpectedPlayers > 1)
        {
            auto* Remote = FindLocalController(Client.Get());
            auto* ClientState = Client->GetGameState<ACasinoLoopGameState>();
            auto* RemoteLoading = Client->GetGameInstance()->GetSubsystem<UCasinoTravelLoadingSubsystem>();
            if (!Remote || !ClientState || !RemoteLoading
                || ClientState->MatchLoadingStatus.Stage != ECasinoMatchLoadingStage::Complete) return false;
            if (RemoteLoading->IsLoadingActive())
            {
                if (FPlatformTime::Seconds() - CompletionSeenAt > 3.0)
                {
                    Test->AddError(TEXT("Remote loading overlay remained active after the server completed every spawn acknowledgement."));
                    RestoreLag();
                    return true;
                }
                return false;
            }
            Acasino_simulatorPlayerController* ServerRemote = nullptr;
            for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
                if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()); PC && !PC->IsLocalController()) ServerRemote = PC;
            if (!Remote->GetPawn() || !ServerRemote || !ServerRemote->GetPawn()) return false;
            Test->TestTrue(TEXT("Owner sees the same final spawn as the authoritative server"),
                Remote->GetPawn()->GetActorLocation().Equals(ServerRemote->GetPawn()->GetActorLocation(), 5.f));
            Test->TestFalse(TEXT("Host and remote player receive separate starting positions"),
                Host->GetPawn()->GetActorLocation().Equals(ServerRemote->GetPawn()->GetActorLocation(), 20.f));
            Test->TestFalse(TEXT("Remote owner leaves the loading input state"), Remote->IsCasinoMatchLoadingLocally());
        }
        Test->TestTrue(TEXT("Local readiness is confirmed before the game leaves the loading barrier"), bObservedWaiting);
        Test->TestTrue(TEXT("Final spawn acknowledgements form a separate barrier"), bObservedSpawnBarrier);
        Test->TestEqual(TEXT("Every player has acknowledged final placement"), Status.ReadyPlayers, ExpectedPlayers);
        Test->TestFalse(TEXT("Host leaves the loading input state"), Host->IsCasinoMatchLoadingLocally());
        Test->TestTrue(TEXT("Day intro or active play follows loading completion"),
            State->LoopStatus.Phase == ECasinoLoopPhase::DayIntro || State->LoopStatus.Phase == ECasinoLoopPhase::Playing);
        TArray<AActor*> SpawnPoints;
        UGameplayStatics::GetAllActorsWithTag(Server.Get(), Mode->PaymentSpawnTag, SpawnPoints);
        Test->TestTrue(TEXT("Actual game map has one tagged starting point per configured room slot"),
            SpawnPoints.Num() >= GetDefault<UCasinoOnlineSettings>()->MaxPlayers);
        Test->AddInfo(FString::Printf(TEXT("Actual map contains %d tagged spawn points for %d room slots."),
            SpawnPoints.Num(), GetDefault<UCasinoOnlineSettings>()->MaxPlayers));
        RestoreLag();
        return true;
    }
private:
    void FindWorlds()
    {
        Server.Reset();
        Client.Reset();
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
            if (Context.World()->GetNetMode() == NM_ListenServer
                || (ExpectedPlayers == 1 && Context.World()->GetNetMode() == NM_Standalone)) Server = Context.World();
            else if (Context.World()->GetNetMode() == NM_Client) Client = Context.World();
        }
    }
    Acasino_simulatorPlayerController* FindLocalController(UWorld* World) const
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()); PC && PC->IsLocalController()) return PC;
        return nullptr;
    }
    void RestoreLag()
    {
#if DO_ENABLE_NET_TEST
        if (DelayedDriver.IsValid()) DelayedDriver->SetPacketSimulationSettings(OriginalLag);
#endif
    }
    FAutomationTestBase* Test;
    int32 ExpectedPlayers;
    int32 Stage = 0;
    double Started = 0.0;
    double CompletionSeenAt = 0.0;
    bool bObservedWaiting = false;
    bool bObservedSpawnBarrier = false;
    TWeakObjectPtr<UWorld> Server, Client;
#if DO_ENABLE_NET_TEST
    TWeakObjectPtr<UNetDriver> DelayedDriver;
    FPacketSimulationSettings OriginalLag;
#endif
};

class FResultsReplication final : public IAutomationLatentCommand
{
public:
    explicit FResultsReplication(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if (!Started) Started = Now;
        if (Now - Started > 30.0)
        { Test->AddError(TEXT("Two-player result replication/presentation timed out.")); return true; }
        UWorld* Server = nullptr;
        UWorld* Client = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
            if (Context.World()->GetNetMode() == NM_ListenServer) Server = Context.World();
            if (Context.World()->GetNetMode() == NM_Client) Client = Context.World();
        }
        if (!Server || !Client) return false;
        auto* ServerState = Server->GetGameState<ACasinoLoopGameState>();
        auto* ClientState = Client->GetGameState<ACasinoLoopGameState>();
        auto* ClientPC = Cast<Acasino_simulatorPlayerController>(Client->GetFirstPlayerController());
        if (!ServerState || !ClientState || !ClientPC || !ClientPC->GetUIManager()) return false;
        if (Stage == 0)
        {
            Test->TestFalse(TEXT("Private history is not published during gameplay"), ClientState->RunResults.bFinalized);
            FCasinoLoopStatus Status = ServerState->LoopStatus;
            Status.Phase = ECasinoLoopPhase::GameOver;
            Status.CurrentDay = 3;
            ServerState->SetLoopStatus(Status);
            for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
                if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()))
                    PC->ClientFinishDailyPayment(Status.Phase);
            Stage = 1;
            return false;
        }
        auto* Widget = Cast<UCasinoRunResultWidget>(ClientPC->GetUIManager()->ActiveScreenWidget);
        if (!Widget) return false;
        UWidget* Menu = Widget->WidgetTree->FindWidget(TEXT("MainMenuButton"));
        if (!Menu) return false;
        if (Stage == 1)
        {
            // Exercise the real RPC opening Result before its separately replicated data.
            Test->TestEqual(TEXT("Early result screen waits for the authoritative report"),
                Menu->GetVisibility(), ESlateVisibility::Collapsed);
            FCasinoRunResults Results;
            Results.bFinalized = true;
            Results.Days = {1, 2, 3};
            for (int32 Index = 0; Index < 2; ++Index)
            {
                auto& Player = Results.Players.AddDefaulted_GetRef();
                Player.RunPlayerId = Index + 1;
                Player.PlayerName = FString::Printf(TEXT("Result Player %d"), Index + 1);
                Player.Rank = Index + 1;
                Player.FinalBalance = 3000 - Index * 1000;
                Player.TotalContribution = 1200 + Index * 100;
                for (int32 Day : Results.Days)
                {
                    auto& Point = Player.DailyBalances.AddDefaulted_GetRef();
                    Point.Day = Day;
                    Point.Balance = Player.FinalBalance + (3 - Day) * 500;
                }
            }
            ServerState->SetRunResults(Results);
            Stage = 2;
            return false;
        }
        if (!ClientState->RunResults.bFinalized || ClientState->RunResults.Players.Num() != 2) return false;
        if (Stage == 2)
        {
            Test->TestEqual(TEXT("Remote receives the same server ranking"), ClientState->RunResults.Players[0].FinalBalance, int64(3000));
            Test->TestEqual(TEXT("Remote receives actual contribution total"), ClientState->RunResults.Players[1].TotalContribution, int64(1300));
            Test->TestEqual(TEXT("Menu waits during the remote graph animation"), Menu->GetVisibility(), ESlateVisibility::Collapsed);
            Stage = 3;
        }
        if (Widget->GetGraphProgress() < 1.f) return false;
        Test->TestTrue(TEXT("Menu appears after the real remote graph animation"), Menu->GetVisibility() != ESlateVisibility::Collapsed);
        Test->TestTrue(TEXT("Result screen instance survives the late report"),
            Widget == ClientPC->GetUIManager()->ActiveScreenWidget);
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started = 0.0;
    int32 Stage = 0;
};

bool QueueTravel(FAutomationTestBase* Test, int32 PlayerCount, bool bCheckResults = false)
{
    if (!GEditor || !GEngine || GEditor->PlayWorld)
    {
        Test->AddError(TEXT("Run loading travel tests from a stopped editor."));
        return false;
    }
    const FString Lobby = GetDefault<UCasinoOnlineSettings>()->LobbyMap.ToSoftObjectPath().GetLongPackageName();
    if (!Test->TestFalse(TEXT("Lobby map is configured"), Lobby.IsEmpty())) return false;
    // The shipped lobby level script focuses its non-focusable lobby widget.
    // Match only the same known error documented by the existing popup PIE test.
    Test->AddExpectedErrorPlain(TEXT("InputMode:UIOnly - Attempting to focus Non-Focusable widget SObjectWidget [Widget.cpp(990)]!"),
        EAutomationExpectedErrorFlags::Contains, 0);
    auto Settings = MakeShared<FSettings>(PlayerCount);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Lobby));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSetLobbyMode>(Settings, Test));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FTravel>(Test, PlayerCount));
    if (bCheckResults)
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FResultsReplication>(Test));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRestore>(Settings));
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoMatchLoadingNetworkPIETest,
    "Casino.Online.MatchLoading.NetworkPIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoMatchLoadingNetworkPIETest::RunTest(const FString& Parameters)
{
    return CasinoMatchLoadingPIE::QueueTravel(this, 2);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoMatchLoadingSinglePlayerPIETest,
    "Casino.Online.MatchLoading.SinglePlayerPIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoMatchLoadingSinglePlayerPIETest::RunTest(const FString& Parameters)
{
    return CasinoMatchLoadingPIE::QueueTravel(this, 1);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoRunResultsNetworkPIETest,
    "Casino.Results.NetworkPIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoRunResultsNetworkPIETest::RunTest(const FString& Parameters)
{
    return CasinoMatchLoadingPIE::QueueTravel(this, 2, true);
}
#endif
