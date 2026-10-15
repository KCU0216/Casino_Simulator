#if WITH_DEV_AUTOMATION_TESTS

#include "casino_simulatorGameMode.h"
#include "casino_simulatorPlayerController.h"
#include "casino_simulatorCharacter.h"
#include "UI/CasinoUIManagerComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "IpNetDriver.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoMatchLoadingBarrierTest,
    "Casino.Online.MatchLoadingBarrier",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoMatchLoadingBarrierTest::RunTest(const FString& Parameters)
{
    UClass* ModeClass = LoadClass<Acasino_simulatorGameMode>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
    UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
    UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Shipped mode"), ModeClass)
        || !TestNotNull(TEXT("Shipped controller"), ControllerClass)
        || !TestNotNull(TEXT("Shipped character"), CharacterClass)) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(State);
    auto* Mode = World->SpawnActor<Acasino_simulatorGameMode>(ModeClass);
    if (!TestNotNull(TEXT("Loop state"), State) || !TestNotNull(TEXT("Game mode"), Mode)) return false;
    Mode->GameState = State;
    Mode->DefaultDailyPayment = 100;

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // NetConnection's public world-package setter also refreshes driver-owned
    // visibility state. A real driver supplies that state without opening a socket.
    auto* TestDriver = NewObject<UIpNetDriver>(GEngine);
    TestDriver->SetWorld(World);
    ON_SCOPE_EXIT { TestDriver->SetWorld(nullptr); };
    Acasino_simulatorPlayerController* Controllers[2];
    Acasino_simulatorCharacter* Characters[2];
    USimulatedClientNetConnection* Connections[2];
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Controllers[Index] = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass,
            FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        Characters[Index] = World->SpawnActor<Acasino_simulatorCharacter>(CharacterClass,
            FVector(Index * 700.f, -1000.f, 200.f), FRotator::ZeroRotator, Spawn);
        if (!TestNotNull(TEXT("Controller"), Controllers[Index])
            || !TestNotNull(TEXT("Character"), Characters[Index])) return false;
        Controllers[Index]->GetUIManager()->PrepareTravel();
        // Keep possession callbacks independent of the simulated peer attachment.
        Controllers[Index]->SetPawn(Characters[Index]);
        Connections[Index] = NewObject<USimulatedClientNetConnection>(GEngine);
        Connections[Index]->Driver = TestDriver;
        Controllers[Index]->Player = Connections[Index];
        Connections[Index]->PlayerController = Controllers[Index];
        Connections[Index]->SetClientWorldPackageName(World->GetOutermost()->GetFName());
        auto* Point = World->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(),
            FVector(Index * 700.f, 1000.f, 200.f), FRotator(0.f, Index * 90.f, 0.f), Spawn);
        Point->Tags.Add(Mode->PaymentSpawnTag);
    }
    ON_SCOPE_EXIT
    {
        for (int32 Index = 0; Index < 2; ++Index)
        {
            Controllers[Index]->Player = nullptr;
            Connections[Index]->PlayerController = nullptr;
        }
    };
    // Only the first player has entered the destination world so far.
    World->RemoveController(Controllers[1]);
    World->AddController(Controllers[0]);
    // Seamless travel leaves the slow client's lobby controller in the server
    // world until LoadedWorld triggers its replacement with the game controller.
    auto* OldLobbyController = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass,
        FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    auto* OldLobbyConnection = NewObject<USimulatedClientNetConnection>(GEngine);
    OldLobbyConnection->Driver = TestDriver;
    OldLobbyController->Player = OldLobbyConnection;
    OldLobbyConnection->PlayerController = OldLobbyController;
    OldLobbyConnection->SetClientWorldPackageName(TEXT("/Game/OldLobby"));
    World->AddController(OldLobbyController);
    Mode->BeginOnlineLoading(2);
    const FGuid Generation = State->MatchLoadingStatus.Generation;
    TestTrue(TEXT("Each travel gets a valid generation"), Generation.IsValid());
    TestEqual(TEXT("Arrival reports one of two"), State->MatchLoadingStatus.ArrivedPlayers, 1);
    TestEqual(TEXT("A retained lobby controller is not captured as a game participant"), Mode->OnlineLoadingParticipants.Num(), 1);
    TestEqual(TEXT("A retained lobby controller does not inflate the expected player count"), State->MatchLoadingStatus.ExpectedPlayers, 2);
    OldLobbyController->Player = nullptr;
    OldLobbyConnection->PlayerController = nullptr;
    World->RemoveController(OldLobbyController);
    OldLobbyController->Destroy();
    TestTrue(TEXT("Server freezes the first arrival"), Characters[0]->GetCharacterMovement()->MovementMode == MOVE_None);
    Mode->NotifyClientWorldReady(Controllers[0], Generation);
    TestEqual(TEXT("Ready first player still waits for the delayed second"), State->LoopStatus.Phase, ECasinoLoopPhase::Waiting);
    TestFalse(TEXT("Loading cannot consume the day timer"), World->GetTimerManager().IsTimerActive(Mode->DayLoopTimer));
    Mode->StartDayLoop();
    TestEqual(TEXT("Blueprint start cannot bypass the loading barrier"), State->LoopStatus.Phase, ECasinoLoopPhase::Waiting);
    Mode->NotifyClientWorldReady(Controllers[0], Generation);
    Mode->NotifyClientWorldReady(Controllers[1], Generation);
    TestEqual(TEXT("Duplicate and unregistered acknowledgements do not add readiness"), State->MatchLoadingStatus.ReadyPlayers, 1);

    World->AddController(Controllers[1]);
    Connections[1]->SetClientWorldPackageName(TEXT("/Game/OldLobby"));
    Mode->WaitForOnlinePlayers();
    Mode->NotifyClientWorldReady(Controllers[1], Generation);
    TestEqual(TEXT("Client still in the old world cannot acknowledge"), State->MatchLoadingStatus.ReadyPlayers, 1);
    Connections[1]->SetClientWorldPackageName(World->GetOutermost()->GetFName());
    Mode->WaitForOnlinePlayers();
    Mode->NotifyClientWorldReady(Controllers[1], FGuid::NewGuid());
    TestEqual(TEXT("An old or forged generation cannot acknowledge"), State->MatchLoadingStatus.ReadyPlayers, 1);
    Mode->NotifyClientWorldReady(Controllers[1], Generation);
    TestEqual(TEXT("All local load acknowledgements begin final spawn preparation"), State->MatchLoadingStatus.Stage, ECasinoMatchLoadingStage::PreparingSpawns);
    TestEqual(TEXT("Spawn preparation still does not begin day intro"), State->LoopStatus.Phase, ECasinoLoopPhase::Waiting);
    TestEqual(TEXT("Spawn readiness starts empty"), State->MatchLoadingStatus.ReadyPlayers, 0);
    FVector FinalLocations[2];
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* Entry = Mode->OnlineLoadingParticipants.FindByPredicate([&, Index](const auto& Item)
            { return Item.Player == Controllers[Index]; });
        if (!TestNotNull(TEXT("Frozen participant"), Entry)) return false;
        FinalLocations[Index] = Entry->SpawnTransform.GetLocation();
        TestTrue(TEXT("Each server pawn is already at its assigned spawn"), Characters[Index]->GetActorLocation().Equals(FinalLocations[Index], 0.1f));
        TestTrue(TEXT("Every pawn stays immobile before the final barrier"), Characters[Index]->GetCharacterMovement()->MovementMode == MOVE_None);
    }
    TestFalse(TEXT("Players receive different start locations"), FinalLocations[0].Equals(FinalLocations[1]));
    Mode->NotifyClientSpawnReady(Controllers[0], FGuid::NewGuid());
    TestEqual(TEXT("Stale final acknowledgements have no effect"), State->MatchLoadingStatus.ReadyPlayers, 0);
    Mode->NotifyClientSpawnReady(Controllers[0], Generation);
    Mode->NotifyClientSpawnReady(Controllers[0], Generation);
    TestEqual(TEXT("First final acknowledgement is counted once"), State->MatchLoadingStatus.ReadyPlayers, 1);
    TestEqual(TEXT("Slow second spawn still holds all players"), State->LoopStatus.Phase, ECasinoLoopPhase::Waiting);
    Characters[1]->SetActorLocation(FinalLocations[1] + FVector(100.f, 0.f, 0.f));
    Mode->NotifyClientSpawnReady(Controllers[1], Generation);
    TestEqual(TEXT("A player outside the assigned spawn cannot acknowledge final placement"), State->MatchLoadingStatus.ReadyPlayers, 1);
    Characters[1]->SetActorLocation(FinalLocations[1]);
    Mode->NotifyClientSpawnReady(Controllers[1], Generation);
    TestEqual(TEXT("All final acknowledgements complete loading"), State->MatchLoadingStatus.Stage, ECasinoMatchLoadingStage::Complete);
    TestEqual(TEXT("Day intro begins only after all clients accept their final spawn"), State->LoopStatus.Phase, ECasinoLoopPhase::DayIntro);
    TestEqual(TEXT("Loading retains the existing first-day bill"), State->LoopStatus.RequiredPayment, 100);
    TestFalse(TEXT("Day gameplay clock still waits for the intro"), World->GetTimerManager().IsTimerActive(Mode->DayLoopTimer));
    for (int32 Index = 0; Index < 2; ++Index)
        TestTrue(TEXT("Beginning day intro does not teleport again"), Characters[Index]->GetActorLocation().Equals(FinalLocations[Index], 0.1f));
    const double IntroEnd = State->LoopStatus.IntroEndServerTime;
    Mode->NotifyClientSpawnReady(Controllers[1], Generation);
    TestEqual(TEXT("Repeated acknowledgements do not restart the intro clock"), State->LoopStatus.IntroEndServerTime, IntroEnd);

    World->GetTimerManager().ClearTimer(Mode->IntroTimer);
    State->SetLoopStatus(FCasinoLoopStatus());
    Mode->BeginOnlineLoading(3);
    TestFalse(TEXT("Restart travel invalidates the preceding generation"), State->MatchLoadingStatus.Generation == Generation);
    Mode->NotifyClientWorldReady(Controllers[0], Generation);
    TestEqual(TEXT("Preceding match acknowledgement cannot unblock a new travel"), State->MatchLoadingStatus.ReadyPlayers, 0);
    Mode->Logout(Controllers[1]);
    TestEqual(TEXT("Disconnect removes the known participant from required count"), State->MatchLoadingStatus.ExpectedPlayers, 2);
    TestEqual(TEXT("Logging out controller is not immediately registered again"), Mode->OnlineLoadingParticipants.Num(), 1);
    Mode->OnlineArrivalDeadline = FPlatformTime::Seconds() - 1.0;
    Mode->WaitForOnlinePlayers();
    TestEqual(TEXT("Never-arrived player produces a visible loading failure"), State->MatchLoadingStatus.Stage, ECasinoMatchLoadingStage::Failed);
    TestFalse(TEXT("Failure supplies a reason for the exit screen"), State->MatchLoadingStatus.FailureReason.IsEmpty());
    TestEqual(TEXT("Timeout never silently starts with a partial group"), State->LoopStatus.Phase, ECasinoLoopPhase::Waiting);
    Mode->StartDayLoop();
    TestEqual(TEXT("Failed loading cannot be bypassed by a later start call"), State->LoopStatus.Phase, ECasinoLoopPhase::Waiting);
    TestFalse(TEXT("Failure cancels the arrival timer"), World->GetTimerManager().IsTimerActive(Mode->OnlineArrivalTimer));

    // Offline/local runs keep their existing direct start path.
    State->SetMatchLoadingStatus(FCasinoMatchLoadingStatus());
    World->RemoveController(Controllers[1]);
    Mode->StartDayLoop();
    TestEqual(TEXT("Offline loading defaults do not require network acknowledgements"), State->LoopStatus.Phase, ECasinoLoopPhase::DayIntro);
    World->GetTimerManager().ClearAllTimersForObject(Mode);
    return true;
}
#endif
