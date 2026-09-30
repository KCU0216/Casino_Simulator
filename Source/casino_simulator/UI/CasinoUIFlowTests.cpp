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
    auto* PS = World->SpawnActor<Acasino_simulatorPlayerState>();
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

    PS->SetPlayerName(TEXT("TestHost"));
    PS->AddItem(0, 5);
    PS->AddMiningPowerUpgradeLevel(2);
    PS->AddMiningSpeedUpgradeLevel(3);
    PS->bLobbyReady = true;
    PS->bDailyPaymentSubmitted = true;
    PS->DailyPaymentAmount = 100;
    PS->ResetForNewCasinoRun();
    TestEqual(TEXT("Run inventory reset"), PS->GetItemQuantity(0), 0);
    TestEqual(TEXT("Power upgrade reset"), PS->GetMiningPowerUpgradeLevel(), 0);
    TestEqual(TEXT("Speed upgrade reset"), PS->GetMiningSpeedUpgradeLevel(), 0);
    TestFalse(TEXT("Ready reset"), PS->bLobbyReady);
    TestFalse(TEXT("Submission reset"), PS->bDailyPaymentSubmitted);
    TestEqual(TEXT("Contribution reset"), PS->DailyPaymentAmount, 0);
    TestEqual(TEXT("Identity preserved"), PS->GetPlayerName(), FString(TEXT("TestHost")));
    return true;
}
#endif
