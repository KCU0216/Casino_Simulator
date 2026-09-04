// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackTableActor.h"

#include "Blackjack/BlackjackPlayerComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"

ABlackjackTableActor::ABlackjackTableActor()
{
	bReplicates = true;
	SetReplicateMovement(false);

	TableRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TableRoot"));
	SetRootComponent(TableRoot);

	StandBackPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StandBackPoint"));
	StandBackPoint->SetupAttachment(TableRoot);
	StandBackPoint->SetRelativeLocation(FVector(-280.0f, 0.0f, 0.0f));
	StandBackPoint->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	SeatPoint0 = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint0"));
	SeatPoint0->SetupAttachment(TableRoot);
	SeatPoint0->SetRelativeLocation(FVector(-130.0f, -120.0f, 0.0f));
	SeatPoint0->SetRelativeRotation(FRotator(0.0f, 35.0f, 0.0f));

	SeatPoint1 = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint1"));
	SeatPoint1->SetupAttachment(TableRoot);
	SeatPoint1->SetRelativeLocation(FVector(-170.0f, -40.0f, 0.0f));
	SeatPoint1->SetRelativeRotation(FRotator(0.0f, 15.0f, 0.0f));

	SeatPoint2 = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint2"));
	SeatPoint2->SetupAttachment(TableRoot);
	SeatPoint2->SetRelativeLocation(FVector(-170.0f, 40.0f, 0.0f));
	SeatPoint2->SetRelativeRotation(FRotator(0.0f, -15.0f, 0.0f));

	SeatPoint3 = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint3"));
	SeatPoint3->SetupAttachment(TableRoot);
	SeatPoint3->SetRelativeLocation(FVector(-130.0f, 120.0f, 0.0f));
	SeatPoint3->SetRelativeRotation(FRotator(0.0f, -35.0f, 0.0f));

	DealerCardRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DealerCardRoot"));
	DealerCardRoot->SetupAttachment(TableRoot);
	DealerCardRoot->SetRelativeLocation(FVector(60.0f, 0.0f, 8.0f));

	DeckPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DeckPoint"));
	DeckPoint->SetupAttachment(TableRoot);
	DeckPoint->SetRelativeLocation(FVector(30.0f, 95.0f, 8.0f));

	InitializeSeats();
}

void ABlackjackTableActor::BeginPlay()
{
	Super::BeginPlay();

	if (Seats.Num() != 4)
	{
		InitializeSeats();
	}

	if (HasAuthority() && Shoe.IsEmpty())
	{
		BuildAndShuffleShoe();
	}
}

void ABlackjackTableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABlackjackTableActor, RoundState);
	DOREPLIFETIME(ABlackjackTableActor, Seats);
	DOREPLIFETIME(ABlackjackTableActor, DealerHand);
	DOREPLIFETIME(ABlackjackTableActor, ActiveSeatIndex);
	DOREPLIFETIME(ABlackjackTableActor, bBettingWindowOpen);
	DOREPLIFETIME(ABlackjackTableActor, BettingWindowEndsAtServerTime);
	DOREPLIFETIME(ABlackjackTableActor, BettingWindowMaxEndsAtServerTime);
}

bool ABlackjackTableActor::TryClaimSeat(Acasino_simulatorCharacter* Player, int32 SeatIndex)
{
	if (!HasAuthority() || GetSeatClaimResult(Player, SeatIndex) != EBlackjackSeatClaimResult::Accepted)
	{
		return false;
	}

	Seats[SeatIndex].Occupant = Player;
	Seats[SeatIndex].SeatIndex = SeatIndex;
	BroadcastSeat(SeatIndex);
	OnTableChanged.Broadcast();
	return true;
}

EBlackjackSeatClaimResult ABlackjackTableActor::GetSeatClaimResult(Acasino_simulatorCharacter* Player, int32 SeatIndex) const
{
	if (!Player)
	{
		return EBlackjackSeatClaimResult::InvalidPlayer;
	}

	if (!IsValidSeatIndex(SeatIndex))
	{
		return EBlackjackSeatClaimResult::InvalidSeat;
	}

	if (Seats[SeatIndex].IsOccupied())
	{
		return EBlackjackSeatClaimResult::SeatOccupied;
	}

	if (GetSeatIndexForPlayer(Player) != INDEX_NONE)
	{
		return EBlackjackSeatClaimResult::PlayerAlreadySeated;
	}

	return EBlackjackSeatClaimResult::Accepted;
}

bool ABlackjackTableActor::CanClaimSeat(Acasino_simulatorCharacter* Player, int32 SeatIndex) const
{
	return GetSeatClaimResult(Player, SeatIndex) == EBlackjackSeatClaimResult::Accepted;
}

void ABlackjackTableActor::LeaveSeat(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || !Player)
	{
		return;
	}

	const int32 SeatIndex = GetSeatIndexForPlayer(Player);
	if (!IsValidSeatIndex(SeatIndex))
	{
		return;
	}

	if (!CanLeaveSeat(Player))
	{
		return;
	}

	Seats[SeatIndex] = FBlackjackSeatState();
	Seats[SeatIndex].SeatIndex = SeatIndex;
	BroadcastSeat(SeatIndex);
	OnTableChanged.Broadcast();
	TryStartRoundFromBettingWindow();
}

bool ABlackjackTableActor::CanLeaveSeat(Acasino_simulatorCharacter* Player) const
{
	if (!Player)
	{
		return false;
	}

	const int32 SeatIndex = GetSeatIndexForPlayer(Player);
	if (!IsValidSeatIndex(SeatIndex))
	{
		return false;
	}

	return !IsSeatLockedForCurrentRound(Seats[SeatIndex]);
}

bool ABlackjackTableActor::ToggleLeaveAfterRound(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || !Player || CanLeaveSeat(Player))
	{
		return false;
	}

	const int32 SeatIndex = GetSeatIndexForPlayer(Player);
	if (!IsValidSeatIndex(SeatIndex))
	{
		return false;
	}

	Seats[SeatIndex].bLeaveAfterRound = !Seats[SeatIndex].bLeaveAfterRound;
	Seats[SeatIndex].RoundDecision = Seats[SeatIndex].bLeaveAfterRound
		? EBlackjackRoundDecision::LeaveAfterRound
		: (Seats[SeatIndex].BetAmount > 0 ? EBlackjackRoundDecision::BetPlaced : EBlackjackRoundDecision::None);
	BroadcastSeat(SeatIndex);
	OnTableChanged.Broadcast();
	return true;
}

int32 ABlackjackTableActor::ReleaseSeatsLeavingAfterRound()
{
	if (!HasAuthority() || IsRoundActive())
	{
		return 0;
	}

	TArray<Acasino_simulatorCharacter*> PlayersToRelease;
	for (const FBlackjackSeatState& Seat : Seats)
	{
		if (Seat.bLeaveAfterRound)
		{
			if (Acasino_simulatorCharacter* Player = Cast<Acasino_simulatorCharacter>(Seat.Occupant))
			{
				PlayersToRelease.Add(Player);
			}
		}
	}

	int32 ReleasedCount = 0;
	for (Acasino_simulatorCharacter* Player : PlayersToRelease)
	{
		if (!Player)
		{
			continue;
		}

		if (UBlackjackPlayerComponent* BlackjackPlayerComponent = Player->GetBlackjackPlayerComponent())
		{
			BlackjackPlayerComponent->CompleteExitBlackjackSeat();
		}
		else
		{
			LeaveSeat(Player);
		}

		if (GetSeatIndexForPlayer(Player) == INDEX_NONE)
		{
			++ReleasedCount;
		}
	}

	return ReleasedCount;
}

int32 ABlackjackTableActor::GetSeatIndexForPlayer(Acasino_simulatorCharacter* Player) const
{
	if (!Player)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Seats.Num(); ++Index)
	{
		if (Seats[Index].Occupant == Player)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool ABlackjackTableActor::IsLeaveAfterRoundRequested(Acasino_simulatorCharacter* Player) const
{
	const FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	return Seat && Seat->bLeaveAfterRound;
}

bool ABlackjackTableActor::ToggleSitOut(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || !Player || (RoundState != EBlackjackRoundState::WaitingForPlayers && RoundState != EBlackjackRoundState::Betting))
	{
		return false;
	}

	FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	if (!Seat || Seat->BetAmount > 0)
	{
		return false;
	}

	const bool bShouldSitOut = Seat->RoundDecision != EBlackjackRoundDecision::SitOut;
	Seat->RoundDecision = bShouldSitOut ? EBlackjackRoundDecision::SitOut : EBlackjackRoundDecision::None;
	Seat->bReadyForRound = false;
	Seat->bLeaveAfterRound = false;
	BroadcastSeat(Seat->SeatIndex);
	OnTableChanged.Broadcast();
	TryStartRoundFromBettingWindow();
	return true;
}

bool ABlackjackTableActor::IsSitOutRequested(Acasino_simulatorCharacter* Player) const
{
	const FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	return Seat && Seat->RoundDecision == EBlackjackRoundDecision::SitOut;
}

bool ABlackjackTableActor::IsSeatAvailable(int32 SeatIndex) const
{
	return IsValidSeatIndex(SeatIndex) && !Seats[SeatIndex].IsOccupied();
}

bool ABlackjackTableActor::PlaceBet(Acasino_simulatorCharacter* Player, int32 Amount)
{
	if (!HasAuthority() || !Player || Amount <= 0)
	{
		return false;
	}

	FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	if (!Seat || !bBettingWindowOpen || Seat->BetAmount > 0
		|| (RoundState != EBlackjackRoundState::WaitingForPlayers && RoundState != EBlackjackRoundState::Betting))
	{
		return false;
	}

	if (!Player->TrySpendCurrency(static_cast<float>(Amount)))
	{
		return false;
	}

	RoundState = EBlackjackRoundState::Betting;
	Seat->BetAmount = Amount;
	Seat->RoundDecision = EBlackjackRoundDecision::BetPlaced;
	Seat->bReadyForRound = true;
	Seat->bLeaveAfterRound = false;
	BroadcastSeat(Seat->SeatIndex);
	OnTableChanged.Broadcast();
	ExtendBettingWindow(MinAfterBetSeconds);
	TryStartRoundFromBettingWindow();
	return true;
}

bool ABlackjackTableActor::StartRound()
{
	if (!HasAuthority() || (RoundState != EBlackjackRoundState::WaitingForPlayers && RoundState != EBlackjackRoundState::Betting))
	{
		return false;
	}

	ClearBettingWindowTimer();
	bBettingWindowOpen = false;
	BettingWindowEndsAtServerTime = 0.0f;
	BettingWindowMaxEndsAtServerTime = 0.0f;

	bool bHasBettingPlayer = false;
	for (FBlackjackSeatState& Seat : Seats)
	{
		if (Seat.IsOccupied() && Seat.BetAmount > 0)
		{
			bHasBettingPlayer = true;
			Seat.RoundDecision = EBlackjackRoundDecision::BetPlaced;
			Seat.Hands.Reset();
			FBlackjackHand InitialHand;
			InitialHand.BetAmount = Seat.BetAmount;
			InitialHand.LastResult = EBlackjackSeatResult::None;
			Seat.Hands.Add(InitialHand);
			Seat.ActiveHandIndex = 0;
			Seat.LastResult = EBlackjackSeatResult::None;
			Seat.InsuranceBetAmount = 0;
			Seat.bInsuranceDecisionMade = false;
			Seat.bHasSplitThisRound = false;
		}
	}

	if (!bHasBettingPlayer)
	{
		return false;
	}

	if (ShouldShuffleBeforeRound())
	{
		BuildAndShuffleShoe();
	}

	ServerDealerHand = FBlackjackHand();
	DealerHand = FBlackjackHand();
	ActiveSeatIndex = INDEX_NONE;
	RoundState = EBlackjackRoundState::Dealing;

	for (int32 CardRound = 0; CardRound < 2; ++CardRound)
	{
		for (int32 SeatIndex = 0; SeatIndex < Seats.Num(); ++SeatIndex)
		{
			if (Seats[SeatIndex].IsOccupied() && Seats[SeatIndex].BetAmount > 0)
			{
				DealCardToSeat(SeatIndex);
			}
		}

		DealCardToDealer(CardRound == 0);
	}

	if (DealerHand.Cards.Num() >= 1 && DealerHand.Cards[0].Rank == EBlackjackRank::Ace)
	{
		RoundState = EBlackjackRoundState::Insurance;
		OnTableChanged.Broadcast();
		return true;
	}

	RoundState = EBlackjackRoundState::PlayerTurns;

	if (!MoveToNextPlayableHand(INDEX_NONE))
	{
		RunDealerAndResolve();
	}
	else
	{
		OnTableChanged.Broadcast();
	}

	return true;
}

bool ABlackjackTableActor::PlayerHit(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || RoundState != EBlackjackRoundState::PlayerTurns)
	{
		return false;
	}

	FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	if (!Seat || !Seat->Hands.IsValidIndex(Seat->ActiveHandIndex) || !IsPlayerTurn(Player))
	{
		return false;
	}

	FBlackjackHand& Hand = Seat->Hands[Seat->ActiveHandIndex];
	if (Hand.bStood || IsHandBust(Hand) || IsNaturalBlackjack(Hand))
	{
		return false;
	}

	Hand.Cards.Add(DrawCard());
	OnPlayerCardDealt.Broadcast(Seat->SeatIndex, Hand.Cards.Last());

	if (IsHandBust(Hand))
	{
		Hand.bStood = true;
		AdvanceTurnAfterSeat(Seat->SeatIndex);
	}
	else
	{
		BroadcastSeat(Seat->SeatIndex);
		OnTableChanged.Broadcast();
	}

	return true;
}

bool ABlackjackTableActor::PlayerStand(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || RoundState != EBlackjackRoundState::PlayerTurns)
	{
		return false;
	}

	FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	if (!Seat || !Seat->Hands.IsValidIndex(Seat->ActiveHandIndex) || !IsPlayerTurn(Player))
	{
		return false;
	}

	Seat->Hands[Seat->ActiveHandIndex].bStood = true;
	AdvanceTurnAfterSeat(Seat->SeatIndex);
	return true;
}

bool ABlackjackTableActor::PlayerDoubleDown(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || RoundState != EBlackjackRoundState::PlayerTurns)
	{
		return false;
	}

	FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	if (!Seat || !Seat->Hands.IsValidIndex(Seat->ActiveHandIndex) || !IsPlayerTurn(Player))
	{
		return false;
	}

	FBlackjackHand& Hand = Seat->Hands[Seat->ActiveHandIndex];
	if (Hand.Cards.Num() != 2 || Hand.BetAmount <= 0 || IsNaturalBlackjack(Hand) || !Player->TrySpendCurrency(static_cast<float>(Hand.BetAmount)))
	{
		return false;
	}

	Hand.BetAmount *= 2;
	Hand.bDoubledDown = true;
	Hand.Cards.Add(DrawCard());
	Hand.bStood = true;
	OnPlayerCardDealt.Broadcast(Seat->SeatIndex, Hand.Cards.Last());
	AdvanceTurnAfterSeat(Seat->SeatIndex);
	return true;
}

bool ABlackjackTableActor::PlayerSplit(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || RoundState != EBlackjackRoundState::PlayerTurns)
	{
		return false;
	}

	FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	if (!Seat || !IsPlayerTurn(Player) || Seat->bHasSplitThisRound || !CanSplitSeat(Seat->SeatIndex) || !Player->TrySpendCurrency(static_cast<float>(Seat->BetAmount)))
	{
		return false;
	}

	FBlackjackHand& FirstHand = Seat->Hands[0];
	FBlackjackHand SecondHand;
	SecondHand.bFromSplit = true;
	SecondHand.BetAmount = Seat->BetAmount;
	SecondHand.LastResult = EBlackjackSeatResult::None;
	SecondHand.Cards.Add(FirstHand.Cards[1]);
	FirstHand.Cards.RemoveAt(1);
	FirstHand.bFromSplit = true;
	FirstHand.BetAmount = Seat->BetAmount;
	FirstHand.LastResult = EBlackjackSeatResult::None;

	Seat->Hands.Add(SecondHand);
	Seat->bHasSplitThisRound = true;
	Seat->ActiveHandIndex = 0;

	FirstHand.Cards.Add(DrawCard());
	Seat->Hands[1].Cards.Add(DrawCard());

	BroadcastSeat(Seat->SeatIndex);
	OnTableChanged.Broadcast();
	return true;
}

bool ABlackjackTableActor::PlaceInsurance(Acasino_simulatorCharacter* Player, int32 Amount)
{
	if (!HasAuthority() || !CanOfferInsurance() || !Player || Amount <= 0)
	{
		return false;
	}

	FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	const int32 MaxInsurance = Seat ? Seat->BetAmount / 2 : 0;
	if (!Seat || Amount > MaxInsurance || Seat->InsuranceBetAmount > 0)
	{
		return false;
	}

	if (!Player->TrySpendCurrency(static_cast<float>(Amount)))
	{
		return false;
	}

	Seat->InsuranceBetAmount = Amount;
	Seat->bInsuranceDecisionMade = true;
	BroadcastSeat(Seat->SeatIndex);
	FinishInsuranceIfReady();
	return true;
}

bool ABlackjackTableActor::SkipInsurance(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || RoundState != EBlackjackRoundState::Insurance || !Player)
	{
		return false;
	}

	FBlackjackSeatState* Seat = FindSeatForPlayer(Player);
	if (!Seat || Seat->BetAmount <= 0 || Seat->bInsuranceDecisionMade)
	{
		return false;
	}

	Seat->bInsuranceDecisionMade = true;
	BroadcastSeat(Seat->SeatIndex);
	FinishInsuranceIfReady();
	return true;
}

void ABlackjackTableActor::ResetRound()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearBettingWindowTimer();
	bBettingWindowOpen = false;
	BettingWindowEndsAtServerTime = 0.0f;
	BettingWindowMaxEndsAtServerTime = 0.0f;

	for (FBlackjackSeatState& Seat : Seats)
	{
		Seat.Hands.Reset();
		Seat.ActiveHandIndex = 0;
		Seat.BetAmount = 0;
		Seat.InsuranceBetAmount = 0;
		Seat.LastResult = EBlackjackSeatResult::None;
		Seat.RoundDecision = Seat.bLeaveAfterRound ? EBlackjackRoundDecision::LeaveAfterRound : EBlackjackRoundDecision::None;
		Seat.bReadyForRound = false;
		Seat.bHasSplitThisRound = false;
		Seat.bInsuranceDecisionMade = false;
		BroadcastSeat(Seat.SeatIndex);
	}

	ServerDealerHand = FBlackjackHand();
	DealerHand = FBlackjackHand();
	ActiveSeatIndex = INDEX_NONE;
	RoundState = EBlackjackRoundState::WaitingForPlayers;
	OnTableChanged.Broadcast();
}

bool ABlackjackTableActor::StartBettingWindow(float DurationSeconds)
{
	if (!HasAuthority() || (RoundState != EBlackjackRoundState::WaitingForPlayers && RoundState != EBlackjackRoundState::Betting))
	{
		return false;
	}

	const float Now = GetServerWorldTimeSeconds();
	const float WindowDuration = DurationSeconds > 0.0f ? DurationSeconds : DefaultBettingWindowSeconds;
	const float ClampedDuration = FMath::Max(1.0f, WindowDuration);
	const float MaxDuration = FMath::Max(ClampedDuration, MaxBettingWindowSeconds);

	bBettingWindowOpen = true;
	BettingWindowEndsAtServerTime = Now + ClampedDuration;
	BettingWindowMaxEndsAtServerTime = Now + MaxDuration;
	RoundState = EBlackjackRoundState::Betting;

	for (FBlackjackSeatState& Seat : Seats)
	{
		if (!Seat.IsOccupied())
		{
			continue;
		}

		if (Seat.BetAmount > 0)
		{
			Seat.RoundDecision = EBlackjackRoundDecision::BetPlaced;
			Seat.bReadyForRound = true;
		}
		else if (Seat.bLeaveAfterRound)
		{
			Seat.RoundDecision = EBlackjackRoundDecision::LeaveAfterRound;
			Seat.bReadyForRound = false;
		}
		else
		{
			Seat.RoundDecision = EBlackjackRoundDecision::None;
			Seat.bReadyForRound = false;
		}

		BroadcastSeat(Seat.SeatIndex);
	}

	ScheduleBettingWindowTimer();
	OnTableChanged.Broadcast();
	TryStartRoundFromBettingWindow();
	return true;
}

void ABlackjackTableActor::FinishBettingWindow()
{
	if (!HasAuthority() || !bBettingWindowOpen)
	{
		return;
	}

	ClearBettingWindowTimer();
	bBettingWindowOpen = false;
	BettingWindowEndsAtServerTime = 0.0f;
	BettingWindowMaxEndsAtServerTime = 0.0f;

	if (HasAnyBettingPlayer())
	{
		StartRound();
		return;
	}

	RoundState = EBlackjackRoundState::WaitingForPlayers;
	for (FBlackjackSeatState& Seat : Seats)
	{
		if (Seat.RoundDecision == EBlackjackRoundDecision::None)
		{
			continue;
		}

		Seat.RoundDecision = Seat.bLeaveAfterRound ? EBlackjackRoundDecision::LeaveAfterRound : EBlackjackRoundDecision::None;
		Seat.bReadyForRound = false;
		BroadcastSeat(Seat.SeatIndex);
	}

	OnTableChanged.Broadcast();
}

bool ABlackjackTableActor::ExtendBettingWindow(float MinRemainingSeconds)
{
	if (!HasAuthority() || !bBettingWindowOpen || MinRemainingSeconds <= 0.0f)
	{
		return false;
	}

	const float Now = GetServerWorldTimeSeconds();
	const float DesiredEndTime = Now + MinRemainingSeconds;
	const float NewEndTime = FMath::Min(FMath::Max(BettingWindowEndsAtServerTime, DesiredEndTime), BettingWindowMaxEndsAtServerTime);
	if (NewEndTime <= BettingWindowEndsAtServerTime + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	BettingWindowEndsAtServerTime = NewEndTime;
	ScheduleBettingWindowTimer();
	OnTableChanged.Broadcast();
	return true;
}

bool ABlackjackTableActor::NotifyBettingInteractionStarted(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || !bBettingWindowOpen || !Player || GetSeatIndexForPlayer(Player) == INDEX_NONE)
	{
		return false;
	}

	return ExtendBettingWindow(MinBettingInteractionSeconds);
}

bool ABlackjackTableActor::HasAnyBettingPlayer() const
{
	for (const FBlackjackSeatState& Seat : Seats)
	{
		if (Seat.IsOccupied() && Seat.BetAmount > 0)
		{
			return true;
		}
	}

	return false;
}

bool ABlackjackTableActor::AreAllSeatedPlayersDecided() const
{
	bool bHasSeatedPlayer = false;
	for (const FBlackjackSeatState& Seat : Seats)
	{
		if (!Seat.IsOccupied())
		{
			continue;
		}

		bHasSeatedPlayer = true;
		if (Seat.BetAmount > 0)
		{
			continue;
		}

		if (Seat.RoundDecision != EBlackjackRoundDecision::SitOut
			&& Seat.RoundDecision != EBlackjackRoundDecision::LeaveAfterRound)
		{
			return false;
		}
	}

	return bHasSeatedPlayer;
}

float ABlackjackTableActor::GetBettingRemainingTime() const
{
	if (!bBettingWindowOpen)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, BettingWindowEndsAtServerTime - GetServerWorldTimeSeconds());
}

int32 ABlackjackTableActor::GetHandBestValue(const FBlackjackHand& Hand) const
{
	int32 Total = 0;
	int32 AceCount = 0;

	for (const FBlackjackCard& Card : Hand.Cards)
	{
		if (!Card.bFaceUp)
		{
			continue;
		}

		if (Card.Rank == EBlackjackRank::Ace)
		{
			++AceCount;
			Total += 11;
		}
		else
		{
			Total += GetCardValue(Card);
		}
	}

	while (Total > 21 && AceCount > 0)
	{
		Total -= 10;
		--AceCount;
	}

	return Total;
}

bool ABlackjackTableActor::IsHandBust(const FBlackjackHand& Hand) const
{
	return GetHandBestValue(Hand) > 21;
}

bool ABlackjackTableActor::IsNaturalBlackjack(const FBlackjackHand& Hand) const
{
	return Hand.Cards.Num() == 2 && !Hand.bFromSplit && GetHandBestValue(Hand) == 21;
}

bool ABlackjackTableActor::CanSplitSeat(int32 SeatIndex) const
{
	if (!IsValidSeatIndex(SeatIndex))
	{
		return false;
	}

	const FBlackjackSeatState& Seat = Seats[SeatIndex];
	if (Seat.Hands.Num() != 1 || Seat.bHasSplitThisRound || Seat.BetAmount <= 0)
	{
		return false;
	}

	const FBlackjackHand& Hand = Seat.Hands[0];
	return Hand.Cards.Num() == 2 && Hand.Cards[0].Rank == Hand.Cards[1].Rank;
}

bool ABlackjackTableActor::CanOfferInsurance() const
{
	return DealerHand.Cards.Num() >= 1 && DealerHand.Cards[0].Rank == EBlackjackRank::Ace && RoundState == EBlackjackRoundState::Insurance;
}

bool ABlackjackTableActor::IsPlayerTurn(Acasino_simulatorCharacter* Player) const
{
	return RoundState == EBlackjackRoundState::PlayerTurns && GetSeatIndexForPlayer(Player) == ActiveSeatIndex;
}

USceneComponent* ABlackjackTableActor::GetSeatPoint(int32 SeatIndex) const
{
	switch (SeatIndex)
	{
	case 0: return SeatPoint0;
	case 1: return SeatPoint1;
	case 2: return SeatPoint2;
	case 3: return SeatPoint3;
	default: return nullptr;
	}
}

void ABlackjackTableActor::OnRep_TableState()
{
	for (int32 SeatIndex = 0; SeatIndex < Seats.Num(); ++SeatIndex)
	{
		BroadcastSeat(SeatIndex);
	}

	OnTableChanged.Broadcast();
}

void ABlackjackTableActor::InitializeSeats()
{
	Seats.SetNum(4);
	for (int32 Index = 0; Index < Seats.Num(); ++Index)
	{
		Seats[Index].SeatIndex = Index;
	}
}

void ABlackjackTableActor::BuildAndShuffleShoe()
{
	Shoe.Reset();
	DiscardPile.Reset();

	for (int32 DeckIndex = 0; DeckIndex < DeckCount; ++DeckIndex)
	{
		for (uint8 SuitIndex = 0; SuitIndex < 4; ++SuitIndex)
		{
			for (uint8 RankIndex = static_cast<uint8>(EBlackjackRank::Ace); RankIndex <= static_cast<uint8>(EBlackjackRank::King); ++RankIndex)
			{
				Shoe.Add(FBlackjackCard(static_cast<EBlackjackSuit>(SuitIndex), static_cast<EBlackjackRank>(RankIndex)));
			}
		}
	}

	for (int32 Index = Shoe.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = FMath::RandRange(0, Index);
		Shoe.Swap(Index, SwapIndex);
	}
}

bool ABlackjackTableActor::ShouldShuffleBeforeRound() const
{
	return Shoe.Num() <= ShuffleThreshold;
}

FBlackjackCard ABlackjackTableActor::DrawCard(bool bFaceUp)
{
	if (Shoe.IsEmpty())
	{
		BuildAndShuffleShoe();
	}

	FBlackjackCard Card = Shoe.Pop();
	Card.bFaceUp = bFaceUp;
	return Card;
}

void ABlackjackTableActor::DealCardToSeat(int32 SeatIndex, bool bFaceUp)
{
	if (!IsValidSeatIndex(SeatIndex) || Seats[SeatIndex].Hands.IsEmpty())
	{
		return;
	}

	FBlackjackHand& Hand = Seats[SeatIndex].Hands[0];
	Hand.Cards.Add(DrawCard(bFaceUp));
	OnPlayerCardDealt.Broadcast(SeatIndex, Hand.Cards.Last());
	BroadcastSeat(SeatIndex);
}

void ABlackjackTableActor::DealCardToDealer(bool bFaceUp)
{
	FBlackjackCard ServerCard = DrawCard(true);
	ServerDealerHand.Cards.Add(ServerCard);

	FBlackjackCard PublicCard = ServerCard;
	PublicCard.bFaceUp = bFaceUp;
	if (!bFaceUp)
	{
		PublicCard.Suit = EBlackjackSuit::Clubs;
		PublicCard.Rank = EBlackjackRank::None;
	}

	DealerHand.Cards.Add(PublicCard);
	OnDealerCardDealt.Broadcast(INDEX_NONE, DealerHand.Cards.Last());
}

void ABlackjackTableActor::AdvanceTurnAfterSeat(int32 SeatIndex)
{
	if (!IsValidSeatIndex(SeatIndex))
	{
		return;
	}

	BroadcastSeat(SeatIndex);

	if (!MoveToNextPlayableHand(SeatIndex))
	{
		RunDealerAndResolve();
	}
	else
	{
		OnTableChanged.Broadcast();
	}
}

void ABlackjackTableActor::FinishInsuranceIfReady()
{
	if (RoundState != EBlackjackRoundState::Insurance)
	{
		OnTableChanged.Broadcast();
		return;
	}

	if (!AllInsuranceDecisionsComplete())
	{
		OnTableChanged.Broadcast();
		return;
	}

	if (IsNaturalBlackjack(ServerDealerHand))
	{
		RunDealerAndResolve();
		return;
	}

	RoundState = EBlackjackRoundState::PlayerTurns;
	if (!MoveToNextPlayableHand(INDEX_NONE))
	{
		RunDealerAndResolve();
	}
	else
	{
		OnTableChanged.Broadcast();
	}
}

bool ABlackjackTableActor::AllInsuranceDecisionsComplete() const
{
	for (const FBlackjackSeatState& Seat : Seats)
	{
		if (!Seat.IsOccupied() || Seat.BetAmount <= 0)
		{
			continue;
		}

		if (!Seat.bInsuranceDecisionMade)
		{
			return false;
		}
	}

	return true;
}

bool ABlackjackTableActor::MoveToNextPlayableHand(int32 CurrentSeatIndex)
{
	ActiveSeatIndex = INDEX_NONE;

	if (IsValidSeatIndex(CurrentSeatIndex))
	{
		FBlackjackSeatState& CurrentSeat = Seats[CurrentSeatIndex];
		for (int32 HandIndex = CurrentSeat.ActiveHandIndex + 1; HandIndex < CurrentSeat.Hands.Num(); ++HandIndex)
		{
			if (!IsHandComplete(CurrentSeat.Hands[HandIndex]))
			{
				CurrentSeat.ActiveHandIndex = HandIndex;
				ActiveSeatIndex = CurrentSeatIndex;
				BroadcastSeat(CurrentSeatIndex);
				return true;
			}
		}
	}

	const int32 FirstSeatIndex = IsValidSeatIndex(CurrentSeatIndex) ? CurrentSeatIndex + 1 : 0;
	for (int32 SeatIndex = FirstSeatIndex; SeatIndex < Seats.Num(); ++SeatIndex)
	{
		FBlackjackSeatState& Seat = Seats[SeatIndex];
		if (!Seat.IsOccupied() || Seat.BetAmount <= 0)
		{
			continue;
		}

		for (int32 HandIndex = 0; HandIndex < Seat.Hands.Num(); ++HandIndex)
		{
			if (!IsHandComplete(Seat.Hands[HandIndex]))
			{
				Seat.ActiveHandIndex = HandIndex;
				ActiveSeatIndex = SeatIndex;
				BroadcastSeat(SeatIndex);
				return true;
			}
		}
	}

	return false;
}

bool ABlackjackTableActor::IsHandComplete(const FBlackjackHand& Hand) const
{
	return Hand.bStood || IsHandBust(Hand) || IsNaturalBlackjack(Hand);
}

bool ABlackjackTableActor::HasAnyNonBustPlayerHand() const
{
	for (const FBlackjackSeatState& Seat : Seats)
	{
		if (!Seat.IsOccupied() || Seat.BetAmount <= 0)
		{
			continue;
		}

		for (const FBlackjackHand& Hand : Seat.Hands)
		{
			if (!Hand.Cards.IsEmpty() && !IsHandBust(Hand))
			{
				return true;
			}
		}
	}

	return false;
}

bool ABlackjackTableActor::TryStartRoundFromBettingWindow()
{
	if (!HasAuthority() || !bBettingWindowOpen || !AreAllSeatedPlayersDecided())
	{
		return false;
	}

	FinishBettingWindow();
	return true;
}

void ABlackjackTableActor::ClearBettingWindowTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BettingWindowTimerHandle);
	}
}

void ABlackjackTableActor::ScheduleBettingWindowTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearBettingWindowTimer();
	if (!bBettingWindowOpen)
	{
		return;
	}

	const float RemainingSeconds = GetBettingRemainingTime();
	if (RemainingSeconds <= KINDA_SMALL_NUMBER)
	{
		FinishBettingWindow();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BettingWindowTimerHandle, this, &ABlackjackTableActor::FinishBettingWindow, RemainingSeconds, false);
	}
}

void ABlackjackTableActor::RunDealerAndResolve()
{
	RoundState = EBlackjackRoundState::DealerTurn;
	ActiveSeatIndex = INDEX_NONE;

	for (FBlackjackCard& Card : ServerDealerHand.Cards)
	{
		Card.bFaceUp = true;
	}
	DealerHand = ServerDealerHand;

	if (ServerDealerHand.Cards.IsValidIndex(1))
	{
		OnDealerHoleCardRevealed.Broadcast(INDEX_NONE, ServerDealerHand.Cards[1]);
	}

	if (!HasAnyNonBustPlayerHand())
	{
		ResolveSeats();
		return;
	}

	while (!IsHandBust(ServerDealerHand))
	{
		const int32 DealerValue = GetHandBestValue(ServerDealerHand);
		if (DealerValue > 17 || (DealerValue == 17 && (bDealerStandsOnSoft17 || !IsSoft17(ServerDealerHand))))
		{
			break;
		}

		DealCardToDealer(true);
	}

	ResolveSeats();
}

void ABlackjackTableActor::ResolveSeats()
{
	RoundState = EBlackjackRoundState::Resolving;
	const int32 DealerValue = GetHandBestValue(ServerDealerHand);
	const bool bDealerBust = IsHandBust(ServerDealerHand);
	const bool bDealerBlackjack = IsNaturalBlackjack(ServerDealerHand);

	for (int32 SeatIndex = 0; SeatIndex < Seats.Num(); ++SeatIndex)
	{
		FBlackjackSeatState& Seat = Seats[SeatIndex];
		Acasino_simulatorCharacter* Player = Cast<Acasino_simulatorCharacter>(Seat.Occupant);
		if (!Player || Seat.BetAmount <= 0 || Seat.Hands.IsEmpty())
		{
			continue;
		}

		if (Seat.InsuranceBetAmount > 0 && bDealerBlackjack)
		{
			Player->AddCurrency(static_cast<float>(Seat.InsuranceBetAmount * 3));
		}

		for (FBlackjackHand& Hand : Seat.Hands)
		{
			const int32 HandBetAmount = Hand.BetAmount > 0 ? Hand.BetAmount : Seat.BetAmount;
			const int32 PlayerValue = GetHandBestValue(Hand);
			const bool bPlayerBlackjack = IsNaturalBlackjack(Hand);

			if (IsHandBust(Hand))
			{
				Hand.LastResult = EBlackjackSeatResult::PlayerBust;
			}
			else if (bPlayerBlackjack && !bDealerBlackjack)
			{
				Hand.LastResult = EBlackjackSeatResult::PlayerBlackjack;
				Player->AddCurrency(static_cast<float>(HandBetAmount + FMath::FloorToInt(HandBetAmount * 1.5f)));
			}
			else if (bDealerBlackjack && !bPlayerBlackjack)
			{
				Hand.LastResult = EBlackjackSeatResult::DealerWin;
			}
			else if (bDealerBust || PlayerValue > DealerValue)
			{
				Hand.LastResult = EBlackjackSeatResult::PlayerWin;
				Player->AddCurrency(static_cast<float>(HandBetAmount * 2));
			}
			else if (PlayerValue == DealerValue)
			{
				Hand.LastResult = EBlackjackSeatResult::Push;
				Player->AddCurrency(static_cast<float>(HandBetAmount));
			}
			else
			{
				Hand.LastResult = EBlackjackSeatResult::DealerWin;
			}

			Seat.LastResult = Hand.LastResult;
		}

		BroadcastSeat(SeatIndex);
	}

	RoundState = EBlackjackRoundState::RoundComplete;
	OnTableChanged.Broadcast();
	OnRoundCompleted.Broadcast();
}

void ABlackjackTableActor::BroadcastSeat(int32 SeatIndex)
{
	if (IsValidSeatIndex(SeatIndex))
	{
		OnSeatChanged.Broadcast(SeatIndex, Seats[SeatIndex]);
	}
}

FBlackjackSeatState* ABlackjackTableActor::FindSeatForPlayer(Acasino_simulatorCharacter* Player)
{
	if (!Player)
	{
		return nullptr;
	}

	for (FBlackjackSeatState& Seat : Seats)
	{
		if (Seat.Occupant == Player)
		{
			return &Seat;
		}
	}

	return nullptr;
}

const FBlackjackSeatState* ABlackjackTableActor::FindSeatForPlayer(Acasino_simulatorCharacter* Player) const
{
	if (!Player)
	{
		return nullptr;
	}

	for (const FBlackjackSeatState& Seat : Seats)
	{
		if (Seat.Occupant == Player)
		{
			return &Seat;
		}
	}

	return nullptr;
}

bool ABlackjackTableActor::IsValidSeatIndex(int32 SeatIndex) const
{
	return Seats.IsValidIndex(SeatIndex);
}

bool ABlackjackTableActor::IsRoundActive() const
{
	return RoundState == EBlackjackRoundState::Dealing
		|| RoundState == EBlackjackRoundState::Insurance
		|| RoundState == EBlackjackRoundState::PlayerTurns
		|| RoundState == EBlackjackRoundState::DealerTurn
		|| RoundState == EBlackjackRoundState::Resolving;
}

bool ABlackjackTableActor::IsSeatLockedForCurrentRound(const FBlackjackSeatState& Seat) const
{
	if (RoundState == EBlackjackRoundState::Betting)
	{
		return Seat.BetAmount > 0 || Seat.RoundDecision == EBlackjackRoundDecision::BetPlaced;
	}

	if (!IsRoundActive())
	{
		return false;
	}

	return Seat.BetAmount > 0 || !Seat.Hands.IsEmpty();
}

float ABlackjackTableActor::GetServerWorldTimeSeconds() const
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

bool ABlackjackTableActor::AllActiveSeatsComplete() const
{
	for (const FBlackjackSeatState& Seat : Seats)
	{
		if (!Seat.IsOccupied() || Seat.BetAmount <= 0)
		{
			continue;
		}

		for (const FBlackjackHand& Hand : Seat.Hands)
		{
			if (!Hand.bStood && !IsHandBust(Hand) && !IsNaturalBlackjack(Hand))
			{
				return false;
			}
		}
	}

	return true;
}

int32 ABlackjackTableActor::GetCardValue(const FBlackjackCard& Card) const
{
	switch (Card.Rank)
	{
	case EBlackjackRank::Ace:
		return 11;
	case EBlackjackRank::Jack:
	case EBlackjackRank::Queen:
	case EBlackjackRank::King:
		return 10;
	default:
		return static_cast<int32>(Card.Rank);
	}
}

bool ABlackjackTableActor::IsSoft17(const FBlackjackHand& Hand) const
{
	int32 Total = 0;
	int32 AceCount = 0;

	for (const FBlackjackCard& Card : Hand.Cards)
	{
		if (!Card.bFaceUp)
		{
			continue;
		}

		if (Card.Rank == EBlackjackRank::Ace)
		{
			Total += 11;
			++AceCount;
		}
		else
		{
			Total += GetCardValue(Card);
		}
	}

	while (Total > 21 && AceCount > 0)
	{
		Total -= 10;
		--AceCount;
	}

	return Total == 17 && AceCount > 0;
}
