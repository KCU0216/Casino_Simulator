#if WITH_DEV_AUTOMATION_TESTS
#include "casino_simulatorGameMode.h"
#include "casino_simulatorPlayerController.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerState.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "UI/CasinoUIManagerComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoRunResultsTest, "Casino.Results.RunHistory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoRunResultsTest::RunTest(const FString& Parameters)
{
    UClass* ModeClass = LoadClass<Acasino_simulatorGameMode>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
    UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
    UClass* PawnClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Game mode class"), ModeClass)
        || !TestNotNull(TEXT("Controller class"), ControllerClass)
        || !TestNotNull(TEXT("Pawn class"), PawnClass)) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* GS = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(GS);
    auto* Mode = World->SpawnActor<Acasino_simulatorGameMode>(ModeClass);
    if (!TestNotNull(TEXT("Game mode"), Mode) || !TestNotNull(TEXT("Game state"), GS)) return false;
    Mode->GameState = GS;
    Mode->PaymentRadius = 1000.f;
    Mode->PaymentSpawns.Add(World->SpawnActor<ATargetPoint>());

    auto* PawnState = FindFProperty<FObjectPropertyBase>(APawn::StaticClass(), TEXT("PlayerState"));
    auto* ControllerState = FindFProperty<FObjectPropertyBase>(AController::StaticClass(), TEXT("PlayerState"));
    if (!TestNotNull(TEXT("Pawn player state property"), PawnState)
        || !TestNotNull(TEXT("Controller player state property"), ControllerState)) return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Acasino_simulatorCharacter* Pawns[3];
    Acasino_simulatorPlayerState* States[3];
    Acasino_simulatorPlayerController* Controllers[3];
    const TCHAR* Names[] = {TEXT("Alpha"), TEXT("Beta"), TEXT("Gamma")};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Pawns[Index] = World->SpawnActor<Acasino_simulatorCharacter>(PawnClass,
            FVector(Index * 100.f, 0.f, 100.f), FRotator::ZeroRotator, Spawn);
        States[Index] = World->SpawnActor<Acasino_simulatorPlayerState>();
        Controllers[Index] = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass,
            FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        if (!TestNotNull(TEXT("Pawn fixture"), Pawns[Index])
            || !TestNotNull(TEXT("State fixture"), States[Index])
            || !TestNotNull(TEXT("Controller fixture"), Controllers[Index])) return false;
        World->AddController(Controllers[Index]);
        GS->AddPlayerState(States[Index]);
        States[Index]->SetPlayerName(Names[Index]);
        PawnState->SetObjectPropertyValue_InContainer(Pawns[Index], States[Index]);
        ControllerState->SetObjectPropertyValue_InContainer(Controllers[Index], States[Index]);
        Controllers[Index]->UIManager->PrepareTravel();
        Controllers[Index]->SetPawn(Pawns[Index]);
        auto* ASC = Pawns[Index]->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Pawns[Index], Pawns[Index]);
        ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    }
    auto SetWallet = [&](int32 Index, float Amount)
    {
        Pawns[Index]->GetAbilitySystemComponent()->SetNumericAttributeBase(
            Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), Amount);
    };
    auto BeginPayment = [&](int32 Day, int32 FinalDay, int32 Required, int32 Count)
    {
        World->GetTimerManager().ClearTimer(Mode->NextDayTimer);
        Mode->PaymentParticipants.Reset();
        FCasinoLoopStatus Status;
        Status.CurrentDay = Day;
        Status.FinalDay = FinalDay;
        Status.RequiredPayment = Required;
        Status.Phase = ECasinoLoopPhase::Settling;
        Status.PaymentParticipantCount = Count;
        Status.PaymentEndServerTime = GS->GetServerWorldTimeSeconds() + 30.0;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            States[Index]->bDailyPaymentSubmitted = false;
            States[Index]->DailyPaymentAmount = 0;
            Mode->PaymentParticipants.Add(States[Index]);
            Mode->RegisterRunPlayer(States[Index], Pawns[Index]).PendingPaymentDay = Day;
        }
        GS->SetLoopStatus(Status);
    };
    auto FindPlayer = [&](const TCHAR* Name) -> const FCasinoRunPlayerResult*
    {
        return GS->RunResults.Players.FindByPredicate(
            [Name](const auto& Row) { return Row.PlayerName == Name; });
    };

    Mode->ResetRunResults();
    SetWallet(0, 1000.f); SetWallet(1, 900.f); SetWallet(2, 800.f);
    BeginPayment(1, 3, 100, 3);
    TestTrue(TEXT("Over-request is accepted with the actual daily cap"), Mode->SubmitDailyPayment(Pawns[0], 5000));
    TestTrue(TEXT("Second player's explicit zero accepted"), Mode->SubmitDailyPayment(Pawns[1], 0));
    TestTrue(TEXT("Third player's explicit zero accepted"), Mode->SubmitDailyPayment(Pawns[2], 0));
    TestEqual(TEXT("First day passes"), GS->LoopStatus.Phase, ECasinoLoopPhase::DayPassed);
    TestFalse(TEXT("A successful intermediate day cannot publish private history"), GS->RunResults.bFinalized);
    TestTrue(TEXT("Replicated player records stay empty during the run"), GS->RunResults.Players.IsEmpty());
    TestTrue(TEXT("Replicated graph days also stay empty during the run"), GS->RunResults.Days.IsEmpty());

    SetWallet(0, 1200.f); SetWallet(1, 800.f); SetWallet(2, 800.f);
    BeginPayment(2, 3, 200, 3);
    TestTrue(TEXT("Next actual payment accepted"), Mode->SubmitDailyPayment(Pawns[0], 200));
    // The server timeout path supplies the remaining zeros without spending their money.
    Mode->FinishPaymentPhase();
    TestFalse(TEXT("Second successful day remains private"), GS->RunResults.bFinalized);
    BeginPayment(3, 3, 10000, 3);
    Mode->FinishPaymentPhase();
    TestEqual(TEXT("Final insufficient total fails the run"), GS->LoopStatus.Phase, ECasinoLoopPhase::GameOver);
    TestTrue(TEXT("Failure publishes a frozen result"), GS->RunResults.bFinalized);
    TestEqual(TEXT("Failure day is included"), GS->RunResults.Days.Num(), 3);
    const auto* Alpha = FindPlayer(TEXT("Alpha"));
    const auto* Beta = FindPlayer(TEXT("Beta"));
    const auto* Gamma = FindPlayer(TEXT("Gamma"));
    if (!TestNotNull(TEXT("Alpha's row"), Alpha) || !TestNotNull(TEXT("Beta's row"), Beta)
        || !TestNotNull(TEXT("Gamma's row"), Gamma)) return false;
    TestEqual(TEXT("History uses the first post-payment wallet"), Alpha->DailyBalances[0].Balance, int64(900));
    TestEqual(TEXT("Later changes cannot rewrite the first day"), Alpha->DailyBalances.Num(), 3);
    TestEqual(TEXT("Total counts actual capped payments across days"), Alpha->TotalContribution, int64(300));
    TestEqual(TEXT("Timeout does not invent a contribution"), Beta->TotalContribution, int64(0));
    TestEqual(TEXT("Final ranking uses the last wallet instead of summed history"), Alpha->FinalBalance, int64(1000));
    TestEqual(TEXT("Highest wallet is first"), Alpha->Rank, 1);
    TestEqual(TEXT("Equal wallets share a rank"), Beta->Rank, Gamma->Rank);
    TestEqual(TEXT("Tied runners-up are second"), Beta->Rank, 2);
    SetWallet(0, 99999.f);
    Mode->FinalizeRunResults();
    TestEqual(TEXT("Published final result cannot change after ending"), FindPlayer(TEXT("Alpha"))->FinalBalance, int64(1000));

    Mode->ResetRunResults();
    TestFalse(TEXT("Restart clears finalized flag"), GS->RunResults.bFinalized);
    TestTrue(TEXT("Restart clears old player records"), GS->RunResults.Players.IsEmpty());
    for (int32 Index = 0; Index < 3; ++Index) SetWallet(Index, 100.f);
    BeginPayment(1, 5, 1000, 3);
    Mode->FinishPaymentPhase();
    TestEqual(TEXT("First-day failure has only one day for ranking-only presentation"), GS->RunResults.Days.Num(), 1);
    TestEqual(TEXT("All equal wallets share first place"), GS->RunResults.Players.Last().Rank, 1);
    TestEqual(TEXT("Previous run's contributions are cleared"), FindPlayer(TEXT("Alpha"))->TotalContribution, int64(0));

    Mode->ResetRunResults();
    for (int32 Index = 0; Index < 3; ++Index) SetWallet(Index, 1000.f);
    BeginPayment(1, 1, 100, 3);
    TestTrue(TEXT("Clear payment accepted"), Mode->SubmitDailyPayment(Pawns[0], 100));
    TestTrue(TEXT("Clearing second zero accepted"), Mode->SubmitDailyPayment(Pawns[1], 0));
    TestTrue(TEXT("Clearing third zero accepted"), Mode->SubmitDailyPayment(Pawns[2], 0));
    TestEqual(TEXT("Last-day success clears"), GS->LoopStatus.Phase, ECasinoLoopPhase::Cleared);
    TestTrue(TEXT("Clear also publishes results"), GS->RunResults.bFinalized);
    TestEqual(TEXT("Competition rank skips tied positions"), FindPlayer(TEXT("Alpha"))->Rank, 3);

    // A late arrival has no fabricated zero balance on earlier days.
    Mode->ResetRunResults();
    World->RemoveController(Controllers[2]);
    SetWallet(0, 1000.f); SetWallet(1, 1000.f);
    BeginPayment(1, 2, 100, 2);
    TestTrue(TEXT("Early player's payment accepted"), Mode->SubmitDailyPayment(Pawns[0], 100));
    TestTrue(TEXT("Other early player's zero accepted"), Mode->SubmitDailyPayment(Pawns[1], 0));
    World->AddController(Controllers[2]);
    BeginPayment(2, 2, 100, 3);
    TestTrue(TEXT("Second-day payment accepted"), Mode->SubmitDailyPayment(Pawns[0], 100));
    TestTrue(TEXT("Second-day zero accepted"), Mode->SubmitDailyPayment(Pawns[1], 0));
    TestTrue(TEXT("Late player zero accepted"), Mode->SubmitDailyPayment(Pawns[2], 0));
    TestEqual(TEXT("Late join keeps only the day they actually attended"), FindPlayer(TEXT("Gamma"))->DailyBalances.Num(), 1);
    TestEqual(TEXT("Late join snapshot keeps the actual day"), FindPlayer(TEXT("Gamma"))->DailyBalances[0].Day, 2);

    // Simulate the engine's real disconnect order: pawn destruction precedes Logout.
    Mode->ResetRunResults();
    for (int32 Index = 0; Index < 3; ++Index) SetWallet(Index, 1000.f);
    BeginPayment(1, 3, 100, 3);
    TestTrue(TEXT("Departing player's actual payment accepted"), Mode->SubmitDailyPayment(Pawns[0], 100));
    const int32 DepartingId = Mode->RunPlayerHistory[0].Result.RunPlayerId;
    Pawns[0]->Destroy();
    Controllers[0]->SetPawn(nullptr);
    Mode->Logout(Controllers[0]);
    World->RemoveController(Controllers[0]);
    States[0]->Destroy();
    TestTrue(TEXT("Remaining player zero accepted"), Mode->SubmitDailyPayment(Pawns[1], 0));
    TestTrue(TEXT("Last remaining player zero accepted"), Mode->SubmitDailyPayment(Pawns[2], 0));
    World->GetTimerManager().ClearTimer(Mode->NextDayTimer);
    Mode->PaymentParticipants.Reset();
    for (int32 Index = 1; Index < 3; ++Index)
    {
        States[Index]->bDailyPaymentSubmitted = false;
        States[Index]->DailyPaymentAmount = 0;
        Mode->PaymentParticipants.Add(States[Index]);
    }
    FCasinoLoopStatus Failure;
    Failure.Phase = ECasinoLoopPhase::Settling;
    Failure.CurrentDay = 2;
    Failure.FinalDay = 3;
    Failure.RequiredPayment = 100;
    Failure.PaymentParticipantCount = 2;
    GS->SetLoopStatus(Failure);
    Mode->FinishPaymentPhase();
    Alpha = FindPlayer(TEXT("Alpha"));
    if (!TestNotNull(TEXT("Disconnected player's result survives"), Alpha)) return false;
    TestTrue(TEXT("Disconnected result is labeled"), Alpha->bDisconnected);
    TestEqual(TEXT("Disconnected participant retains a stable run identity"), Alpha->RunPlayerId, DepartingId);
    TestEqual(TEXT("Pawn destruction preserves the post-payment wallet"), Alpha->FinalBalance, int64(900));
    TestEqual(TEXT("Payment survives participant disconnection"), Alpha->TotalContribution, int64(100));
    TestEqual(TEXT("Departed participant has no fictitious subsequent snapshot"), Alpha->DailyBalances.Num(), 1);
    return true;
}
#endif
