#if WITH_DEV_AUTOMATION_TESTS
#include "casino_simulatorGameMode.h"
#include "casino_simulatorPlayerState.h"
#include "Interaction/CasinoDayParticipant.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoUIFlowTest, "Casino.DayLoop.IntroAndRunReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoUIFlowTest::RunTest(const FString& Parameters)
{
    UClass* ModeClass = LoadClass<Acasino_simulatorGameMode>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
    if (!TestNotNull(TEXT("Gameplay mode class"), ModeClass)) return false;
    UClass* StateClass = LoadClass<Acasino_simulatorPlayerState>(nullptr,
        TEXT("/Game/1_BluePrint/BP_casino_simulatorPlayerState.BP_casino_simulatorPlayerState_C"));
    if (!TestNotNull(TEXT("Gameplay player state class"), StateClass)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
    auto* GS = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(GS);
    auto* GM = World->SpawnActor<Acasino_simulatorGameMode>(ModeClass);
    GM->GameState = GS;
    GM->DayIntroDurationSeconds = 3.f;
    GM->DayDurationSeconds = 30.f;
    auto* Point = World->SpawnActor<ATargetPoint>();
    Point->Tags.Add(GM->PaymentSpawnTag);
    auto* PS = World->SpawnActor<Acasino_simulatorPlayerState>(StateClass);
    if (!TestNotNull(TEXT("Previous run player state"), PS)) return false;
    GS->AddPlayerState(PS);
    PS->bDailyPaymentSubmitted = true;
    PS->DailyPaymentAmount = 99;
    GM->StartDayLoop();
    TestEqual(TEXT("Day starts with introduction"), GS->LoopStatus.Phase, ECasinoLoopPhase::DayIntro);
    TestFalse(TEXT("Games blocked during introduction"), IsCasinoGameplayAllowed(GM));
    TestEqual(TEXT("No running day timer during introduction"), GS->GetRemainingDaySeconds(), 0.f);
    TestFalse(TEXT("New day resets submission"), PS->bDailyPaymentSubmitted);
    GM->StartDayLoop();
    TestEqual(TEXT("Repeated start does not bypass introduction"), GS->LoopStatus.Phase, ECasinoLoopPhase::DayIntro);
    auto Tick = [World](float Delta)
    {
        ++GFrameCounter;
        World->TimeSeconds += Delta;
        World->GetTimerManager().Tick(Delta);
    };
    Tick(0.f); // Install pending timers before advancing simulated time.
    Tick(2.f);
    TestEqual(TEXT("Still introducing before deadline"), GS->LoopStatus.Phase, ECasinoLoopPhase::DayIntro);
    Tick(1.1f);
    TestEqual(TEXT("Server deadline starts play"), GS->LoopStatus.Phase, ECasinoLoopPhase::Playing);
    TestTrue(TEXT("Gameplay enabled after introduction"), IsCasinoGameplayAllowed(GM));
    TestTrue(TEXT("Full day duration remains after introduction"), GS->GetRemainingDaySeconds() >= 29.9f);
    TestEqual(TEXT("Intro countdown cleared"), GS->GetRemainingIntroSeconds(), 0.f);
    TestFalse(TEXT("Restart without host rejected"), GM->RestartCasinoRun(nullptr));

    auto CheckStartingInventory = [this](const TCHAR* Context, const Acasino_simulatorPlayerState* State)
    {
        TestEqual(FString::Printf(TEXT("%s cigarette quantity"), Context), State->GetItemQuantity(0), 10);
        TestEqual(FString::Printf(TEXT("%s alcohol quantity"), Context), State->GetItemQuantity(1), 10);
    };
    PS->DispatchBeginPlay();
    CheckStartingInventory(TEXT("Initial Blueprint grant"), PS);
    PS->SetPlayerName(TEXT("TestHost"));
    PS->RemoveItem(0, 3);
    PS->AddItem(1, 5);
    TestEqual(TEXT("Previous run consumed cigarettes"), PS->GetItemQuantity(0), 7);
    TestEqual(TEXT("Previous run acquired alcohol"), PS->GetItemQuantity(1), 15);

    // A late client gets a new PlayerState after the world has begun play. Its
    // Blueprint grant runs before seamless identity copying and the run reset.
    auto* ClientState = World->SpawnActor<Acasino_simulatorPlayerState>(StateClass);
    if (!TestNotNull(TEXT("Late client player state"), ClientState)) return false;
    ClientState->DispatchBeginPlay();
    CheckStartingInventory(TEXT("Late client before reset"), ClientState);
    PS->Reset();
    PS->SeamlessTravelTo(ClientState);
    CheckStartingInventory(TEXT("Late client ignores previous run inventory"), ClientState);
    ClientState->AddMiningPowerUpgradeLevel(2);
    ClientState->AddMiningSpeedUpgradeLevel(3);
    ClientState->bLobbyReady = true;
    ClientState->bDailyPaymentSubmitted = true;
    ClientState->DailyPaymentAmount = 100;
    ClientState->ResetForNewCasinoRun();
    CheckStartingInventory(TEXT("Late client after reset"), ClientState);
    TestEqual(TEXT("Power upgrade reset"), ClientState->GetMiningPowerUpgradeLevel(), 0);
    TestEqual(TEXT("Speed upgrade reset"), ClientState->GetMiningSpeedUpgradeLevel(), 0);
    TestFalse(TEXT("Ready reset"), ClientState->bLobbyReady);
    TestFalse(TEXT("Submission reset"), ClientState->bDailyPaymentSubmitted);
    TestEqual(TEXT("Contribution reset"), ClientState->DailyPaymentAmount, 0);
    TestEqual(TEXT("Identity preserved"), ClientState->GetPlayerName(), FString(TEXT("TestHost")));

    // The host is reset before the new world's BeginPlay, so the same Blueprint
    // grant must still supply exactly ten of each item, without old-run copies.
    auto* HostState = World->SpawnActor<Acasino_simulatorPlayerState>(StateClass);
    if (!TestNotNull(TEXT("Host player state"), HostState)) return false;
    PS->SeamlessTravelTo(HostState);
    HostState->ResetForNewCasinoRun();
    TestEqual(TEXT("Host has no cigarette grant before BeginPlay"), HostState->GetItemQuantity(0), 0);
    TestEqual(TEXT("Host has no alcohol grant before BeginPlay"), HostState->GetItemQuantity(1), 0);
    HostState->DispatchBeginPlay();
    CheckStartingInventory(TEXT("Host after BeginPlay"), HostState);
    return true;
}
#endif
