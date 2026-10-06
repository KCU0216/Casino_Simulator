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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoAdaptivePaymentTest, "Casino.DayLoop.AdaptivePayment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoAdaptivePaymentTest::RunTest(const FString& Parameters)
{
    UClass* ModeClass = LoadClass<Acasino_simulatorGameMode>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
    UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Gameplay mode class"), ModeClass)
        || !TestNotNull(TEXT("Gameplay controller class"), ControllerClass)
        || !TestNotNull(TEXT("Gameplay player class"), PlayerClass)) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(State);
    auto* Mode = World->SpawnActor<Acasino_simulatorGameMode>(ModeClass);
    if (!TestNotNull(TEXT("Gameplay mode"), Mode) || !TestNotNull(TEXT("Loop state"), State)) return false;
    Mode->GameState = State;
    Mode->DefaultDailyPayment = 123;
    Mode->DailyPaymentBaseMultiplier = 1.5;
    Mode->DailyPaymentMultiplierIncreasePerDay = 0.0;

    auto* PawnStateProperty = FindFProperty<FObjectPropertyBase>(APawn::StaticClass(), TEXT("PlayerState"));
    auto* ControllerStateProperty = FindFProperty<FObjectPropertyBase>(AController::StaticClass(), TEXT("PlayerState"));
    if (!TestNotNull(TEXT("Pawn player-state property"), PawnStateProperty)
        || !TestNotNull(TEXT("Controller player-state property"), ControllerStateProperty)) return false;

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
        if (!TestNotNull(TEXT("Player pawn"), Players[Index])
            || !TestNotNull(TEXT("Player state"), States[Index])
            || !TestNotNull(TEXT("Player controller"), PC)) return false;
        // CreateWorld without InitializeActorsForPlay skips controller PostInitializeComponents.
        World->AddController(PC);
        State->AddPlayerState(States[Index]);
        PawnStateProperty->SetObjectPropertyValue_InContainer(Players[Index], States[Index]);
        ControllerStateProperty->SetObjectPropertyValue_InContainer(PC, States[Index]);
        // Exercise the server's actual controller list without launching gameplay UI or abilities.
        PC->UIManager->PrepareTravel();
        PC->SetPawn(Players[Index]);
        auto* ASC = Players[Index]->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Players[Index], Players[Index]);
        ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
        auto* Point = World->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(),
            FVector(Index * 500.f, 1000.f, 100.f), FRotator::ZeroRotator, Spawn);
        Mode->PaymentSpawns.Add(Point);
    }
    int32 ControllerCount = 0;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get());
        if (!TestNotNull(TEXT("Registered casino controller"), PC)) return false;
        if (!TestNotNull(TEXT("Registered controller's pawn"), Cast<Acasino_simulatorCharacter>(PC->GetPawn()))
            || !TestNotNull(TEXT("Registered controller's player state"), PC->GetPlayerState<Acasino_simulatorPlayerState>())) return false;
        ++ControllerCount;
    }
    if (!TestEqual(TEXT("Fixture registers both player controllers"), ControllerCount, 2)) return false;
    auto SetWallet = [&](int32 Index, float Value)
    {
        Players[Index]->GetAbilitySystemComponent()->SetNumericAttributeBase(
            Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), Value);
    };
    SetWallet(0, 6000.f);
    SetWallet(1, 4000.f);
    if (!TestEqual(TEXT("First server wallet fixture"), Players[0]->GetCurrency(), 6000.f)
        || !TestEqual(TEXT("Second server wallet fixture"), Players[1]->GetCurrency(), 4000.f)) return false;

    TestEqual(TEXT("First day uses the fixed amount regardless of team wealth"), Mode->CalculateRequiredPayment(1), 123);
    TestEqual(TEXT("Second day includes both server player wallets at 150 percent"), Mode->CalculateRequiredPayment(2), 15000);
    TestEqual(TEXT("Zero daily increase keeps the fourth day at 150 percent"), Mode->CalculateRequiredPayment(4), 15000);
    Mode->DailyPaymentBaseMultiplier = 2.0;
    TestEqual(TEXT("Configured base multiplier changes the team target"), Mode->CalculateRequiredPayment(2), 20000);
    Mode->DailyPaymentBaseMultiplier = 1.5;
    States[1]->SetIsOnlyASpectator(true);
    TestEqual(TEXT("Spectator money is excluded"), Mode->CalculateRequiredPayment(2), 9000);
    States[1]->SetIsOnlyASpectator(false);
    States[1]->SetIsInactive(true);
    TestEqual(TEXT("Inactive player money is excluded"), Mode->CalculateRequiredPayment(2), 9000);
    States[1]->SetIsInactive(false);

    auto* UncontrolledPawn = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass,
        FVector(2000.f, 0.f, 100.f), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Uncontrolled character"), UncontrolledPawn)) return false;
    auto* UncontrolledASC = UncontrolledPawn->GetAbilitySystemComponent();
    UncontrolledASC->InitAbilityActorInfo(UncontrolledPawn, UncontrolledPawn);
    UncontrolledASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    UncontrolledASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 90000.f);
    TestEqual(TEXT("An unowned character cannot inflate the team target"), Mode->CalculateRequiredPayment(2), 15000);

    Mode->DailyPaymentMultiplierIncreasePerDay = 0.1;
    TestEqual(TEXT("Daily increase starts from day three"), Mode->CalculateRequiredPayment(2), 15000);
    TestEqual(TEXT("Third day uses 160 percent without rounding an extra currency unit"), Mode->CalculateRequiredPayment(3), 16000);
    TestEqual(TEXT("Fourth day uses 170 percent"), Mode->CalculateRequiredPayment(4), 17000);
    Mode->DailyPaymentMultiplierIncreasePerDay = 0.0;

    Mode->BeginCasinoDay(2);
    TestEqual(TEXT("Day introduction publishes the computed target"), State->LoopStatus.RequiredPayment, 15000);
    TestEqual(TEXT("Snapshot is published for the requested day"), State->LoopStatus.CurrentDay, 2);
    TestEqual(TEXT("Players move to a valid introduction phase"), State->LoopStatus.Phase, ECasinoLoopPhase::DayIntro);
    TestTrue(TEXT("First player's previous payment is charged"), Players[0]->TrySpendCurrency(500.f));
    TestTrue(TEXT("Second player's previous payment is charged"), Players[1]->TrySpendCurrency(1000.f));
    TestEqual(TEXT("Spending cannot change the current day's snapshot"), State->LoopStatus.RequiredPayment, 15000);
    TestEqual(TEXT("Following day's calculation uses the remaining team balance"), Mode->CalculateRequiredPayment(3), 12750);
    Mode->BeginCasinoDay(3);
    TestEqual(TEXT("Following introduction publishes the new remaining-balance target"), State->LoopStatus.RequiredPayment, 12750);

    SetWallet(0, 1000.25f);
    SetWallet(1, 0.f);
    TestEqual(TEXT("Fractional targets round upward to whole currency"), Mode->CalculateRequiredPayment(2), 1501);
    SetWallet(0, -100.f);
    TestEqual(TEXT("Nonpositive team balance keeps a nonzero target"), Mode->CalculateRequiredPayment(2), 1);
    SetWallet(0, 1.e20f);
    TestEqual(TEXT("Very large wallets cannot overflow the replicated integer target"), Mode->CalculateRequiredPayment(2), MAX_int32);

    Mode->DailyPayments = {137, 999, 9999};
    Mode->DefaultDailyPayment = 50;
    Mode->PostLoad();
    TestEqual(TEXT("Legacy first-day amount migrates without retaining later fixed targets"), Mode->DefaultDailyPayment, 137);
    TestTrue(TEXT("Legacy schedule is cleared after migration"), Mode->DailyPayments.IsEmpty());
    Mode->DefaultDailyPayment = 246;
    Mode->PostLoad();
    TestEqual(TEXT("Migration cannot override subsequent first-day edits"), Mode->DefaultDailyPayment, 246);
    return true;
}
#endif
