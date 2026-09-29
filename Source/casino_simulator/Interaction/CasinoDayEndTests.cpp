#if WITH_DEV_AUTOMATION_TESTS
#include "Interaction/CasinoDayParticipant.h"
#include "Blackjack/BlackjackTableActor.h"
#include "Blackjack/BlackjackPlayerComponent.h"
#include "ThreeCardPoker/ThreeCardPokerTableActor.h"
#include "UI/LadderMachine.h"
#include "NPC/NPC_Dice.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoDayEndTest, "Casino.DayLoop.ForfeitAndRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoDayEndTest::RunTest(const FString& Parameters)
{
    UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Character class"), CharacterClass)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
    auto* GS = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(GS);
    auto Status = GS->LoopStatus;
    Status.Phase = ECasinoLoopPhase::Playing;
    GS->SetLoopStatus(Status);
    auto* Table = World->SpawnActor<ABlackjackTableActor>();
    Table->DispatchBeginPlay();
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Acasino_simulatorCharacter* Players[2];
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Players[Index] = World->SpawnActor<Acasino_simulatorCharacter>(CharacterClass,
            FVector(Index * 200.f, 0.f, 200.f), FRotator::ZeroRotator, Spawn);
        auto* ASC = Players[Index]->GetAbilitySystemComponent();
        ASC->InitAbilityActorInfo(Players[Index], Players[Index]);
        ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
        ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 1000.f);
        TestTrue(TEXT("Claim seat"), Table->TryClaimSeat(Players[Index], Index));
        Players[Index]->GetBlackjackPlayerComponent()->EnterBlackjackSeatMode(Table, Index);
    }
    TestTrue(TEXT("Commit wager"), Table->PlaceBet(Players[0], 100));
    const float StakedBalance = Players[0]->GetCurrency();
    TestEqual(TEXT("Stake charged once"), StakedBalance, 900.f);
    Status.Phase = ECasinoLoopPhase::Settling;
    GS->SetLoopStatus(Status);
    ICasinoDayParticipant::Execute_EndCasinoDay(Table);
    ICasinoDayParticipant::Execute_EndCasinoDay(Table);
    TestFalse(TEXT("Betting closed"), Table->IsBettingWindowOpen());
    TestEqual(TEXT("Seat released"), Table->GetSeatIndexForPlayer(Players[0]), INDEX_NONE);
    TestFalse(TEXT("Player seat state cleared"), Players[0]->GetBlackjackPlayerComponent()->IsInBlackjackSeat());
    TestEqual(TEXT("No extra charge or refund"), Players[0]->GetCurrency(), StakedBalance);
    TestFalse(TEXT("No reseating during payment"), Table->TryClaimSeat(Players[0], 0));
    TestFalse(TEXT("No restart during payment"), Table->StartBettingWindow());
    for (int32 Step = 0; Step < 60; ++Step)
    {
        ++GFrameCounter;
        World->TimeSeconds += 1.0;
        World->GetTimerManager().Tick(1.f);
    }
    TestEqual(TEXT("No late payout"), Players[0]->GetCurrency(), StakedBalance);
    AActor* Others[] = {World->SpawnActor<AThreeCardPokerTableActor>(),
        World->SpawnActor<ALadderMachine>(), World->SpawnActor<ANPC_Dice>()};
    for (AActor* Other : Others)
    {
        TestTrue(TEXT("Game implements day lifecycle"), Other->GetClass()->ImplementsInterface(UCasinoDayParticipant::StaticClass()));
        ICasinoDayParticipant::Execute_EndCasinoDay(Other);
        ICasinoDayParticipant::Execute_EndCasinoDay(Other);
    }
    Status.Phase = ECasinoLoopPhase::Playing;
    GS->SetLoopStatus(Status);
    ICasinoDayParticipant::Execute_BeginCasinoDay(Table);
    TestTrue(TEXT("Next day betting restored"), Table->IsBettingWindowOpen());
    TestTrue(TEXT("Next day seat available"), Table->TryClaimSeat(Players[0], 0));
    auto* Dice = Cast<ANPC_Dice>(Others[2]);
    static_cast<IWorldInteractable*>(Dice)->Interact(Players[1]);
    TestTrue(TEXT("Dice wager accepted"), Dice->ExecutePlaceBet(Players[1], 0, 100));
    const float DiceBalance = Players[1]->GetCurrency();
    Status.Phase = ECasinoLoopPhase::Settling;
    GS->SetLoopStatus(Status);
    ICasinoDayParticipant::Execute_EndCasinoDay(Dice);
    TestFalse(TEXT("Late dice result ignored"), Dice->ShowResult(2));
    TestEqual(TEXT("Dice stake forfeited without extra charge"), Players[1]->GetCurrency(), DiceBalance);
    Status.Phase = ECasinoLoopPhase::Playing;
    GS->SetLoopStatus(Status);
    static_cast<IWorldInteractable*>(Dice)->Interact(Players[1]);
    TestTrue(TEXT("New day dice bet accepted"), Dice->ExecutePlaceBet(Players[1], 0, 100));
    TestTrue(TEXT("Normal dice win"), Dice->ShowResult(2));
    const float PaidBalance = Players[1]->GetCurrency();
    TestFalse(TEXT("Duplicate dice result ignored"), Dice->ShowResult(2));
    TestEqual(TEXT("No duplicate dice payout"), Players[1]->GetCurrency(), PaidBalance);
    return true;
}
#endif
