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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoPaymentGrowthFloorTest, "Casino.DayLoop.PaymentGrowthFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoPaymentGrowthFloorTest::RunTest(const FString& Parameters)
{
    UClass* ModeClass = LoadClass<Acasino_simulatorGameMode>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
    UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Mode class"), ModeClass)
        || !TestNotNull(TEXT("Controller class"), ControllerClass)
        || !TestNotNull(TEXT("Player class"), PlayerClass)) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(State);
    auto* Mode = World->SpawnActor<Acasino_simulatorGameMode>(ModeClass);
    if (!TestNotNull(TEXT("Loop state"), State) || !TestNotNull(TEXT("Game mode"), Mode)) return false;
    Mode->GameState = State;
    Mode->DefaultDailyPayment = 100;
    Mode->DailyPaymentBaseMultiplier = 1.5;
    Mode->DailyPaymentMultiplierIncreasePerDay = 0.0;
    Mode->PaymentRadius = 1000.f;

    auto* PawnState = FindFProperty<FObjectPropertyBase>(APawn::StaticClass(), TEXT("PlayerState"));
    auto* ControllerState = FindFProperty<FObjectPropertyBase>(AController::StaticClass(), TEXT("PlayerState"));
    if (!TestNotNull(TEXT("Pawn state property"), PawnState)
        || !TestNotNull(TEXT("Controller state property"), ControllerState)) return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Acasino_simulatorCharacter* Players[2];
    Acasino_simulatorPlayerState* States[2];
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Players[Index] = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass,
            FVector(Index * 500.f, 0.f, 100.f), FRotator::ZeroRotator, Spawn);
        States[Index] = World->SpawnActor<Acasino_simulatorPlayerState>();
        auto* PC = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass,
            FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        if (!TestNotNull(TEXT("Player"), Players[Index])
            || !TestNotNull(TEXT("Player state"), States[Index])
            || !TestNotNull(TEXT("Controller"), PC)) return false;
        World->AddController(PC);
        State->AddPlayerState(States[Index]);
        PawnState->SetObjectPropertyValue_InContainer(Players[Index], States[Index]);
        ControllerState->SetObjectPropertyValue_InContainer(PC, States[Index]);
        PC->UIManager->PrepareTravel();
        PC->SetPawn(Players[Index]);
        auto* ASC = Players[Index]->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Players[Index], Players[Index]);
        ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
        Mode->PaymentSpawns.Add(World->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(),
            FVector(Index * 500.f, 1000.f, 100.f), FRotator::ZeroRotator, Spawn));
    }
    auto SetWallet = [&](int32 Index, float Amount)
    {
        Players[Index]->GetAbilitySystemComponent()->SetNumericAttributeBase(
            Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), Amount);
    };
    auto Collect = [&](int32 Day, int32 Required)
    {
        World->GetTimerManager().ClearTimer(Mode->NextDayTimer);
        Mode->PaymentParticipants.Reset();
        FCasinoLoopStatus Status;
        Status.CurrentDay = Day;
        Status.FinalDay = 10;
        Status.RequiredPayment = Required;
        Status.Phase = ECasinoLoopPhase::Settling;
        Status.PaymentParticipantCount = 2;
        Status.PaymentEndServerTime = State->GetServerWorldTimeSeconds() + 30.0;
        for (auto* PS : States)
        {
            PS->bDailyPaymentSubmitted = false;
            PS->DailyPaymentAmount = 0;
            Mode->PaymentParticipants.Add(PS);
        }
        State->SetLoopStatus(Status);
    };

    SetWallet(0, 40.f);
    SetWallet(1, 60.f);
    Collect(1, 100);
    TestTrue(TEXT("First all-in contribution accepted"), Mode->SubmitDailyPayment(Players[0], 40));
    TestTrue(TEXT("Second all-in contribution accepted"), Mode->SubmitDailyPayment(Players[1], 60));
    TestEqual(TEXT("All-in team clears today's bill"), State->LoopStatus.Phase, ECasinoLoopPhase::DayPassed);
    TestEqual(TEXT("Post-payment team snapshot is zero"), Mode->PreviousDayRemainingCurrency, 0.0);
    Mode->AdvanceCasinoDay();
    TestEqual(TEXT("Tomorrow remains 150 after the entire team paid all cash"), State->LoopStatus.RequiredPayment, 150);

    SetWallet(0, 1000.f);
    SetWallet(1, 500.f);
    Collect(2, 150);
    TestTrue(TEXT("First wealthy contribution accepted"), Mode->SubmitDailyPayment(Players[0], 100));
    TestTrue(TEXT("Second wealthy contribution accepted"), Mode->SubmitDailyPayment(Players[1], 50));
    TestEqual(TEXT("Both server wallets contribute to the remaining balance"), Mode->PreviousDayRemainingCurrency, 1350.0);
    TestEqual(TEXT("High remaining team balance dominates the bill floor"), Mode->CalculateRequiredPayment(3), 2025);
    SetWallet(0, 1.e20f);
    SetWallet(1, 0.f);
    States[1]->SetIsInactive(true);
    TestEqual(TEXT("Result-screen wallet and participant changes cannot rewrite tomorrow"), Mode->CalculateRequiredPayment(3), 2025);
    States[1]->SetIsInactive(false);
    Mode->AdvanceCasinoDay();
    TestEqual(TEXT("Day intro publishes the frozen next-day bill"), State->LoopStatus.RequiredPayment, 2025);
    Mode->DailyPaymentMultiplierIncreasePerDay = 0.1;
    TestEqual(TEXT("Configured day-three increase applies to both branches"), Mode->CalculateRequiredPayment(3), 2160);
    Mode->DailyPaymentMultiplierIncreasePerDay = 0.0;

    SetWallet(0, 1150.f);
    SetWallet(1, 875.f);
    Collect(3, 2025);
    TestTrue(TEXT("Following all-in first contribution accepted"), Mode->SubmitDailyPayment(Players[0], 1150));
    TestTrue(TEXT("Following all-in second contribution accepted"), Mode->SubmitDailyPayment(Players[1], 875));
    TestEqual(TEXT("Fractional growth floor rounds up"), Mode->CalculateRequiredPayment(4), 3038);

    SetWallet(0, 100.f);
    SetWallet(1, 100.f);
    Collect(4, 100);
    TestTrue(TEXT("Voluntary first full contribution accepted"), Mode->SubmitDailyPayment(Players[0], 100));
    TestTrue(TEXT("Voluntary second full contribution accepted"), Mode->SubmitDailyPayment(Players[1], 100));
    TestEqual(TEXT("Floor tracks required bill, not voluntary overpayment"), Mode->PreviousDayRequiredPayment, 100);
    TestEqual(TEXT("Voluntary overpayment cannot replace the required-bill floor"), Mode->CalculateRequiredPayment(5), 150);

    SetWallet(0, 1000.f);
    SetWallet(1, 1000.f);
    States[1]->SetIsOnlyASpectator(true);
    Collect(5, 100);
    TestTrue(TEXT("Active player's contribution accepted"), Mode->SubmitDailyPayment(Players[0], 100));
    Mode->FinishPaymentPhase();
    TestEqual(TEXT("Spectator wallet excluded from payment snapshot"), Mode->PreviousDayRemainingCurrency, 900.0);
    States[1]->SetIsOnlyASpectator(false);

    SetWallet(0, 1.e20f);
    Collect(6, 100);
    TestTrue(TEXT("Large-wallet payment accepted"), Mode->SubmitDailyPayment(Players[0], 100));
    Mode->FinishPaymentPhase();
    TestEqual(TEXT("Huge remaining balance saturates integer bill"), Mode->CalculateRequiredPayment(7), MAX_int32);
    Mode->PreviousDayRequiredPayment = MAX_int32;
    Mode->PreviousDayRemainingCurrency = 0.0;
    TestEqual(TEXT("Huge floor saturates without overflow"), Mode->CalculateRequiredPayment(7), MAX_int32);

    Mode->BeginCasinoDay(1);
    TestEqual(TEXT("Restarting day one restores the fixed first bill"), State->LoopStatus.RequiredPayment, 100);
    TestEqual(TEXT("Restart clears prior successful payment day"), Mode->PreviousPaymentDay, 0);
    TestEqual(TEXT("Restart clears prior remaining team balance"), Mode->PreviousDayRemainingCurrency, 0.0);
    return true;
}
#endif
