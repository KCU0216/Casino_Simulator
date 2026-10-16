#if WITH_DEV_AUTOMATION_TESTS
#include "UI/LadderMachine.h"
#include "Interaction/InteractionSessionComponent.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_loop_gamestate.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLadderMachineReleaseResetTest,
    "Casino.Ladder.ReleaseRestoresInitialState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLadderMachineReleaseResetTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);

    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
    if (!TestNotNull(TEXT("Concrete player class"), PlayerClass) ||
        !TestNotNull(TEXT("Concrete controller class"), ControllerClass)) return false;
    auto* Player = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass);
    auto* Controller = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass);
    auto* Machine = World->SpawnActor<ALadderMachine>();
    if (!TestNotNull(TEXT("State"), State) || !TestNotNull(TEXT("Player"), Player) ||
        !TestNotNull(TEXT("Controller"), Controller) || !TestNotNull(TEXT("Machine"), Machine)) return false;
    World->SetGameState(State);
    State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
    Controller->Possess(Player);
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 5000.f);

    Machine->Rails = 4;
    Machine->RowsPerRail = 3;
    Machine->DispatchBeginPlay();
    const TArray<int32> InitialOdds = Machine->Payouts;
    auto* Session = Machine->GetInteractionSessionComponent();
    auto AdvanceTimers = [&]
    {
        // Activate timers queued before this test world's first timer tick.
        ++GFrameCounter;
        World->GetTimerManager().Tick(0.f);
        ++GFrameCounter;
        World->TimeSeconds += Machine->TraceDuration + 1.f;
        World->GetTimerManager().Tick(Machine->TraceDuration + 1.f);
    };
    auto CheckReset = [&]
    {
        TestFalse(TEXT("Seat released"), Machine->IsOccupied());
        TestFalse(TEXT("Result closed"), Machine->bResultOpen);
        TestFalse(TEXT("Playback stopped"), Machine->bIsPlaying);
        TestTrue(TEXT("Previous drawing cleared"), Machine->CurrentPlan.PathRails.IsEmpty());
        TestEqual(TEXT("Instance startup rail count restored"), Machine->Rails, 4);
        TestEqual(TEXT("Instance startup rows restored"), Machine->Rows, 12);
        TestTrue(TEXT("Exact startup odds restored without reroll"), Machine->Payouts == InitialOdds);
        TestEqual(TEXT("First rail selected"), Machine->RailCursor, 0);
        const FLadderDisplayState Display = Machine->GetLadderState();
        TestEqual(TEXT("Replicated display rail count updated"), Display.Rails, 4);
        TestEqual(TEXT("Replicated display selection updated"), Display.SelectedRail, 0);
        TestTrue(TEXT("Replicated display odds updated"), Display.Odds == InitialOdds);
    };

    for (int32 Route = 0; Route < 2; ++Route)
    {
        if (!TestTrue(TEXT("Seat accepts returning player"), Session->TryJoin(Player))) return false;
        Machine->SetRailCount(5);
        Machine->SetRailCursor(2);
        Machine->SetBetAmount(275);
        Machine->Play();
        if (!TestTrue(TEXT("Round started"), Machine->bIsPlaying)) return false;
        AdvanceTimers();
        if (!TestTrue(TEXT("Settled result open before exit"), Machine->bResultOpen)) return false;
        const float SettledBalance = Player->GetCurrency();
        if (Route == 0) Controller->CloseCurrentInteraction();
        else Controller->ExitCurrentMachine();
        CheckReset();
        TestEqual(TEXT("Exit does not change wager"), Machine->BetAmount, 275);
        TestEqual(TEXT("Exit does not change settled balance"), Player->GetCurrency(), SettledBalance);
        Machine->ResetMachineToInitialState();
        CheckReset();
    }

    if (!TestTrue(TEXT("Seat accepts player before forced exit"), Session->TryJoin(Player))) return false;
    Machine->SetRailCount(5);
    Machine->SetRailCursor(2);
    Machine->Play();
    if (!TestTrue(TEXT("Pending round before forced exit"), Machine->bIsPlaying)) return false;
    const float StakedBalance = Player->GetCurrency();
    State->LoopStatus.Phase = ECasinoLoopPhase::Settling;
    Machine->EndCasinoDay_Implementation();
    CheckReset();
    AdvanceTimers();
    TestFalse(TEXT("No late result after forced exit"), Machine->bResultOpen);
    TestEqual(TEXT("No late payout after forced exit"), Player->GetCurrency(), StakedBalance);
    TestTrue(TEXT("Forced exit preserves payment movement lock"),
        Player->GetCharacterMovement()->MovementMode == MOVE_None);
    return true;
}
#endif
