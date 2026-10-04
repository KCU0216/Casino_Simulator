#if WITH_DEV_AUTOMATION_TESTS

#include "casino_simulatorGameMode.h"
#include "RaceGame/RaceManager.h"
#include "RaceGame/RaceRunner.h"
#include "RaceGame/RaceAnnouncementTestHUD.h"
#include "casino_simulatorPlayerController.h"
#include "UI/CasinoUIManagerComponent.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceWorldEventTest, "Casino.Race.DailyWorldEvent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaceWorldEventTest::RunTest(const FString& Parameters)
{
    UClass* ModeClass = LoadClass<Acasino_simulatorGameMode>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
    if (!TestNotNull(TEXT("GameMode class"), ModeClass)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(State);
    auto* Mode = World->SpawnActor<Acasino_simulatorGameMode>(ModeClass);
    auto* Manager = World->SpawnActor<ARaceManager>();
    if (!TestNotNull(TEXT("GameMode"), Mode) || !TestNotNull(TEXT("Manager"), Manager)) return false;
    Mode->GameState = State;
    Mode->bEnableDailyRaceEvent = true;
    Mode->DayDurationSeconds = 60.f;
    Manager->RunnerClass = ARaceRunner::StaticClass();
    auto* SpawnPoint = World->SpawnActor<ATargetPoint>();
    Manager->RunnerSpawnPoints.Add(SpawnPoint);
    Mode->PaymentSpawns.Add(SpawnPoint);
    auto TickTimers = [World](float Delta)
    {
        ++GFrameCounter;
        World->TimeSeconds += Delta;
        World->GetTimerManager().Tick(Delta);
    };
    FCasinoLoopStatus Status;
    Status.Phase = ECasinoLoopPhase::DayIntro;
    State->SetLoopStatus(Status);
    Mode->ActivateCasinoDay();
    TickTimers(0.f);
    TestTrue(TEXT("Day activation resolves the level manager"), Mode->ScheduledRaceManager.Get() == Manager);
    TestEqual(TEXT("Race starts halfway through the day"),
        World->GetTimerManager().GetTimerRemaining(Mode->RaceEventTimer), 30.f);
    TestEqual(TEXT("Announcement is ten seconds earlier"),
        World->GetTimerManager().GetTimerRemaining(Mode->RaceAnnouncementTimer), 20.f);
    TickTimers(19.9f);
    TestTrue(TEXT("No early announcement"), World->GetTimerManager().IsTimerActive(Mode->RaceAnnouncementTimer));
    TestEqual(TEXT("No early runners"), Manager->GetRunners().Num(), 0);
    TickTimers(0.2f);
    TestFalse(TEXT("Announcement timer fires once"), World->GetTimerManager().IsTimerActive(Mode->RaceAnnouncementTimer));
    TestEqual(TEXT("Announcement does not spawn runners"), Manager->GetRunners().Num(), 0);
    TickTimers(9.8f);
    TestTrue(TEXT("Manager stays idle before midpoint"), Manager->Phase == ERacePhase::Idle);
    TickTimers(0.2f);
    TestTrue(TEXT("Midpoint starts entry, not racing"), Manager->Phase == ERacePhase::Entering);
    TestEqual(TEXT("One scheduled round"), Manager->CurrentRoundNumber, 1);
    TestFalse(TEXT("Scheduled reference cleared after start"), Mode->ScheduledRaceManager.IsValid());

    // A completed round must be reset before the next day's start.
    Manager->Phase = ERacePhase::Finished;
    Mode->StartRaceRound(Manager);
    TestEqual(TEXT("Completed round can restart"), Manager->CurrentRoundNumber, 2);
    TestTrue(TEXT("Restart begins entry"), Manager->Phase == ERacePhase::Entering);
    Mode->StartRaceRound(Manager);
    TestEqual(TEXT("Active round is not restarted"), Manager->CurrentRoundNumber, 2);

    World->GetTimerManager().ClearTimer(Mode->DayLoopTimer);
    Mode->RaceEventManager = Manager;
    Mode->ScheduleRaceEvent(60.f);
    TickTimers(0.f);
    Mode->BeginPaymentPhase();
    TestFalse(TEXT("Day end cancels race timer"), World->GetTimerManager().IsTimerActive(Mode->RaceEventTimer));
    TestFalse(TEXT("Day end cancels announcement timer"), World->GetTimerManager().IsTimerActive(Mode->RaceAnnouncementTimer));
    TickTimers(31.f);
    TestEqual(TEXT("Cancelled schedule never starts another round"), Manager->CurrentRoundNumber, 2);

    Status.Phase = ECasinoLoopPhase::Playing;
    State->SetLoopStatus(Status);
    Mode->bEnableDailyRaceEvent = false;
    Mode->ScheduleRaceEvent(60.f);
    TestFalse(TEXT("Disabled event is not scheduled"), World->GetTimerManager().IsTimerActive(Mode->RaceEventTimer));
    Mode->bEnableDailyRaceEvent = true;
    Manager->Phase = ERacePhase::Finished;
    Mode->ScheduleRaceEvent(10.f);
    TickTimers(0.f);
    TestFalse(TEXT("Short day announces immediately instead of setting a zero timer"),
        World->GetTimerManager().IsTimerActive(Mode->RaceAnnouncementTimer));
    TestEqual(TEXT("Short day still starts at midpoint"),
        World->GetTimerManager().GetTimerRemaining(Mode->RaceEventTimer), 5.f);
    TickTimers(5.1f);
    TestEqual(TEXT("Short day starts one new round"), Manager->CurrentRoundNumber, 3);

    Mode->ScheduleRaceEvent(60.f);
    TickTimers(0.f);
    Manager->Destroy();
    TickTimers(30.1f);
    TestFalse(TEXT("Destroyed manager does not leave a schedule"), Mode->ScheduledRaceManager.IsValid());

    UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
    if (!TestNotNull(TEXT("Controller class"), ControllerClass)) return false;
    auto* PC = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass);
    auto* HUD = CreateWidget<URaceAnnouncementTestHUD>(World);
    auto* HUDProperty = FindFProperty<FObjectPropertyBase>(Acasino_simulatorPlayerController::StaticClass(), TEXT("PlayerHUDWidget"));
    if (!TestNotNull(TEXT("Controller"), PC) || !TestNotNull(TEXT("HUD"), HUD)
        || !TestNotNull(TEXT("HUD property"), HUDProperty)) return false;
    // Client RPCs require an owning player even in a standalone test world.
    PC->Player = NewObject<ULocalPlayer>(GEngine);
    PC->Player->PlayerController = PC;
    PC->SetAsLocalPlayerController();
    HUDProperty->SetObjectPropertyValue_InContainer(PC, HUD);
    PC->UIManager->UIScreen = ECasinoUIScreen::Playing;
    PC->ClientShowWorldEventAnnouncement();
    TestEqual(TEXT("Client RPC reaches the player HUD event"), HUD->ShowCount, 1);
    PC->ClientShowWorldEventAnnouncement();
    TestEqual(TEXT("Repeated announcement reaches the same HUD"), HUD->ShowCount, 2);
    PC->UIManager->ClearWorldEventAnnouncement();
    TestEqual(TEXT("Cleanup reaches the HUD clear event"), HUD->ClearCount, 1);
    PC->UIManager->UIScreen = ECasinoUIScreen::Payment;
    PC->ClientShowWorldEventAnnouncement();
    TestEqual(TEXT("Announcements outside gameplay are ignored"), HUD->ShowCount, 2);
    return true;
}

#endif
