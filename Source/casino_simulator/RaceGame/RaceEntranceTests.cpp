#if WITH_DEV_AUTOMATION_TESTS

#include "RaceGame/RaceManager.h"
#include "RaceGame/RaceRunner.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceEntranceFlowTest, "Casino.Race.EntranceFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaceEntranceFlowTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

    for (int32 Frame = 0; Frame < 30; ++Frame) World->Tick(LEVELTICK_All, 0.1f);

    auto* Manager = World->SpawnActor<ARaceManager>();
    auto* Point = World->SpawnActor<ATargetPoint>(FVector(100.f, 200.f, 0.f), FRotator(0.f, 90.f, 0.f));
    if (!TestNotNull(TEXT("Manager"), Manager) || !TestNotNull(TEXT("Spawn point"), Point)) return false;
    Manager->RunnerClass = ARaceRunner::StaticClass();
    Manager->RunnerSpawnPoints.Add(Point);
    Manager->EnterDistance = 400.f;
    Manager->TrackLength = 1000.f;
    Manager->ExitDistance = 200.f;
    Manager->TransitSpeed = 200.f;

    TestTrue(TEXT("Initial phase is idle"), Manager->Phase == ERacePhase::Idle);
    Manager->StartNewRound();
    TestTrue(TEXT("Round starts with entering"), Manager->Phase == ERacePhase::Entering);
    if (!TestEqual(TEXT("One runner per spawn point"), Manager->GetRunners().Num(), 1)) return false;
    ARaceRunner* Runner = Manager->GetRunners()[0];
    const FRunnerRaceScript Original = Runner->RaceScript;
    const FVector Direction = Point->GetActorForwardVector();
    TestTrue(TEXT("Spawn position comes from point"), Original.SpawnLoc.Equals(Point->GetActorLocation()));
    TestTrue(TEXT("Start position uses entry distance"),
        Original.StartLoc.Equals(Original.SpawnLoc + Direction * 400.f));
    TestTrue(TEXT("Finish position uses track length"),
        Original.FinishLoc.Equals(Original.StartLoc + Direction * 1000.f));
    TestTrue(TEXT("Exit position uses exit distance"),
        Original.ExitLoc.Equals(Original.FinishLoc + Direction * 200.f));
    TestEqual(TEXT("Shared entry duration is distance divided by speed"), Original.EnterDuration, 2.f);
    TestTrue(TEXT("Runner begins at spawn position"), Runner->GetActorLocation().Equals(Original.SpawnLoc));
	TestTrue(TEXT("Manager explicitly enables entry"), Runner->bIsEntering);
	TestFalse(TEXT("Entry is not racing"), Runner->bRacing);

    Manager->StartNewRound();
    TestEqual(TEXT("Duplicate start does not create another round"), Manager->CurrentRoundNumber, 1);
    TestTrue(TEXT("Duplicate start preserves runner"), Manager->GetRunners()[0] == Runner);
    Manager->StartRace();
    TestTrue(TEXT("Racing is blocked during entry"), Manager->Phase == ERacePhase::Entering);

    // Advance the shared deadline without ticking unrelated world systems.
    FRunnerRaceScript Halfway = Original;
    Halfway.EnterStartServerTime = World->GetTimeSeconds() - 1.0;
    Manager->EnterStartServerTime = Halfway.EnterStartServerTime;
    Runner->ServerSetupScript(Halfway);
    Runner->Tick(0.f);
    Manager->Tick(0.f);
    TestTrue(TEXT("Entry interpolates along rotated direction"),
        Runner->GetActorLocation().Equals(FMath::Lerp(Original.SpawnLoc, Original.StartLoc, 0.5f)));
    TestTrue(TEXT("Betting stays closed before deadline"), Manager->Phase == ERacePhase::Entering);

    Manager->EnterStartServerTime = World->GetTimeSeconds() - 2.0;
    Manager->Tick(0.f);
    TestTrue(TEXT("Deadline opens betting"), Manager->Phase == ERacePhase::Betting);
    TestTrue(TEXT("Manager places runner at exact start"), Runner->GetActorLocation().Equals(Original.StartLoc));
    TestFalse(TEXT("Runner waits during betting"), Runner->bIsRunning);
	TestFalse(TEXT("Manager explicitly disables entry"), Runner->bIsEntering);
    Runner->Tick(1.f);
    TestTrue(TEXT("Waiting runner does not move"), Runner->GetActorLocation().Equals(Original.StartLoc));

	Manager->Tick(0.f);
	TestTrue(TEXT("Betting remains open before its deadline"), Manager->Phase == ERacePhase::Betting);
	Manager->BettingStartServerTime = World->GetTimeSeconds() - Manager->BettingDuration;
	Manager->Tick(0.f);
    TestTrue(TEXT("Betting permits race start"), Manager->Phase == ERacePhase::Racing);
    TestTrue(TEXT("Race reuses prepared geometry"), Runner->RaceScript.StartLoc.Equals(Original.StartLoc));
    TestEqual(TEXT("Race keeps the prepared speed"), Runner->RaceScript.Speed, Original.Speed);
	TestTrue(TEXT("Manager explicitly enables racing"), Runner->bRacing);
	TestFalse(TEXT("Entry stays disabled while racing"), Runner->bIsEntering);
	FRunnerRaceScript RaceElapsedScript = Runner->RaceScript;
	RaceElapsedScript.RaceStartServerTime = World->GetTimeSeconds() - 0.25;
	Runner->ServerSetupScript(RaceElapsedScript);
	Runner->Tick(0.f);
	const float RacePosition = Runner->GetPosUnits();
	TestTrue(TEXT("Race catches up to server time"), RacePosition > 0.f);
	Runner->Tick(10.f);
	TestEqual(TEXT("Delta time alone cannot advance race playback"), Runner->GetPosUnits(), RacePosition);

	Manager->RaceStartServerTime = World->GetTimeSeconds() - Manager->RaceDuration;
	Manager->Tick(0.f);
	TestTrue(TEXT("Full finish starts exit"), Manager->Phase == ERacePhase::Exiting);
	TestFalse(TEXT("Manager explicitly disables racing"), Runner->bRacing);
	TestTrue(TEXT("Manager explicitly enables exit"), Runner->bIsExiting);
	TestTrue(TEXT("Exit starts at finish position"), Runner->GetActorLocation().Equals(Original.FinishLoc));

	FRunnerRaceScript ExitHalfway = Runner->RaceScript;
	ExitHalfway.ExitStartServerTime = World->GetTimeSeconds() - 0.5;
	Manager->ExitStartServerTime = ExitHalfway.ExitStartServerTime;
	Runner->ServerSetupScript(ExitHalfway);
	Runner->Tick(0.f);
	Manager->Tick(0.f);
	TestTrue(TEXT("Exit interpolates to the exit point"),
		Runner->GetActorLocation().Equals(FMath::Lerp(Original.FinishLoc, Original.ExitLoc, 0.5f)));
	TestTrue(TEXT("Exit stays active before deadline"), Manager->Phase == ERacePhase::Exiting);

	const int32 Winner = Manager->WinnerIndex;
	Manager->ExitStartServerTime = World->GetTimeSeconds() - Manager->ExitDuration;
	Manager->Tick(0.f);
	TestTrue(TEXT("Exit deadline finishes the round"), Manager->Phase == ERacePhase::Finished);
	TestTrue(TEXT("Runner is destroyed on exit"), Runner->IsActorBeingDestroyed());
	TestFalse(TEXT("Exit flag is disabled before destruction"), Runner->bIsExiting);
	TestTrue(TEXT("Runner reaches exact exit point"), Runner->GetActorLocation().Equals(Original.ExitLoc));
	TestEqual(TEXT("Runner list is cleared"), Manager->GetRunners().Num(), 0);
	TestEqual(TEXT("Results survive runner cleanup"), Manager->WinnerIndex, Winner);
	TestEqual(TEXT("Finish order survives runner cleanup"), Manager->FinishOrder.Num(), 1);
	Manager->ResetRace();
	TestTrue(TEXT("Reset prepares idle for the next round"), Manager->Phase == ERacePhase::Idle);

	// RepNotify order must not turn script configuration into an implicit start signal.
	auto* ScriptFirst = World->SpawnActor<ARaceRunner>();
	auto* SignalFirst = World->SpawnActor<ARaceRunner>();
	if (!TestNotNull(TEXT("Script-first runner"), ScriptFirst)
		|| !TestNotNull(TEXT("Signal-first runner"), SignalFirst)) return false;
	FRunnerRaceScript EntryScript = Original;
	EntryScript.EnterStartServerTime = World->GetTimeSeconds();
	ScriptFirst->ServerSetupScript(EntryScript);
	TestFalse(TEXT("Script alone does not enable entry"), ScriptFirst->bIsEntering);
	TestFalse(TEXT("Script alone does not start animation"), ScriptFirst->bIsRunning);
	ScriptFirst->ServerSetEntering(true);
	TestTrue(TEXT("Entry signal starts configured runner"), ScriptFirst->bIsRunning);
	SignalFirst->bIsEntering = true;
	SignalFirst->OnRep_Entering();
	TestFalse(TEXT("Early signal waits for script"), SignalFirst->bIsRunning);
	SignalFirst->RaceScript = EntryScript;
	SignalFirst->OnRep_RaceScript();
	TestTrue(TEXT("Late script activates pending entry signal"), SignalFirst->bIsRunning);
    return true;
}

#endif
