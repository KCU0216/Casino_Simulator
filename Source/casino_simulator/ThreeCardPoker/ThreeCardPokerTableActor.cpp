// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreeCardPoker/ThreeCardPokerTableActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"

AThreeCardPokerTableActor::AThreeCardPokerTableActor()
{
	bReplicates = true;
	SetReplicateMovement(false);

	TableRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TableRoot"));
	SetRootComponent(TableRoot);

	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(TableRoot);
	TableMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TableMesh->SetCollisionProfileName(TEXT("BlockAll"));

	DeckPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DeckPoint"));
	DeckPoint->SetupAttachment(TableRoot);
	DeckPoint->SetRelativeLocation(FVector(30.0f, 95.0f, 8.0f));
}

void AThreeCardPokerTableActor::BeginPlay()
{
	Super::BeginPlay();

	// Captured before any interaction can retarget Owner, so SetInteractingPlayer(nullptr) has
	// something to restore (same as ANPC_Dice::DefaultOwner).
	DefaultOwner = GetOwner();

	if (HasAuthority() && Deck.IsEmpty())
	{
		BuildAndShuffleDeck();
	}
}

void AThreeCardPokerTableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AThreeCardPokerTableActor, RoundState);
	DOREPLIFETIME(AThreeCardPokerTableActor, PlayerCards);
	DOREPLIFETIME(AThreeCardPokerTableActor, DealerHand);
	DOREPLIFETIME(AThreeCardPokerTableActor, AnteBet);
	DOREPLIFETIME(AThreeCardPokerTableActor, PlayBet);
	DOREPLIFETIME(AThreeCardPokerTableActor, PairPlusBet);
	DOREPLIFETIME(AThreeCardPokerTableActor, bFolded);
	DOREPLIFETIME(AThreeCardPokerTableActor, bDecisionMade);
	DOREPLIFETIME(AThreeCardPokerTableActor, LastResult);
	DOREPLIFETIME(AThreeCardPokerTableActor, bDecisionWindowOpen);
	DOREPLIFETIME(AThreeCardPokerTableActor, DecisionWindowEndsAtServerTime);
}

void AThreeCardPokerTableActor::SetInteractingPlayer(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority())
	{
		return;
	}

	if (InteractingPlayer.Get() != Player)
	{
		// Switching players (or clearing) mid-round would leave stale bets/cards behind otherwise.
		ResetRound();
	}

	InteractingPlayer = Player;

	// Owning the table while a specific player is using it gives ROLE_AutonomousProxy to that
	// player's client only (same trick as ANPC_Dice::SetInteractingPlayer). Reverts to
	// DefaultOwner once nobody's interacting.
	SetOwner(Player ? static_cast<AActor*>(Player) : DefaultOwner.Get());
}

bool AThreeCardPokerTableActor::PlaceAnte(Acasino_simulatorCharacter* Player, int32 Amount)
{
	if (!Player || Amount < MinAnteBet)
	{
		return false;
	}

	if (HasAuthority())
	{
		return ExecutePlaceAnte(Player, Amount);
	}

	// This table isn't owned by any player's connection, so a Server RPC declared on it would just
	// be dropped if called from a client. Route through Player's own Character instead, which IS
	// owned by the calling client's connection (same forwarding trick as ANPC_Dice::PlaceBet).
	Player->ServerPlaceThreeCardPokerAnte(this, Amount);
	return true;
}

bool AThreeCardPokerTableActor::ExecutePlaceAnte(Acasino_simulatorCharacter* Player, int32 Amount)
{
	if (!HasAuthority() || !Player || Amount < MinAnteBet)
	{
		return false;
	}

	// Only the player currently interacting with this table may place a bet on it.
	if (InteractingPlayer.Get() != Player || RoundState != EThreeCardPokerRoundState::WaitingForBet || AnteBet > 0)
	{
		return false;
	}

	if (!Player->TrySpendCurrency(static_cast<float>(Amount)))
	{
		return false;
	}

	AnteBet = Amount;
	OnTableChanged.Broadcast();

	// No other seat to wait for — the Ante alone starts the round.
	StartRound();
	return true;
}

bool AThreeCardPokerTableActor::PlacePairPlus(Acasino_simulatorCharacter* Player, int32 Amount)
{
	if (!Player || Amount < MinPairPlusBet)
	{
		return false;
	}

	if (HasAuthority())
	{
		return ExecutePlacePairPlus(Player, Amount);
	}

	Player->ServerPlaceThreeCardPokerPairPlus(this, Amount);
	return true;
}

bool AThreeCardPokerTableActor::ExecutePlacePairPlus(Acasino_simulatorCharacter* Player, int32 Amount)
{
	if (!HasAuthority() || !Player || Amount < MinPairPlusBet)
	{
		return false;
	}

	// Pair Plus rides on an Ante already being down (standard table rule), and both bets are only
	// open while the hand hasn't been dealt yet.
	if (InteractingPlayer.Get() != Player || RoundState != EThreeCardPokerRoundState::WaitingForBet
		|| AnteBet <= 0 || PairPlusBet > 0)
	{
		return false;
	}

	if (!Player->TrySpendCurrency(static_cast<float>(Amount)))
	{
		return false;
	}

	PairPlusBet = Amount;
	OnTableChanged.Broadcast();
	return true;
}

bool AThreeCardPokerTableActor::PlayHand(Acasino_simulatorCharacter* Player)
{
	if (!Player)
	{
		return false;
	}

	if (HasAuthority())
	{
		return ExecutePlayHand(Player);
	}

	Player->ServerPlayThreeCardPokerHand(this);
	return true;
}

bool AThreeCardPokerTableActor::ExecutePlayHand(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || !Player)
	{
		return false;
	}

	if (InteractingPlayer.Get() != Player || RoundState != EThreeCardPokerRoundState::PlayerDecision || bDecisionMade)
	{
		return false;
	}

	if (!Player->TrySpendCurrency(static_cast<float>(AnteBet)))
	{
		return false;
	}

	PlayBet = AnteBet;
	bDecisionMade = true;
	OnTableChanged.Broadcast();
	FinishDecisionWindow();
	return true;
}

bool AThreeCardPokerTableActor::FoldHand(Acasino_simulatorCharacter* Player)
{
	if (!Player)
	{
		return false;
	}

	if (HasAuthority())
	{
		return ExecuteFoldHand(Player);
	}

	Player->ServerFoldThreeCardPokerHand(this);
	return true;
}

bool AThreeCardPokerTableActor::ExecuteFoldHand(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || !Player)
	{
		return false;
	}

	if (InteractingPlayer.Get() != Player || RoundState != EThreeCardPokerRoundState::PlayerDecision || bDecisionMade)
	{
		return false;
	}

	bFolded = true;
	bDecisionMade = true;
	OnTableChanged.Broadcast();
	FinishDecisionWindow();
	return true;
}

bool AThreeCardPokerTableActor::LeaveTable(Acasino_simulatorCharacter* Player)
{
	if (!Player)
	{
		return false;
	}

	if (HasAuthority())
	{
		return ExecuteLeaveTable(Player);
	}

	Player->ServerLeaveThreeCardPokerTable(this);
	return true;
}

bool AThreeCardPokerTableActor::ExecuteLeaveTable(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || !Player || InteractingPlayer.Get() != Player)
	{
		return false;
	}

	SetInteractingPlayer(nullptr);
	return true;
}

void AThreeCardPokerTableActor::ResetRound()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearDecisionWindowTimer();
	ClearDealingTimer();
	bDecisionWindowOpen = false;
	DecisionWindowEndsAtServerTime = 0.0f;

	PlayerCards.Reset();
	DealerHand.Reset();
	ServerDealerHand.Reset();
	AnteBet = 0;
	PlayBet = 0;
	PairPlusBet = 0;
	bFolded = false;
	bDecisionMade = false;
	LastResult = EThreeCardPokerHandResult::None;
	RoundState = EThreeCardPokerRoundState::WaitingForBet;
	OnTableChanged.Broadcast();
}

float AThreeCardPokerTableActor::GetDecisionRemainingTime() const
{
	if (!bDecisionWindowOpen)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, DecisionWindowEndsAtServerTime - GetServerWorldTimeSeconds());
}

EThreeCardPokerHandRank AThreeCardPokerTableActor::GetHandRank(const TArray<FBlackjackCard>& Cards) const
{
	return static_cast<EThreeCardPokerHandRank>(EvaluateHandValue(Cards) / 1000000);
}

bool AThreeCardPokerTableActor::IsDealerQualified() const
{
	if (DealerHand.Num() != 3)
	{
		return false;
	}

	if (GetHandRank(DealerHand) != EThreeCardPokerHandRank::HighCard)
	{
		return true;
	}

	int32 HighestValue = 0;
	for (const FBlackjackCard& Card : DealerHand)
	{
		const int32 Value = Card.Rank == EBlackjackRank::Ace ? 14 : static_cast<int32>(Card.Rank);
		HighestValue = FMath::Max(HighestValue, Value);
	}

	return HighestValue >= 12; // Queen (12) or better
}

void AThreeCardPokerTableActor::OnRep_TableState()
{
	OnTableChanged.Broadcast();
}

void AThreeCardPokerTableActor::BuildAndShuffleDeck()
{
	Deck.Reset();

	for (uint8 SuitIndex = 0; SuitIndex < 4; ++SuitIndex)
	{
		for (uint8 RankIndex = static_cast<uint8>(EBlackjackRank::Ace); RankIndex <= static_cast<uint8>(EBlackjackRank::King); ++RankIndex)
		{
			Deck.Add(FBlackjackCard(static_cast<EBlackjackSuit>(SuitIndex), static_cast<EBlackjackRank>(RankIndex)));
		}
	}

	for (int32 Index = Deck.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = FMath::RandRange(0, Index);
		Deck.Swap(Index, SwapIndex);
	}
}

FBlackjackCard AThreeCardPokerTableActor::DrawCard()
{
	if (Deck.IsEmpty())
	{
		BuildAndShuffleDeck();
	}

	return Deck.Pop();
}

void AThreeCardPokerTableActor::DealCardToPlayer()
{
	FBlackjackCard Card = DrawCard();
	Card.bFaceUp = true;
	PlayerCards.Add(Card);
	OnPlayerCardDealt.Broadcast(Card);
}

void AThreeCardPokerTableActor::DealCardToDealer(bool bFaceUp)
{
	FBlackjackCard ServerCard = DrawCard();
	ServerCard.bFaceUp = true;
	ServerDealerHand.Add(ServerCard);

	FBlackjackCard PublicCard = ServerCard;
	PublicCard.bFaceUp = bFaceUp;
	if (!bFaceUp)
	{
		// Mask the real card so non-authoritative observers can't read it off replicated state
		// before the reveal. Suit/Rank here are a fixed dummy, not the real card — Rank::None was
		// tried first but that maps to NAME_None in UBlackjackBlueprintLibrary::GetBlackjackRankName,
		// which makes FindBlackjackCardVisualData fail to resolve *any* row (not even a card-back
		// texture) for the hidden card's 3D visual. Ace still reveals nothing real about the card
		// (Suit is already fixed to Clubs) but resolves a valid row so the back texture renders.
		PublicCard.Suit = EBlackjackSuit::Clubs;
		PublicCard.Rank = EBlackjackRank::Ace;
	}

	DealerHand.Add(PublicCard);
	OnDealerCardDealt.Broadcast(DealerHand.Last());
}

void AThreeCardPokerTableActor::StartRound()
{
	// A fresh 52-card deck every round — no multi-deck shoe/discard bookkeeping needed.
	BuildAndShuffleDeck();

	PlayerCards.Reset();
	ServerDealerHand.Reset();
	DealerHand.Reset();
	RoundState = EThreeCardPokerRoundState::Dealing;
	NextDealingCardIndex = 0;

	// Deal the 6 opening cards one at a time (player, dealer, player, dealer, ...) instead of all
	// in the same frame, so the 3D deal animation is visibly sequential. DealNextRoundCard() moves
	// on to StartDecisionWindow() once all 6 are out.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DealingTimerHandle, this, &AThreeCardPokerTableActor::DealNextRoundCard, DealIntervalSeconds, true, 0.0f);
	}
}

void AThreeCardPokerTableActor::DealNextRoundCard()
{
	if ((NextDealingCardIndex % 2) == 0)
	{
		DealCardToPlayer();
	}
	else
	{
		DealCardToDealer(false);
	}

	++NextDealingCardIndex;

	if (NextDealingCardIndex >= 6)
	{
		ClearDealingTimer();
		StartDecisionWindow();
	}
}

void AThreeCardPokerTableActor::ClearDealingTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DealingTimerHandle);
	}
}

void AThreeCardPokerTableActor::StartDecisionWindow()
{
	const float Now = GetServerWorldTimeSeconds();
	bDecisionWindowOpen = true;
	DecisionWindowEndsAtServerTime = Now + DefaultDecisionWindowSeconds;
	RoundState = EThreeCardPokerRoundState::PlayerDecision;

	ScheduleDecisionWindowTimer();
	OnTableChanged.Broadcast();
}

void AThreeCardPokerTableActor::FinishDecisionWindow()
{
	if (!bDecisionWindowOpen)
	{
		return;
	}

	ClearDecisionWindowTimer();
	bDecisionWindowOpen = false;
	DecisionWindowEndsAtServerTime = 0.0f;

	if (!bDecisionMade)
	{
		// Timed out without a decision — auto-fold rather than block the table.
		bFolded = true;
		bDecisionMade = true;
	}

	RevealDealerHandAndResolve();
}

void AThreeCardPokerTableActor::ScheduleDecisionWindowTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearDecisionWindowTimer();
	if (!bDecisionWindowOpen)
	{
		return;
	}

	const float RemainingSeconds = GetDecisionRemainingTime();
	if (RemainingSeconds <= KINDA_SMALL_NUMBER)
	{
		FinishDecisionWindow();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DecisionWindowTimerHandle, this, &AThreeCardPokerTableActor::FinishDecisionWindow, RemainingSeconds, false);
	}
}

void AThreeCardPokerTableActor::ClearDecisionWindowTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DecisionWindowTimerHandle);
	}
}

void AThreeCardPokerTableActor::RevealDealerHandAndResolve()
{
	RoundState = EThreeCardPokerRoundState::Resolving;

	DealerHand = ServerDealerHand;
	for (FBlackjackCard& Card : DealerHand)
	{
		Card.bFaceUp = true;
	}

	OnDealerHandRevealed.Broadcast();
	Resolve();
}

void AThreeCardPokerTableActor::Resolve()
{
	Acasino_simulatorCharacter* Player = InteractingPlayer.Get();
	if (!Player || AnteBet <= 0 || PlayerCards.Num() != 3)
	{
		RoundState = EThreeCardPokerRoundState::RoundComplete;
		OnTableChanged.Broadcast();
		OnRoundCompleted.Broadcast();
		return;
	}

	const EThreeCardPokerHandRank PlayerRank = GetHandRank(PlayerCards);

	// Pair Plus is judged on the player's own hand alone, independent of Fold/dealer qualify.
	if (PairPlusBet > 0)
	{
		const int32 Multiplier = GetPairPlusMultiplier(PlayerRank);
		if (Multiplier > 0)
		{
			Player->AddCurrency(static_cast<float>(PairPlusBet * (Multiplier + 1)));
		}
	}

	// Same for the Ante Bonus — paid even on a fold, as long as the Ante was down.
	if (bEnableAnteBonus && PlayerRank >= EThreeCardPokerHandRank::Straight)
	{
		const int32 BonusMultiplier = GetAnteBonusMultiplier(PlayerRank);
		if (BonusMultiplier > 0)
		{
			Player->AddCurrency(static_cast<float>(AnteBet * BonusMultiplier));
		}
	}

	if (bFolded)
	{
		LastResult = EThreeCardPokerHandResult::Folded;
	}
	else
	{
		const bool bDealerQualifies = IsDealerQualified();
		if (!bDealerQualifies)
		{
			Player->AddCurrency(static_cast<float>(AnteBet * 2)); // Ante pays 1:1
			Player->AddCurrency(static_cast<float>(PlayBet));     // Play pushes
			LastResult = EThreeCardPokerHandResult::DealerNotQualified;
		}
		else
		{
			const int32 PlayerValue = EvaluateHandValue(PlayerCards);
			const int32 DealerValue = EvaluateHandValue(DealerHand);
			if (PlayerValue > DealerValue)
			{
				Player->AddCurrency(static_cast<float>(AnteBet * 2));
				Player->AddCurrency(static_cast<float>(PlayBet * 2));
				LastResult = EThreeCardPokerHandResult::PlayerWin;
			}
			else if (PlayerValue == DealerValue)
			{
				Player->AddCurrency(static_cast<float>(AnteBet));
				Player->AddCurrency(static_cast<float>(PlayBet));
				LastResult = EThreeCardPokerHandResult::Push;
			}
			else
			{
				LastResult = EThreeCardPokerHandResult::DealerWin;
			}
		}
	}

	RoundState = EThreeCardPokerRoundState::RoundComplete;
	OnTableChanged.Broadcast();
	OnRoundCompleted.Broadcast();
}

float AThreeCardPokerTableActor::GetServerWorldTimeSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}

		return World->GetTimeSeconds();
	}

	return 0.0f;
}

int32 AThreeCardPokerTableActor::EvaluateHandValue(const TArray<FBlackjackCard>& Cards) const
{
	if (Cards.Num() != 3)
	{
		return 0;
	}

	auto RankToValue = [](EBlackjackRank Rank) -> int32
	{
		return Rank == EBlackjackRank::Ace ? 14 : static_cast<int32>(Rank);
	};

	TArray<int32> Values = { RankToValue(Cards[0].Rank), RankToValue(Cards[1].Rank), RankToValue(Cards[2].Rank) };
	Values.Sort();
	const int32 V0 = Values[0];
	const int32 V1 = Values[1];
	const int32 V2 = Values[2];

	const bool bFlush = Cards[0].Suit == Cards[1].Suit && Cards[1].Suit == Cards[2].Suit;
	const bool bTrips = V0 == V1 && V1 == V2;
	const bool bPair = !bTrips && (V0 == V1 || V1 == V2);

	bool bStraight = false;
	int32 StraightHighValue = 0;
	if (V2 == V1 + 1 && V1 == V0 + 1)
	{
		bStraight = true;
		StraightHighValue = V2; // covers Q-K-A too, since Ace maps to 14 here
	}
	else if (V0 == 2 && V1 == 3 && V2 == 14)
	{
		bStraight = true;
		StraightHighValue = 3; // A-2-3 wheel: the lowest straight
	}

	EThreeCardPokerHandRank Rank;
	int32 Tiebreak;

	if (bStraight && bFlush)
	{
		Rank = EThreeCardPokerHandRank::StraightFlush;
		Tiebreak = StraightHighValue;
	}
	else if (bTrips)
	{
		Rank = EThreeCardPokerHandRank::ThreeOfAKind;
		Tiebreak = V0;
	}
	else if (bStraight)
	{
		Rank = EThreeCardPokerHandRank::Straight;
		Tiebreak = StraightHighValue;
	}
	else if (bFlush)
	{
		Rank = EThreeCardPokerHandRank::Flush;
		Tiebreak = V2 * 10000 + V1 * 100 + V0;
	}
	else if (bPair)
	{
		Rank = EThreeCardPokerHandRank::Pair;
		const int32 PairValue = (V0 == V1) ? V0 : V1;
		const int32 KickerValue = (V0 == V1) ? V2 : V0;
		Tiebreak = PairValue * 100 + KickerValue;
	}
	else
	{
		Rank = EThreeCardPokerHandRank::HighCard;
		Tiebreak = V2 * 10000 + V1 * 100 + V0;
	}

	return static_cast<int32>(Rank) * 1000000 + Tiebreak;
}

int32 AThreeCardPokerTableActor::GetPairPlusMultiplier(EThreeCardPokerHandRank Rank) const
{
	switch (Rank)
	{
	case EThreeCardPokerHandRank::Pair: return PairPlusPayouts.PairMultiplier;
	case EThreeCardPokerHandRank::Flush: return PairPlusPayouts.FlushMultiplier;
	case EThreeCardPokerHandRank::Straight: return PairPlusPayouts.StraightMultiplier;
	case EThreeCardPokerHandRank::ThreeOfAKind: return PairPlusPayouts.ThreeOfAKindMultiplier;
	case EThreeCardPokerHandRank::StraightFlush: return PairPlusPayouts.StraightFlushMultiplier;
	default: return 0;
	}
}

int32 AThreeCardPokerTableActor::GetAnteBonusMultiplier(EThreeCardPokerHandRank Rank) const
{
	switch (Rank)
	{
	case EThreeCardPokerHandRank::Straight: return AnteBonusPayouts.StraightMultiplier;
	case EThreeCardPokerHandRank::ThreeOfAKind: return AnteBonusPayouts.ThreeOfAKindMultiplier;
	case EThreeCardPokerHandRank::StraightFlush: return AnteBonusPayouts.StraightFlushMultiplier;
	default: return 0;
	}
}
