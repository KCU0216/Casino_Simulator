#if WITH_DEV_AUTOMATION_TESTS
#include "UI/CasinoManagedWidget.h"
#include "UI/CasinoUIManagerComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoUIScreenStatusTest, "Casino.UI.ReplicatedScreenStatus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoUIScreenStatusTest::RunTest(const FString& Parameters)
{
    UClass* WidgetClass = LoadClass<UCasinoManagedWidget>(nullptr,
        TEXT("/Game/Blackjack/WBP/WBP_DayIntro.WBP_DayIntro_C"));
    if (!TestNotNull(TEXT("Managed day-intro widget class"), WidgetClass)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(State);
    auto* Widget = CreateWidget<UCasinoManagedWidget>(World, WidgetClass);
    if (!TestNotNull(TEXT("Day-intro widget"), Widget)) return false;
    auto* Manager = NewObject<UCasinoUIManagerComponent>();
    Manager->ActiveScreenWidget = Widget;
    Widget->NotifyOpened(Manager);

    // The screen RPC arrives first, while GameState still describes yesterday's play.
    Manager->UIScreen = ECasinoUIScreen::DayIntro;
    FCasinoLoopStatus Status;
    Status.Phase = ECasinoLoopPhase::Playing;
    Manager->UpdateActiveScreenLoopStatus(Status);
    TestFalse(TEXT("Previous phase not delivered to new screen"), Widget->bHasLoopStatus);
    Status.Phase = ECasinoLoopPhase::DayIntro;
    Status.CurrentDay = 1;
    Status.RequiredPayment = 3000;
    Manager->UpdateActiveScreenLoopStatus(Status);
    TestTrue(TEXT("Replicated intro status delivered after early open"), Widget->bHasLoopStatus);
    TestEqual(TEXT("First-day target refreshed"), Widget->LoopStatus.RequiredPayment, 3000);

    // A later snapshot must refresh the same widget, without reopening/resetting input.
    Status.CurrentDay = 2;
    Status.RequiredPayment = 5000;
    Manager->UpdateActiveScreenLoopStatus(Status);
    TestEqual(TEXT("Same screen receives later target"), Widget->LoopStatus.RequiredPayment, 5000);
    TestEqual(TEXT("Same screen receives later day"), Widget->LoopStatus.CurrentDay, 2);
    TestTrue(TEXT("Screen instance preserved"), Manager->ActiveScreenWidget == Widget);

    Manager->UIScreen = ECasinoUIScreen::Result;
    Status.Phase = ECasinoLoopPhase::GameOver;
    Status.CollectedPayment = 1000;
    Manager->UpdateActiveScreenLoopStatus(Status);
    TestEqual(TEXT("Failure result refreshed"), Widget->LoopStatus.Phase, ECasinoLoopPhase::GameOver);
    Status.Phase = ECasinoLoopPhase::Cleared;
    Status.CollectedPayment = 6000;
    Manager->UpdateActiveScreenLoopStatus(Status);
    TestEqual(TEXT("Success result refreshed on same Result screen"), Widget->LoopStatus.Phase, ECasinoLoopPhase::Cleared);
    TestEqual(TEXT("Final payment refreshed"), Widget->LoopStatus.CollectedPayment, 6000);

    Manager->bUITravelPending = true;
    Status.CollectedPayment = 7000;
    Manager->UpdateActiveScreenLoopStatus(Status);
    TestEqual(TEXT("Travel suppresses stale updates"), Widget->LoopStatus.CollectedPayment, 6000);
    Manager->bUITravelPending = false;
    Widget->NotifyClosed();
    Manager->UpdateActiveScreenLoopStatus(Status);
    TestFalse(TEXT("Closed screen has no live snapshot"), Widget->bHasLoopStatus);
    TestEqual(TEXT("Closed screen does not consume updates"), Widget->LoopStatus.CollectedPayment, 6000);
    return true;
}
#endif
