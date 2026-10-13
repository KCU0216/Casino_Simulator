// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blackjack/BlackjackTableActor.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackDealerPresentationTest,
	"Casino.Blackjack.DealerPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackDealerPresentationTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
		TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("Playable character class"), CharacterClass)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<uint64> FrameCounterGuard(GFrameCounter, GFrameCounter);
	TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
	ABlackjackTableActor* Table = World->SpawnActor<ABlackjackTableActor>();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Acasino_simulatorCharacter* Player = World->SpawnActor<Acasino_simulatorCharacter>(CharacterClass,
		FVector(0, 0, 200), FRotator::ZeroRotator, SpawnParameters);
	if (!TestNotNull(TEXT("Table"), Table) || !TestNotNull(TEXT("Player"), Player)) return false;
	Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
	Player->GetAbilitySystemComponent()->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
	Player->GetAbilitySystemComponent()->SetNumericAttributeBase(
		Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 1000.0f);
	Table->DispatchBeginPlay();
	TestFalse(TEXT("Native tables retain synchronous defaults"), Table->IsDealerPresentationBusy());
	TestFalse(TEXT("Shuffle gestures default to disabled independently of dealing"), Table->IsDealerShuffleAnimationEnabled());
	FBoolProperty* Animated = FindFProperty<FBoolProperty>(Table->GetClass(), TEXT("bAnimateDealer"));
	FBoolProperty* AnimateShuffle = FindFProperty<FBoolProperty>(Table->GetClass(), TEXT("bAnimateDealerShuffle"));
	FFloatProperty* DealInterval = FindFProperty<FFloatProperty>(Table->GetClass(), TEXT("DealerCardIntervalSeconds"));
	FFloatProperty* ShuffleInterval = FindFProperty<FFloatProperty>(Table->GetClass(), TEXT("DealerShuffleSeconds"));
	FArrayProperty* ShoeProperty = FindFProperty<FArrayProperty>(Table->GetClass(), TEXT("Shoe"));
	FEnumProperty* RoundProperty = FindFProperty<FEnumProperty>(Table->GetClass(), TEXT("RoundState"));
	FIntProperty* ActiveProperty = FindFProperty<FIntProperty>(Table->GetClass(), TEXT("ActiveSeatIndex"));
	if (!TestNotNull(TEXT("Animated dealer setting"), Animated)
		|| !TestNotNull(TEXT("Optional shuffle setting"), AnimateShuffle)
		|| !TestNotNull(TEXT("Deal interval setting"), DealInterval)
		|| !TestNotNull(TEXT("Shuffle interval setting"), ShuffleInterval)
		|| !TestNotNull(TEXT("Shoe"), ShoeProperty)
		|| !TestNotNull(TEXT("Round state"), RoundProperty)
		|| !TestNotNull(TEXT("Active seat"), ActiveProperty)) return false;
	Animated->SetPropertyValue_InContainer(Table, true);
	DealInterval->SetPropertyValue_InContainer(Table, 0.5f);
	ShuffleInterval->SetPropertyValue_InContainer(Table, 0.5f);
	auto Advance = [World](float Seconds)
	{
		// Exercise the actual server TimerManager without animation or a game viewport.
		while (Seconds > 0.0f)
		{
			const float Step = FMath::Min(0.01f, Seconds);
			++GFrameCounter;
			World->TimeSeconds += Step;
			World->GetTimerManager().Tick(Step);
			Seconds -= Step;
		}
	};
	auto SetDraws = [&](std::initializer_list<EBlackjackRank> DrawOrder)
	{
		TArray<FBlackjackCard>& Shoe = *ShoeProperty->ContainerPtrToValuePtr<TArray<FBlackjackCard>>(Table);
		Shoe.Init(FBlackjackCard(EBlackjackSuit::Clubs, EBlackjackRank::Ten), 40);
		TArray<EBlackjackRank> Ranks(DrawOrder);
		for (int32 Index = Ranks.Num() - 1; Index >= 0; --Index)
			Shoe.Add(FBlackjackCard(EBlackjackSuit::Hearts, Ranks[Index]));
	};
	auto StartOrdinaryRound = [&]()
	{
		SetDraws({EBlackjackRank::Ten, EBlackjackRank::Six, EBlackjackRank::Seven, EBlackjackRank::Nine});
		TestTrue(TEXT("Timed wager accepted"), Table->PlaceBet(Player, 100));
		Advance(3.0f);
		TestTrue(TEXT("Initial gestures finish before player turn"), Table->GetRoundState() == EBlackjackRoundState::PlayerTurns);
		TestFalse(TEXT("Initial queue unlocks player actions"), Table->IsDealerPresentationBusy());
	};
	TestTrue(TEXT("Player seated with default shuffle disabled"), Table->TryClaimSeat(Player, 0));
	SetDraws({EBlackjackRank::Ten, EBlackjackRank::Six, EBlackjackRank::Seven, EBlackjackRank::Nine});
	TestTrue(TEXT("Animated deal starts with shuffle disabled"), Table->PlaceBet(Player, 100));
	TestTrue(TEXT("Disabling shuffle preserves paced card dealing"), Table->IsDealerPresentationBusy());
	TestEqual(TEXT("Disabled shuffle deals first card immediately without shuffle wait"), Table->GetSeats()[0].Hands[0].Cards.Num(), 1);
	TestEqual(TEXT("Disabled shuffle still spaces subsequent cards"), Table->GetDealerHand().Cards.Num(), 0);
	Table->ResetRound();
	Advance(10.0f);
	TestTrue(TEXT("Disabled-shuffle reset cancels pending card queue"), Table->GetSeats()[0].Hands.IsEmpty() && Table->GetDealerHand().Cards.IsEmpty());
	Table->LeaveSeat(Player);
	Table->Destroy();
	// A fresh table has a fresh randomized shoe and can explicitly opt into shuffle gestures.
	Table = World->SpawnActor<ABlackjackTableActor>();
	if (!TestNotNull(TEXT("Optional-shuffle test table"), Table)) return false;
	Table->DispatchBeginPlay();
	Animated->SetPropertyValue_InContainer(Table, true);
	AnimateShuffle->SetPropertyValue_InContainer(Table, true);
	DealInterval->SetPropertyValue_InContainer(Table, 0.5f);
	ShuffleInterval->SetPropertyValue_InContainer(Table, 0.5f);
	TestTrue(TEXT("Player seated"), Table->TryClaimSeat(Player, 0));
	SetDraws({EBlackjackRank::Ten, EBlackjackRank::Six, EBlackjackRank::Seven, EBlackjackRank::Nine});
	TestTrue(TEXT("First timed wager accepted"), Table->PlaceBet(Player, 100));
	TestTrue(TEXT("Initial shuffle remains in dealing"), Table->GetRoundState() == EBlackjackRoundState::Dealing);
	TestTrue(TEXT("Initial presentation is busy"), Table->IsDealerPresentationBusy());
	TestEqual(TEXT("Shuffle runs before first card"), Table->GetSeats()[0].Hands[0].Cards.Num(), 0);
	Advance(0.51f);
	TestEqual(TEXT("First gesture deals just one player card"), Table->GetSeats()[0].Hands[0].Cards.Num(), 1);
	TestEqual(TEXT("First gesture has not dealt dealer"), Table->GetDealerHand().Cards.Num(), 0);
	Advance(1.53f);
	TestEqual(TEXT("Initial player hand complete"), Table->GetSeats()[0].Hands[0].Cards.Num(), 2);
	TestEqual(TEXT("Initial dealer hand complete"), Table->GetDealerHand().Cards.Num(), 2);
	TestTrue(TEXT("Last card gesture still blocks actions"), Table->GetRoundState() == EBlackjackRoundState::Dealing);
	TestFalse(TEXT("Hit is blocked until last release interval"), Table->PlayerHit(Player));
	TestFalse(TEXT("Hole card face remains hidden"), Table->GetDealerHand().Cards[1].bFaceUp);
	TestTrue(TEXT("Public hole rank is redacted"), Table->GetDealerHand().Cards[1].Rank == EBlackjackRank::None);
	TestTrue(TEXT("Public hole suit is redacted"), Table->GetDealerHand().Cards[1].Suit == EBlackjackSuit::Clubs);
	Advance(0.51f);
	TestTrue(TEXT("Player turn begins after final gesture"), Table->IsPlayerTurn(Player));
	SetDraws({EBlackjackRank::Two, EBlackjackRank::Ten});
	TestTrue(TEXT("Timed hit accepted"), Table->PlayerHit(Player));
	TestTrue(TEXT("Hit gesture blocks another action"), Table->IsDealerPresentationBusy());
	TestFalse(TEXT("Duplicate hit while dealing rejected"), Table->PlayerHit(Player));
	TestFalse(TEXT("Stand while dealing rejected"), Table->PlayerStand(Player));
	Advance(0.51f);
	TestTrue(TEXT("Hit below 21 restores same turn"), Table->IsPlayerTurn(Player));
	TestTrue(TEXT("Stand accepted after hit gesture"), Table->PlayerStand(Player));
	TestTrue(TEXT("Dealer reveal keeps round unfinished"), Table->GetRoundState() == EBlackjackRoundState::DealerTurn);
	TestTrue(TEXT("Reveal exposes actual hole after player decisions"), Table->GetDealerHand().Cards[1].Rank == EBlackjackRank::Nine);
	TestEqual(TEXT("Dealer hit does not run in reveal frame"), Table->GetDealerHand().Cards.Num(), 2);
	const float BeforeSettlement = Player->GetCurrency();
	Advance(0.51f);
	TestEqual(TEXT("Dealer draws one timed hit"), Table->GetDealerHand().Cards.Num(), 3);
	TestTrue(TEXT("Settlement waits for final hit release"), Table->GetRoundState() == EBlackjackRoundState::DealerTurn);
	TestEqual(TEXT("No payout before final gesture"), Player->GetCurrency(), BeforeSettlement);
	Advance(0.51f);
	TestTrue(TEXT("Server timers resolve without AnimNotify"), Table->GetRoundState() == EBlackjackRoundState::RoundComplete);
	TestEqual(TEXT("Dealer bust pays once"), Player->GetCurrency(), BeforeSettlement + 200.0f);
	Advance(3.0f);
	TestEqual(TEXT("Extra timer ticks do not pay twice"), Player->GetCurrency(), BeforeSettlement + 200.0f);

	Table->ResetRound();
	StartOrdinaryRound();
	FBlackjackSeatState& Seat = const_cast<FBlackjackSeatState&>(Table->GetSeats()[0]);
	Seat.Hands[0].Cards = {FBlackjackCard(EBlackjackSuit::Clubs, EBlackjackRank::Eight),
		FBlackjackCard(EBlackjackSuit::Hearts, EBlackjackRank::Eight)};
	SetDraws({EBlackjackRank::Two, EBlackjackRank::Three});
	TestTrue(TEXT("Timed split accepted"), Table->PlayerSplit(Player));
	TestEqual(TEXT("First split replacement arrives immediately"), Seat.Hands[0].Cards.Num(), 2);
	TestEqual(TEXT("Second split replacement waits its gesture"), Seat.Hands[1].Cards.Num(), 1);
	TestFalse(TEXT("Split gesture blocks hit"), Table->PlayerHit(Player));
	Advance(0.51f);
	TestEqual(TEXT("Second split replacement arrives next"), Seat.Hands[1].Cards.Num(), 2);
	TestTrue(TEXT("Second split release still busy"), Table->IsDealerPresentationBusy());
	Advance(0.51f);
	TestTrue(TEXT("Split release restores first hand turn"), Table->IsPlayerTurn(Player));
	TestEqual(TEXT("First split hand remains active"), Seat.ActiveHandIndex, 0);

	Table->ResetRound();
	SetDraws({EBlackjackRank::Ten, EBlackjackRank::Six, EBlackjackRank::Seven, EBlackjackRank::Nine});
	Table->PlaceBet(Player, 100);
	TestTrue(TEXT("Reset fixture has pending initial cards"), Table->IsDealerPresentationBusy());
	Table->ResetRound();
	Advance(10.0f);
	TestFalse(TEXT("Reset cancels busy state"), Table->IsDealerPresentationBusy());
	TestTrue(TEXT("Reset removes stale player hands"), Table->GetSeats()[0].Hands.IsEmpty());
	TestTrue(TEXT("Reset timer cannot restore dealer cards"), Table->GetDealerHand().Cards.IsEmpty());
	TestTrue(TEXT("Reset remains in new betting phase"), Table->GetRoundState() == EBlackjackRoundState::Betting);

	StartOrdinaryRound();
	SetDraws({EBlackjackRank::Two});
	Table->PlayerHit(Player);
	TestTrue(TEXT("Day end fixture has a pending hit"), Table->IsDealerPresentationBusy());
	Table->EndCasinoDay_Implementation();
	const float DayEndBalance = Player->GetCurrency();
	Advance(10.0f);
	TestFalse(TEXT("Day end cancels busy state"), Table->IsDealerPresentationBusy());
	TestTrue(TEXT("Day end keeps seats cleared"), !Table->GetSeats()[0].IsOccupied());
	TestTrue(TEXT("Day end keeps cards cleared"), Table->GetDealerHand().Cards.IsEmpty());
	TestEqual(TEXT("Cancelled hit cannot settle after day end"), Player->GetCurrency(), DayEndBalance);

	for (const TCHAR* FunctionName : {TEXT("Multicast_PlayerHandCardDealt"), TEXT("Multicast_DealerCardDealt"),
		TEXT("Multicast_DealerHoleCardRevealed"), TEXT("Multicast_PlayerHandsSplit"), TEXT("Multicast_DealerShuffle"),
		TEXT("Multicast_DealerPresentationReset")})
	{
		UFunction* Event = Table->FindFunction(FunctionName);
		if (TestNotNull(FunctionName, Event))
			TestTrue(FString::Printf(TEXT("%s is reliable multicast"), FunctionName),
				Event->HasAllFunctionFlags(FUNC_Net | FUNC_NetMulticast | FUNC_NetReliable));
	}
	return true;
}

#endif
