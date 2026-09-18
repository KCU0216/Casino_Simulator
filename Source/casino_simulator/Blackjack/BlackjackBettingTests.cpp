// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blackjack/BlackjackTableActor.h"
#include "Blackjack/BlackjackTestObserver.h"
#include "Blackjack/BlackjackTableInteractionActor.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

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
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<uint64> FrameCounterGuard(GFrameCounter, GFrameCounter);
	// This isolated world does not initialize gameplay actors. Allow reflected
	// actor delegate callbacks (including OnDestroyed) without starting BP gameplay.
	TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
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
	ABlackjackTableInteractionActor* Chip = World->SpawnActor<ABlackjackTableInteractionActor>();
	Chip->AttachToActor(Table, FAttachmentTransformRules::KeepWorldTransform);
	Players[0]->GetBlackjackPlayerComponent()->EnterBlackjackSeatMode(Table, 0);
	Players[1]->GetBlackjackPlayerComponent()->EnterBlackjackSeatMode(Table, 1);
	FIntProperty* AllowedSeat = FindFProperty<FIntProperty>(Chip->GetClass(), TEXT("AllowedSeatIndex"));
	if (!TestNotNull(TEXT("Chip seat restriction property"), AllowedSeat)) { return false; }
	AllowedSeat->SetPropertyValue_InContainer(Chip, 0);
	TestTrue(TEXT("Own seat chip usable"), Chip->CanInteract(Players[0]));
	TestFalse(TEXT("Neighbor cannot use restricted chip"), Chip->CanInteract(Players[1]));
	Chip->Interact(Players[1]);
	TestEqual(TEXT("Rejected neighbor interaction does not bet"), Table->GetSeats()[1].BetAmount, 0);
	AllowedSeat->SetPropertyValue_InContainer(Chip, INDEX_NONE);
	TestTrue(TEXT("Shared chip allows another seated player"), Chip->CanInteract(Players[1]));
	Players[0]->GetBlackjackPlayerComponent()->CompleteExitBlackjackSeat();
	Players[1]->GetBlackjackPlayerComponent()->CompleteExitBlackjackSeat();
	TestTrue(TEXT("Reseat first player after chip test"), Table->TryClaimSeat(Players[0], 0));
	TestTrue(TEXT("Reseat second player after chip test"), Table->TryClaimSeat(Players[1], 1));
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
	TestFalse(TEXT("Availability rejects unaffordable bet"), Table->CanPlaceBet(Players[0], 100000000));
	TestFalse(TEXT("Rejected bets do not start countdown"), Table->IsBettingCountdownActive());

	const float BeforeBet = Players[0]->GetCurrency();
	TestTrue(TEXT("First bet accepted"), Table->PlaceBet(Players[0], 100));
	TestFalse(TEXT("Bettor cannot reopen keypad"), Table->CanOpenBetting(Players[0]));
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

	// Force a deterministic pair and an exact-capacity array so splitting must
	// reallocate. Mutate only this isolated test fixture, not production state APIs.
	FBlackjackSeatState& SplitSeat = const_cast<FBlackjackSeatState&>(Table->GetSeats()[0]);
	SplitSeat.Hands.SetNum(1);
	SplitSeat.Hands.Shrink();
	SplitSeat.Hands[0] = FBlackjackHand();
	SplitSeat.Hands[0].BetAmount = 100;
	SplitSeat.Hands[0].Cards.SetNum(2);
	SplitSeat.Hands[0].Cards[0].Rank = EBlackjackRank::Eight;
	SplitSeat.Hands[0].Cards[0].Suit = EBlackjackSuit::Clubs;
	SplitSeat.Hands[0].Cards[1].Rank = EBlackjackRank::Eight;
	SplitSeat.Hands[0].Cards[1].Suit = EBlackjackSuit::Hearts;
	SplitSeat.ActiveHandIndex = 0;
	SplitSeat.bHasSplitThisRound = false;
	FEnumProperty* RoundProperty = FindFProperty<FEnumProperty>(Table->GetClass(), TEXT("RoundState"));
	FIntProperty* ActiveProperty = FindFProperty<FIntProperty>(Table->GetClass(), TEXT("ActiveSeatIndex"));
	if (!TestNotNull(TEXT("Round state property"), RoundProperty)
		|| !TestNotNull(TEXT("Active seat property"), ActiveProperty))
	{
		return false;
	}
	RoundProperty->GetUnderlyingProperty()->SetIntPropertyValue(
		RoundProperty->ContainerPtrToValuePtr<void>(Table), static_cast<uint64>(EBlackjackRoundState::PlayerTurns));
	ActiveProperty->SetPropertyValue_InContainer(Table, 0);
	Players[0]->GetAbilitySystemComponent()->SetNumericAttributeBase(
		Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 50.0f);
	TestFalse(TEXT("Split rejects insufficient funds"), Table->PlayerSplit(Players[0]));
	TestEqual(TEXT("Rejected split preserves one hand"), SplitSeat.Hands.Num(), 1);
	Players[0]->GetAbilitySystemComponent()->SetNumericAttributeBase(
		Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 1000.0f);
	if (!TestTrue(TEXT("Split succeeds across array reallocation"), Table->PlayerSplit(Players[0])))
	{
		return false;
	}
	TestEqual(TEXT("Split creates two hands"), SplitSeat.Hands.Num(), 2);
	TestEqual(TEXT("First hand gets replacement card"), SplitSeat.Hands[0].Cards.Num(), 2);
	TestEqual(TEXT("Second hand gets replacement card"), SplitSeat.Hands[1].Cards.Num(), 2);
	TestTrue(TEXT("Original first card remains"), SplitSeat.Hands[0].Cards[0].Suit == EBlackjackSuit::Clubs);
	TestTrue(TEXT("Original second card moves"), SplitSeat.Hands[1].Cards[0].Suit == EBlackjackSuit::Hearts);
	TestEqual(TEXT("Split charges one additional bet"), Players[0]->GetCurrency(), 900.0f);
	TestFalse(TEXT("Repeated split rejected"), Table->PlayerSplit(Players[0]));
	TestEqual(TEXT("Repeated split does not charge"), Players[0]->GetCurrency(), 900.0f);

	FArrayProperty* ShoeProperty = FindFProperty<FArrayProperty>(Table->GetClass(), TEXT("Shoe"));
	if (!TestNotNull(TEXT("Shoe property"), ShoeProperty)) { return false; }
	auto MakeHand = [](std::initializer_list<EBlackjackRank> Ranks)
	{
		FBlackjackHand Hand;
		Hand.BetAmount = 100;
		for (EBlackjackRank Rank : Ranks)
		{
			FBlackjackCard Card;
			Card.Rank = Rank;
			Hand.Cards.Add(Card);
		}
		return Hand;
	};
	auto PrepareTurn = [&]()
	{
		SplitSeat.Hands.Reset();
		SplitSeat.ActiveHandIndex = 0;
		SplitSeat.bHasSplitThisRound = false;
		RoundProperty->GetUnderlyingProperty()->SetIntPropertyValue(
			RoundProperty->ContainerPtrToValuePtr<void>(Table), static_cast<uint64>(EBlackjackRoundState::PlayerTurns));
		ActiveProperty->SetPropertyValue_InContainer(Table, 0);
	};
	auto SetDraws = [&](std::initializer_list<EBlackjackRank> Ranks)
	{
		// DrawCard pops from the end: list the last draw first.
		*ShoeProperty->ContainerPtrToValuePtr<TArray<FBlackjackCard>>(Table) = MakeHand(Ranks).Cards;
	};
	PrepareTurn();
	SplitSeat.Hands.Add(MakeHand({EBlackjackRank::Ace, EBlackjackRank::Nine, EBlackjackRank::Ace}));
	TestEqual(TEXT("A 9 A totals 21"), Table->GetHandBestValue(SplitSeat.Hands[0]), 21);
	TestFalse(TEXT("HUD hit disabled at 21"), Table->CanHit(Players[0]));
	TestFalse(TEXT("HUD stand disabled on completed hand"), Table->CanStand(Players[0]));
	TestFalse(TEXT("Three-card 21 is not natural blackjack"), Table->IsNaturalBlackjack(SplitSeat.Hands[0]));
	TestFalse(TEXT("Already 21 rejects hit"), Table->PlayerHit(Players[0]));
	TestEqual(TEXT("Rejected hit leaves cards unchanged"), SplitSeat.Hands[0].Cards.Num(), 3);

	// Hit to 21 advances to the next occupied player's playable hand.
	FBlackjackSeatState& NextSeat = const_cast<FBlackjackSeatState&>(Table->GetSeats()[1]);
	NextSeat.Occupant = Players[1];
	NextSeat.BetAmount = 100;
	NextSeat.Hands = {MakeHand({EBlackjackRank::Ten, EBlackjackRank::Seven})};
	NextSeat.ActiveHandIndex = 0;
	PrepareTurn();
	SplitSeat.Hands.Add(MakeHand({EBlackjackRank::Ten, EBlackjackRank::Ten}));
	SetDraws({EBlackjackRank::Ace});
	TestTrue(TEXT("Hit 20 accepts one ace"), Table->PlayerHit(Players[0]));
	TestEqual(TEXT("Hit reaches 21"), Table->GetHandBestValue(SplitSeat.Hands[0]), 21);
	TestTrue(TEXT("Hit 21 stands automatically"), SplitSeat.Hands[0].bStood);
	TestTrue(TEXT("Hit 21 advances to next player"), Table->IsPlayerTurn(Players[1]));
	TestFalse(TEXT("Previous player cannot hit again"), Table->PlayerHit(Players[0]));

	PrepareTurn();
	SplitSeat.Hands.Add(MakeHand({EBlackjackRank::Ten, EBlackjackRank::Ten}));
	SplitSeat.Hands.Add(MakeHand({EBlackjackRank::Ten, EBlackjackRank::Seven}));
	SetDraws({EBlackjackRank::Ace});
	TestTrue(TEXT("First split hand hits 21"), Table->PlayerHit(Players[0]));
	TestEqual(TEXT("Continues with second hand"), SplitSeat.ActiveHandIndex, 1);
	TestTrue(TEXT("Second hand keeps same player's turn"), Table->IsPlayerTurn(Players[0]));

	PrepareTurn();
	SplitSeat.Hands.Add(MakeHand({EBlackjackRank::Ace, EBlackjackRank::Ace}));
	SetDraws({EBlackjackRank::Five, EBlackjackRank::King});
	TestTrue(TEXT("Split with first replacement making 21"), Table->PlayerSplit(Players[0]));
	TestEqual(TEXT("Split skips completed first hand"), SplitSeat.ActiveHandIndex, 1);
	TestFalse(TEXT("Split A K is not natural blackjack"), Table->IsNaturalBlackjack(SplitSeat.Hands[0]));

	PrepareTurn();
	SplitSeat.Hands.Add(MakeHand({EBlackjackRank::Ace, EBlackjackRank::Ace}));
	SetDraws({EBlackjackRank::King, EBlackjackRank::King});
	TestTrue(TEXT("Both split hands can reach 21"), Table->PlayerSplit(Players[0]));
	TestTrue(TEXT("Both completed hands advance to next player"), Table->IsPlayerTurn(Players[1]));

	NextSeat.Occupant = nullptr;
	NextSeat.BetAmount = 0;
	NextSeat.Hands.Reset();
	PrepareTurn();
	SplitSeat.Hands.Add(MakeHand({EBlackjackRank::Ten, EBlackjackRank::Ten}));
	SetDraws({EBlackjackRank::Ten, EBlackjackRank::Ten, EBlackjackRank::Ten, EBlackjackRank::Ace});
	TestTrue(TEXT("Last player hits to 21"), Table->PlayerHit(Players[0]));
	TestTrue(TEXT("Last hand 21 runs dealer and resolves"), Table->GetRoundState() == EBlackjackRoundState::RoundComplete);

	auto StartInsuranceRound = [&](float FirstBalance, float SecondBalance, EBlackjackRank HoleCard, int32 Bet = 100)
	{
		Table->ResetRound();
		// Earlier hand fixtures assign occupants directly. Restore real seat lifecycle
		// before testing insurance and destruction delegates.
		Table->LeaveSeat(Players[0]);
		Table->LeaveSeat(Players[1]);
		TestTrue(TEXT("First player seated for insurance test"), Table->TryClaimSeat(Players[0], 0));
		if (Table->GetSeatIndexForPlayer(Players[1]) == INDEX_NONE)
		{
			TestTrue(TEXT("Second player seated for insurance test"), Table->TryClaimSeat(Players[1], 1));
		}
		Players[0]->GetAbilitySystemComponent()->SetNumericAttributeBase(
			Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), FirstBalance);
		Players[1]->GetAbilitySystemComponent()->SetNumericAttributeBase(
			Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), SecondBalance);
		TArray<FBlackjackCard>& TestShoe = *ShoeProperty->ContainerPtrToValuePtr<TArray<FBlackjackCard>>(Table);
		TestShoe.Init(MakeHand({EBlackjackRank::Ten}).Cards[0], 40); // Above shuffle threshold.
		// Pop order: player 0, player 1, dealer ace, player 0, player 1, dealer hole.
		TestShoe.Append(MakeHand({HoleCard, EBlackjackRank::Seven, EBlackjackRank::Seven,
			EBlackjackRank::Ace, EBlackjackRank::Ten, EBlackjackRank::Ten}).Cards);
		TestTrue(TEXT("First insurance fixture bet accepted"), Table->PlaceBet(Players[0], Bet));
		TestTrue(TEXT("Second insurance fixture bet accepted"), Table->PlaceBet(Players[1], Bet));
	};
	StartInsuranceRound(100.0f, 130.0f, EBlackjackRank::Nine);
	TestEqual(TEXT("All-in player has zero balance"), Players[0]->GetCurrency(), 0.0f);
	TestTrue(TEXT("All-in player automatically declines insurance"), SplitSeat.bInsuranceDecisionMade);
	TestEqual(TEXT("Auto-decline does not place insurance"), SplitSeat.InsuranceBetAmount, 0);
	TestFalse(TEXT("Player below half-bet but able to insure still chooses"), NextSeat.bInsuranceDecisionMade);
	TestTrue(TEXT("Waits for funded player's decision"), Table->GetRoundState() == EBlackjackRoundState::Insurance);
	TestTrue(TEXT("Funded player can buy partial insurance"), Table->PlaceInsurance(Players[1], 30));
	TestTrue(TEXT("Decision completes insurance phase"), Table->GetRoundState() == EBlackjackRoundState::PlayerTurns);

	StartInsuranceRound(100.0f, 100.0f, EBlackjackRank::Nine);
	TestTrue(TEXT("Both all-in players automatically decline"), SplitSeat.bInsuranceDecisionMade && NextSeat.bInsuranceDecisionMade);
	TestTrue(TEXT("Everyone all-in skips waiting and starts player turns"), Table->GetRoundState() == EBlackjackRoundState::PlayerTurns);
	TestEqual(TEXT("Auto-decline leaves first balance unchanged"), Players[0]->GetCurrency(), 0.0f);
	TestEqual(TEXT("Auto-decline leaves second balance unchanged"), Players[1]->GetCurrency(), 0.0f);

	StartInsuranceRound(100.0f, 100.0f, EBlackjackRank::King);
	TestTrue(TEXT("Everyone all-in with dealer blackjack resolves immediately"), Table->GetRoundState() == EBlackjackRoundState::RoundComplete);

	StartInsuranceRound(130.0f, 130.0f, EBlackjackRank::Nine);
	TestFalse(TEXT("Funded first player is not auto-declined"), SplitSeat.bInsuranceDecisionMade);
	TestFalse(TEXT("Funded second player is not auto-declined"), NextSeat.bInsuranceDecisionMade);
	TestTrue(TEXT("Funded players retain insurance choice"), Table->GetRoundState() == EBlackjackRoundState::Insurance);
	TestTrue(TEXT("HUD allows affordable partial insurance"), Table->CanPlaceInsurance(Players[0], 30));
	TestFalse(TEXT("HUD rejects insurance above balance"), Table->CanPlaceInsurance(Players[0], 50));
	TestFalse(TEXT("HUD disables hit during insurance"), Table->CanHit(Players[0]));
	TestTrue(TEXT("First funded player can skip"), Table->SkipInsurance(Players[0]));
	TestTrue(TEXT("Still waits for second decision"), Table->GetRoundState() == EBlackjackRoundState::Insurance);
	TestTrue(TEXT("Second funded player can skip"), Table->SkipInsurance(Players[1]));
	TestTrue(TEXT("All skips resume play"), Table->GetRoundState() == EBlackjackRoundState::PlayerTurns);

	StartInsuranceRound(100.5f, 100.0f, EBlackjackRank::Nine);
	TestTrue(TEXT("Fractional balance below one unit cannot insure"), SplitSeat.bInsuranceDecisionMade);
	StartInsuranceRound(10.0f, 10.0f, EBlackjackRank::Nine, 1);
	TestTrue(TEXT("Zero integer insurance limit skips selection"), Table->GetRoundState() == EBlackjackRoundState::PlayerTurns);

	StartInsuranceRound(200.0f, 200.0f, EBlackjackRank::Nine);
	TestTrue(TEXT("Insurance can be declined"), Table->SkipInsurance(Players[0]));
	const float DeclinedBalance = Players[0]->GetCurrency();
	TestFalse(TEXT("Declined insurance cannot be purchased later"), Table->PlaceInsurance(Players[0], 50));
	TestEqual(TEXT("Rejected insurance re-entry preserves funds"), Players[0]->GetCurrency(), DeclinedBalance);

	ABlackjackTableActor* OtherTable = World->SpawnActor<ABlackjackTableActor>();
	TestFalse(TEXT("Cannot claim another table while seated"), OtherTable->TryClaimSeat(Players[0], 0));
	UBlackjackPlayerComponent* Component = Players[0]->GetBlackjackPlayerComponent();
	UBlackjackTestObserver* Observer = NewObject<UBlackjackTestObserver>();
	Component->OnBlackjackSeatModeStarted.AddDynamic(Observer, &UBlackjackTestObserver::Started);
	Component->OnBlackjackSeatModeEnded.AddDynamic(Observer, &UBlackjackTestObserver::Ended);
	Component->OnActionCompleted.AddDynamic(Observer, &UBlackjackTestObserver::Completed);
	Component->EnterBlackjackSeatMode(Table, 0);
	Component->EnterBlackjackSeatMode(Table, 0);
	TestEqual(TEXT("Repeated enter notifies once"), Observer->Starts, 1);
	Component->EnterBlackjackSeatMode(OtherTable, 0);
	TestTrue(TEXT("Unclaimed seat cannot overwrite component"), Component->GetCurrentBlackjackTable() == Table);
	TestFalse(TEXT("Component rejects hit during insurance"), Component->Hit());
	TestEqual(TEXT("Server failure publishes one outcome"), Observer->Results, 1);
	TestTrue(TEXT("Outcome is rejected"), Observer->LastResult == EBlackjackRequestResult::Rejected);
	TestTrue(TEXT("Second player declines to resume play"), Table->SkipInsurance(Players[1]));
	TestTrue(TEXT("Component stand succeeds"), Component->Stand());
	TestEqual(TEXT("Success publishes one more outcome"), Observer->Results, 2);
	TestTrue(TEXT("Success outcome"), Observer->LastResult == EBlackjackRequestResult::Success);

	// Exercise the exact RepNotify path with a received seat snapshot, including
	// duplicate notifications. This does not substitute for multi-process PIE.
	FStructProperty* SessionProperty = FindFProperty<FStructProperty>(Component->GetClass(), TEXT("SeatSession"));
	UFunction* SeatNotify = Component->FindFunction(TEXT("OnRep_BlackjackSeatMode"));
	if (!TestNotNull(TEXT("Seat snapshot property"), SessionProperty) || !TestNotNull(TEXT("Seat notify"), SeatNotify)) { return false; }
	FBlackjackSeatSession* Session = SessionProperty->ContainerPtrToValuePtr<FBlackjackSeatSession>(Component);
	*Session = FBlackjackSeatSession();
	Component->ProcessEvent(SeatNotify, nullptr);
	Component->ProcessEvent(SeatNotify, nullptr);
	TestEqual(TEXT("RepNotify seat end emitted once"), Observer->Ends, 1);
	Session->Table = Table;
	Session->SeatIndex = 0;
	Component->ProcessEvent(SeatNotify, nullptr);
	Component->ProcessEvent(SeatNotify, nullptr);
	TestEqual(TEXT("RepNotify seat start emitted once"), Observer->Starts, 2);

	// Destroyed occupants must not hold an insurance decision or player turn.
	StartInsuranceRound(200.0f, 200.0f, EBlackjackRank::Nine);
	TestTrue(TEXT("First player chooses before other disconnects"), Table->SkipInsurance(Players[0]));
	Players[1]->Destroy();
	TestFalse(TEXT("Disconnected insurance seat cleared"), Table->GetSeats()[1].IsOccupied());
	TestTrue(TEXT("Insurance disconnect resumes player turn"), Table->IsPlayerTurn(Players[0]));
	Players[0]->Destroy();
	TestFalse(TEXT("Disconnected active seat cleared"), Table->GetSeats()[0].IsOccupied());
	TestTrue(TEXT("Last active player disconnect completes round"), Table->GetRoundState() == EBlackjackRoundState::RoundComplete);

	Table->ResetRound();
	TestTrue(TEXT("Remaining player seated"), Table->TryClaimSeat(Players[2], 0));
	// Keep an occupied non-betting seat so the first bet starts a countdown.
	Acasino_simulatorCharacter* WaitingPlayer = World->SpawnActor<Acasino_simulatorCharacter>(CharacterClass,
		FVector(1000, 0, 200), FRotator::ZeroRotator, SpawnParameters);
	TestTrue(TEXT("Waiting player seated"), Table->TryClaimSeat(WaitingPlayer, 1));
	TestTrue(TEXT("Countdown bet accepted"), Table->PlaceBet(Players[2], 100));
	TestTrue(TEXT("Countdown is active"), Table->IsBettingCountdownActive());
	Players[2]->Destroy();
	TestFalse(TEXT("Last bettor disconnect cancels countdown"), Table->IsBettingCountdownActive());
	TestEqual(TEXT("Betting returns to unlimited wait"), Table->GetBettingRemainingTime(), -1.0f);
	return true;
}

#endif
