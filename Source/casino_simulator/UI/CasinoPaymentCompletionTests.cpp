#if WITH_DEV_AUTOMATION_TESTS
#include "casino_simulatorGameMode.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerState.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoPaymentCompletionTest, "Casino.DayLoop.AllPlayersPayment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoPaymentCompletionTest::RunTest(const FString& Parameters)
{
    UClass* ModeClass = LoadClass<Acasino_simulatorGameMode>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Mode class"), ModeClass) || !TestNotNull(TEXT("Player class"), PlayerClass)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(State);
    auto* Mode = World->SpawnActor<Acasino_simulatorGameMode>(ModeClass);
    Mode->GameState = State;
    Mode->PaymentRadius = 1000.f;
    Mode->PaymentSpawns.Add(World->SpawnActor<ATargetPoint>());
    auto* PSProperty = FindFProperty<FObjectPropertyBase>(APawn::StaticClass(), TEXT("PlayerState"));
    if (!TestNotNull(TEXT("Pawn player-state property"), PSProperty)) return false;
    Acasino_simulatorCharacter* Players[4];
    Acasino_simulatorPlayerState* States[4];
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Players[Index] = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass,
            FVector(Index * 100.f, 0.f, 100.f), FRotator::ZeroRotator, Spawn);
        States[Index] = World->SpawnActor<Acasino_simulatorPlayerState>();
        PSProperty->SetObjectPropertyValue_InContainer(Players[Index], States[Index]);
        Mode->PaymentParticipants.Add(States[Index]);
        auto* ASC = Players[Index]->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Players[Index], Players[Index]);
        ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    }
    auto ResetPayment = [&]()
    {
        World->GetTimerManager().ClearTimer(Mode->PaymentTimer);
        World->GetTimerManager().ClearTimer(Mode->NextDayTimer);
        FCasinoLoopStatus Status;
        Status.Phase = ECasinoLoopPhase::Settling;
        Status.RequiredPayment = 100;
        Status.FinalDay = 5;
        Status.PaymentParticipantCount = 4;
        Status.PaymentEndServerTime = State->GetServerWorldTimeSeconds() + 10.0;
        State->SetLoopStatus(Status);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            States[Index]->bDailyPaymentSubmitted = false;
            States[Index]->DailyPaymentAmount = 0;
            Players[Index]->GetAbilitySystemComponent()->SetNumericAttributeBase(
                Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 1000.f);
        }
    };
    ResetPayment();
    TestTrue(TEXT("30 accepted"), Mode->SubmitDailyPayment(Players[0], 30));
    TestTrue(TEXT("70 accepted"), Mode->SubmitDailyPayment(Players[1], 70));
    TestEqual(TEXT("Target reached but still waiting"), State->LoopStatus.Phase, ECasinoLoopPhase::Settling);
    TestEqual(TEXT("Two submitted"), State->LoopStatus.PaymentSubmittedCount, 2);
    TestFalse(TEXT("Duplicate rejected"), Mode->SubmitDailyPayment(Players[0], 30));
    TestTrue(TEXT("Third may submit zero"), Mode->SubmitDailyPayment(Players[2], 0));
    TestEqual(TEXT("Still waiting for fourth"), State->LoopStatus.Phase, ECasinoLoopPhase::Settling);
    TestTrue(TEXT("Fourth may submit zero"), Mode->SubmitDailyPayment(Players[3], 0));
    TestEqual(TEXT("All submitted produces success"), State->LoopStatus.Phase, ECasinoLoopPhase::DayPassed);

    ResetPayment();
    TestTrue(TEXT("Over-target request accepted with cap"), Mode->SubmitDailyPayment(Players[0], 3000));
    TestEqual(TEXT("Only daily cap charged"), Players[0]->GetCurrency(), 900.f);
    TestEqual(TEXT("Full first contribution still waits"), State->LoopStatus.Phase, ECasinoLoopPhase::Settling);
    TestTrue(TEXT("Others can pay full target after target reached"), Mode->SubmitDailyPayment(Players[1], 100));
    TestEqual(TEXT("Second contribution independently charged"), Players[1]->GetCurrency(), 900.f);
    TestEqual(TEXT("Shared total can exceed target"), State->LoopStatus.CollectedPayment, 200);

    // Advance the actual deadline timer: unsubmitted players become zero without being charged.
    World->GetTimerManager().SetTimer(Mode->PaymentTimer, Mode,
        &Acasino_simulatorGameMode::FinishPaymentPhase, 10.f, false);
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
    ++GFrameCounter;
    World->GetTimerManager().Tick(0.f);
    World->TimeSeconds += 10.1f;
    ++GFrameCounter;
    World->GetTimerManager().Tick(10.1f);
    TestEqual(TEXT("Deadline finishes successful total"), State->LoopStatus.Phase, ECasinoLoopPhase::DayPassed);
    for (int32 Index = 2; Index < 4; ++Index)
    {
        TestTrue(TEXT("Timeout marks missing submission"), States[Index]->bDailyPaymentSubmitted);
        TestEqual(TEXT("Timeout amount is zero"), States[Index]->DailyPaymentAmount, 0);
        TestEqual(TEXT("Timeout does not spend money"), Players[Index]->GetCurrency(), 1000.f);
    }
    TestEqual(TEXT("Deadline counts all participants"), State->LoopStatus.PaymentSubmittedCount, 4);

    ResetPayment();
    for (int32 Index = 0; Index < 4; ++Index)
        TestTrue(TEXT("Explicit zero accepted"), Mode->SubmitDailyPayment(Players[Index], 0));
    TestEqual(TEXT("All zero produces failure"), State->LoopStatus.Phase, ECasinoLoopPhase::GameOver);
    TestFalse(TEXT("Submission after result rejected"), Mode->SubmitDailyPayment(Players[0], 100));
    return true;
}
#endif
