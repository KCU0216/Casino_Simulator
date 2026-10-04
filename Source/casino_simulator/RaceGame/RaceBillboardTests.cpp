#if WITH_DEV_AUTOMATION_TESTS

#include "RaceManager.h"
#include "RaceRunner.h"
#include "RaceBillboardTestWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceBillboardFlowTest, "Casino.Race.BillboardFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaceBillboardFlowTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->TimeSeconds = 10.f;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    auto* Manager = World->SpawnActor<ARaceManager>();
    auto* Widget = CreateWidget<URaceBillboardTestWidget>(World);
    if (!TestNotNull(TEXT("Manager"), Manager) || !TestNotNull(TEXT("Widget"), Widget)) return false;
    TArray<UChildActorComponent*> ManagerChildActors;
    Manager->GetComponents(ManagerChildActors);
    TestEqual(TEXT("Manager no longer creates camera child actors"), ManagerChildActors.Num(), 0);
    Manager->BillboardWidget->SetWidget(Widget);
    Manager->OnRep_Phase();
    TestTrue(TEXT("Widget receives manager before its event"), Widget->RaceManager == Manager);
    TestEqual(TEXT("Initial phase delivered once"), Widget->PhaseCount, 1);
    Manager->RefreshBillboard();
    Manager->OnRep_Phase();
    TestEqual(TEXT("Repeated refresh does not restart BP phase animation"), Widget->PhaseCount, 1);

    const ERacePhase Phases[] = { ERacePhase::Entering, ERacePhase::Betting,
        ERacePhase::Racing, ERacePhase::Exiting, ERacePhase::Finished, ERacePhase::Idle };
    for (ERacePhase Phase : Phases)
    {
        Manager->Phase = Phase;
        Manager->OnRep_Phase(); // Same callback used when clients receive replicated Phase.
        TestTrue(TEXT("Widget receives each current phase"), Widget->CurrentPhase == Phase);
    }
    TestEqual(TEXT("All six transitions delivered"), Widget->PhaseCount, 7);
    const int32 PreviousDataCount = Widget->DataCount;
    Manager->OnRep_BillboardData();
    TestEqual(TEXT("Late replicated data refreshes the widget"), Widget->DataCount, PreviousDataCount + 1);
    TestEqual(TEXT("Data refresh does not replay phase animation"), Widget->PhaseCount, 7);

    auto* Replacement = CreateWidget<URaceBillboardTestWidget>(World);
    Manager->Phase = ERacePhase::Betting;
    Manager->BillboardWidget->SetWidget(Replacement);
    Manager->RefreshBillboard();
    TestEqual(TEXT("Late widget receives current phase"), Replacement->PhaseCount, 1);
    TestTrue(TEXT("Late widget starts in betting"), Replacement->CurrentPhase == ERacePhase::Betting);
    Manager->BettingDuration = 30.f;
    Manager->BettingStartServerTime = World->GetTimeSeconds() - 7.0;
    TestEqual(TEXT("Countdown uses elapsed server time"), Manager->GetRemainingBettingSeconds(), 23.f);
    Manager->BettingStartServerTime = World->GetTimeSeconds() - 40.0;
    TestEqual(TEXT("Countdown clamps at zero"), Manager->GetRemainingBettingSeconds(), 0.f);

    auto* First = World->SpawnActor<ARaceRunner>();
    auto* Second = World->SpawnActor<ARaceRunner>();
    Manager->Runners = { First, Second };
    TArray<UChildActorComponent*> RunnerChildActors;
    First->GetComponents(RunnerChildActors);
    TestEqual(TEXT("Runner no longer creates a camera child actor"), RunnerChildActors.Num(), 0);
    auto SetupRunner = [World](ARaceRunner* Runner, float Speed)
    {
        FRunnerRaceScript Script;
        Script.Speed = Speed;
        Script.TrackLength = 1000.f;
        Script.RaceStartServerTime = World->GetTimeSeconds() - 1.0;
        Runner->ServerSetupScript(Script);
        Runner->ServerSetRacing(true, Script.RaceStartServerTime);
        Runner->Tick(0.f);
    };
    SetupRunner(First, 100.f);
    SetupRunner(Second, 200.f);
    Manager->WinnerIndex = 0;
    Manager->Phase = ERacePhase::Racing;
    TestTrue(TEXT("Leader display uses current progress, not predetermined winner"), Manager->GetLeadingRunner() == Second);
    Manager->Phase = ERacePhase::Exiting;
    TestNull(TEXT("Live leader tracking stops outside racing"), Manager->GetLeadingRunner());
    TestTrue(TEXT("Final winner remains separately available"), Manager->GetWinner() == First);
    return true;
}

#endif