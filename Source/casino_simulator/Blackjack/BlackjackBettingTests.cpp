// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blackjack/BlackjackTableActor.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackBettingTest,
	"Casino.Blackjack.FirstBetCountdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackBettingTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("Playable character class"), CharacterClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	TGuardValue<uint64> FrameCounterGuard(GFrameCounter, GFrameCounter);
	ABlackjackTableActor* Table = World->SpawnActor<ABlackjackTableActor>();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Acasino_simulatorCharacter* Players[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Players[Index] = World->SpawnActor<Acasino_simulatorCharacter>(CharacterClass,
			FVector(Index * 200.0f, 0.0f, 200.0f), FRotator::ZeroRotator, SpawnParameters);
		if (!TestNotNull(TEXT("Spawned player"), Players[Index]))
		{
			return false;
		}
		Players[Index]->GetAbilitySystemComponent()->InitAbilityActorInfo(Players[Index], Players[Index]);
		Players[Index]->GetAbilitySystemComponent()->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
		Players[Index]->GetAbilitySystemComponent()->SetNumericAttributeBase(
			Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 10000.0f);
	}
	if (!TestNotNull(TEXT("Spawned table"), Table))
	{
		return false;
	}
	Table->DispatchBeginPlay();
	auto Advance = [World](int32 Seconds)
	{
		for (int32 Second = 0; Second < Seconds; ++Second)
		{
			// Advance the isolated world's clock and real timer manager without ticking
			// editor/physics systems or the player's presentation blueprint.
			++GFrameCounter;
			World->TimeSeconds += 1.0;
			World->GetTimerManager().Tick(1.0f);
		}
	};

	TestTrue(TEXT("Table opens betting at startup"), Table->IsBettingWindowOpen());
	Table->TryClaimSeat(Players[0], 0);
	Table->TryClaimSeat(Players[1], 1);
	Table->TryClaimSeat(Players[2], 2);
	Table->StartBettingWindow(10.0f); // Old BP argument must not impose a deadline.
	Advance(30);
	TestTrue(TEXT("No bets: still open after 30 seconds"), Table->IsBettingWindowOpen());
	TestFalse(TEXT("No bets: no countdown"), Table->IsBettingCountdownActive());
	TestEqual(TEXT("Unlimited time sentinel"), Table->GetBettingRemainingTime(), -1.0f);
	TestTrue(TEXT("Keypad is accepted without extending time"), Table->NotifyBettingInteractionStarted(Players[0]));
	Table->FinishBettingWindow();
	TestTrue(TEXT("No-bet finish cannot close reception"), Table->IsBettingWindowOpen());
	TestFalse(TEXT("Cannot start an empty round"), Table->StartRound());
	TestFalse(TEXT("Invalid bet"), Table->PlaceBet(Players[0], 0));
	TestFalse(TEXT("Insufficient funds"), Table->PlaceBet(Players[0], 100000000));
	TestFalse(TEXT("Rejected bets do not start countdown"), Table->IsBettingCountdownActive());

	const float BeforeBet = Players[0]->GetCurrency();
	TestTrue(TEXT("First bet accepted"), Table->PlaceBet(Players[0], 100));
	TestEqual(TEXT("First bet starts exactly 15 seconds"), Table->GetBettingRemainingTime(), 15.0f);
	TestEqual(TEXT("First bet charged once"), Players[0]->GetCurrency(), BeforeBet - 100.0f);
	TestFalse(TEXT("Duplicate bet rejected"), Table->PlaceBet(Players[0], 100));
	TestEqual(TEXT("Duplicate bet does not charge"), Players[0]->GetCurrency(), BeforeBet - 100.0f);
	TestFalse(TEXT("Manual start cannot skip other players' time"), Table->StartRound());
	Advance(10);
	TestTrue(TEXT("Second bet accepted"), Table->PlaceBet(Players[1], 100));
	TestTrue(TEXT("Keypad during countdown accepted"), Table->NotifyBettingInteractionStarted(Players[2]));
	TestFalse(TEXT("Legacy extension refused"), Table->ExtendBettingWindow(60.0f));
	Table->StartBettingWindow(10.0f);
	TestEqual(TEXT("Later bets/keypad/reopen do not extend deadline"), Table->GetBettingRemainingTime(), 5.0f);
	Table->FinishBettingWindow();
	TestTrue(TEXT("Premature finish keeps betting open"), Table->IsBettingWindowOpen());
	TestTrue(TEXT("Last player bets"), Table->PlaceBet(Players[2], 100));
	TestFalse(TEXT("All players bet: immediate start"), Table->IsBettingWindowOpen());
	TestFalse(TEXT("Bet during round rejected"), Table->PlaceBet(Players[2], 100));

	Table->ResetRound();
	TestTrue(TEXT("Reset reopens betting"), Table->IsBettingWindowOpen());
	TestFalse(TEXT("Reset removes deadline"), Table->IsBettingCountdownActive());
	Advance(30);
	TestTrue(TEXT("Next round also waits indefinitely"), Table->IsBettingWindowOpen());
	Table->PlaceBet(Players[0], 100);
	Table->ToggleSitOut(Players[1]);
	Table->ToggleSitOut(Players[2]);
	TestTrue(TEXT("Sit-out does not count as everyone betting"), Table->IsBettingWindowOpen());
	Advance(14);
	TestTrue(TEXT("Still open before deadline"), Table->IsBettingWindowOpen());
	Advance(2);
	TestFalse(TEXT("Timer starts round at deadline"), Table->IsBettingWindowOpen());
	TestTrue(TEXT("Bettor receives a hand"), !Table->GetSeats()[0].Hands.IsEmpty());
	TestTrue(TEXT("Non-bettor 1 receives no hand"), Table->GetSeats()[1].Hands.IsEmpty());
	TestTrue(TEXT("Non-bettor 2 receives no hand"), Table->GetSeats()[2].Hands.IsEmpty());

	Table->ResetRound();
	Table->LeaveSeat(Players[1]);
	Table->LeaveSeat(Players[2]);
	TestTrue(TEXT("Solo bet accepted"), Table->PlaceBet(Players[0], 100));
	TestFalse(TEXT("Solo bettor starts immediately"), Table->IsBettingWindowOpen());
	return true;
}

#endif
