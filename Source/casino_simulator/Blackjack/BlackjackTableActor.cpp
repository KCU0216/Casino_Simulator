// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackTableActor.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"

ABlackjackTableActor::ABlackjackTableActor()
{
	bReplicates = true;
	SetReplicateMovement(false);

	TableRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TableRoot"));
	SetRootComponent(TableRoot);

	SeatSelectCameraPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatSelectCameraPoint"));
	SeatSelectCameraPoint->SetupAttachment(TableRoot);
	SeatSelectCameraPoint->SetRelativeLocation(FVector(-120.0f, 0.0f, 220.0f));
	SeatSelectCameraPoint->SetRelativeRotation(FRotator(-25.0f, 0.0f, 0.0f));

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
	DOREPLIFETIME(ABlackjackTableActor, Shoe);
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
	if (!HasAuthority() || !Player || IsRoundActive())
	{
		return;
	}

	const int32 SeatIndex = GetSeatIndexForPlayer(Player);
	if (!IsValidSeatIndex(SeatIndex))
	{
		return;
	}

	Seats[SeatIndex] = FBlackjackSeatState();
	Seats[SeatIndex].SeatIndex = SeatIndex;
	BroadcastSeat(SeatIndex);
	OnTableChanged.Broadcast();
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
	if (!Seat || (RoundState != EBlackjackRoundState::WaitingForPlayers && RoundState != EBlackjackRoundState::Betting))
	{
		return false;
	}

	if (!Player->TrySpendCurrency(static_cast<float>(Amount)))
	{
		return false;
	}

	RoundState = EBlackjackRoundState::Betting;
	Seat->BetAmount += Amount;
	Seat->bReadyForRound = true;
	BroadcastSeat(Seat->SeatIndex);
	OnTableChanged.Broadcast();
	return true;
}

bool ABlackjackTableActor::StartRound()
{
	if (!HasAuthority() || (RoundState != EBlackjackRoundState::WaitingForPlayers && RoundState != EBlackjackRoundState::Betting))
	{
		return false;
	}

	bool bHasBettingPlayer = false;
	for (FBlackjackSeatState& Seat : Seats)
	{
		if (Seat.IsOccupied() && Seat.BetAmount > 0)
		{
			bHasBettingPlayer = true;
			Seat.Hands.Reset();
			Seat.Hands.Add(FBlackjackHand());
			Seat.ActiveHandIndex = 0;
			Seat.LastResult = EBlackjackSeatResult::None;
			Seat.InsuranceBetAmount = 0;
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

	DealerHand = FBlackjackHand();
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

	RoundState = EBlackjackRoundState::PlayerTurns;

	if (CanOfferInsurance())
	{
		OnTableChanged.Broadcast();
		return true;
	}

	if (AllActiveSeatsComplete())
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
	if (!Seat || !Seat->Hands.IsValidIndex(Seat->ActiveHandIndex))
	{
		return false;
	}

	FBlackjackHand& Hand = Seat->Hands[Seat->ActiveHandIndex];
	if (Hand.bStood || IsHandBust(Hand))
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
	if (!Seat || !Seat->Hands.IsValidIndex(Seat->ActiveHandIndex))
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
	if (!Seat || !Seat->Hands.IsValidIndex(Seat->ActiveHandIndex))
	{
		return false;
	}

	FBlackjackHand& Hand = Seat->Hands[Seat->ActiveHandIndex];
	if (Hand.Cards.Num() != 2 || Seat->BetAmount <= 0 || !Player->TrySpendCurrency(static_cast<float>(Seat->BetAmount)))
	{
		return false;
	}

	Seat->BetAmount *= 2;
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
	if (!Seat || Seat->bHasSplitThisRound || !CanSplitSeat(Seat->SeatIndex) || !Player->TrySpendCurrency(static_cast<float>(Seat->BetAmount)))
	{
		return false;
	}

	FBlackjackHand& FirstHand = Seat->Hands[0];
	FBlackjackHand SecondHand;
	SecondHand.bFromSplit = true;
	SecondHand.Cards.Add(FirstHand.Cards[1]);
	FirstHand.Cards.RemoveAt(1);
	FirstHand.bFromSplit = true;

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
	BroadcastSeat(Seat->SeatIndex);
	OnTableChanged.Broadcast();
	return true;
}

void ABlackjackTableActor::ResetRound()
{
	if (!HasAuthority())
	{
		return;
	}

	for (FBlackjackSeatState& Seat : Seats)
	{
		Seat.Hands.Reset();
		Seat.ActiveHandIndex = 0;
		Seat.BetAmount = 0;
		Seat.InsuranceBetAmount = 0;
		Seat.LastResult = EBlackjackSeatResult::None;
		Seat.bReadyForRound = false;
		Seat.bHasSplitThisRound = false;
		BroadcastSeat(Seat.SeatIndex);
	}

	DealerHand = FBlackjackHand();
	RoundState = EBlackjackRoundState::WaitingForPlayers;
	OnTableChanged.Broadcast();
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
	return DealerHand.Cards.Num() >= 1 && DealerHand.Cards[0].Rank == EBlackjackRank::Ace && RoundState == EBlackjackRoundState::PlayerTurns;
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

USceneComponent* ABlackjackTableActor::GetSeatCameraPoint(int32 SeatIndex) const
{
	return GetSeatPoint(SeatIndex);
}

UCameraComponent* ABlackjackTableActor::GetSeatCamera(int32 SeatIndex) const
{
	return nullptr;
}

void ABlackjackTableActor::ActivateSeatSelectCamera()
{
}

bool ABlackjackTableActor::ActivateSeatCamera(int32 SeatIndex)
{
	return IsValidSeatIndex(SeatIndex);
}

void ABlackjackTableActor::DeactivateTableCameras()
{
}

void ABlackjackTableActor::OnRep_TableState()
{
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
	DealerHand.Cards.Add(DrawCard(bFaceUp));
	OnDealerCardDealt.Broadcast(INDEX_NONE, DealerHand.Cards.Last());
}

void ABlackjackTableActor::AdvanceTurnAfterSeat(int32 SeatIndex)
{
	if (!IsValidSeatIndex(SeatIndex))
	{
		return;
	}

	FBlackjackSeatState& Seat = Seats[SeatIndex];
	if (Seat.Hands.IsValidIndex(Seat.ActiveHandIndex + 1))
	{
		++Seat.ActiveHandIndex;
	}

	BroadcastSeat(SeatIndex);

	if (AllActiveSeatsComplete())
	{
		RunDealerAndResolve();
	}
	else
	{
		OnTableChanged.Broadcast();
	}
}

void ABlackjackTableActor::RunDealerAndResolve()
{
	RoundState = EBlackjackRoundState::DealerTurn;

	for (FBlackjackCard& Card : DealerHand.Cards)
	{
		Card.bFaceUp = true;
	}

	while (!IsHandBust(DealerHand))
	{
		const int32 DealerValue = GetHandBestValue(DealerHand);
		if (DealerValue > 17 || (DealerValue == 17 && (bDealerStandsOnSoft17 || !IsSoft17(DealerHand))))
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
	const int32 DealerValue = GetHandBestValue(DealerHand);
	const bool bDealerBust = IsHandBust(DealerHand);
	const bool bDealerBlackjack = IsNaturalBlackjack(DealerHand);

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

		const FBlackjackHand& Hand = Seat.Hands[0];
		const int32 PlayerValue = GetHandBestValue(Hand);
		const bool bPlayerBlackjack = IsNaturalBlackjack(Hand);

		if (IsHandBust(Hand))
		{
			Seat.LastResult = EBlackjackSeatResult::PlayerBust;
		}
		else if (bPlayerBlackjack && !bDealerBlackjack)
		{
			Seat.LastResult = EBlackjackSeatResult::PlayerBlackjack;
			Player->AddCurrency(static_cast<float>(Seat.BetAmount + FMath::FloorToInt(Seat.BetAmount * 1.5f)));
		}
		else if (bDealerBlackjack && !bPlayerBlackjack)
		{
			Seat.LastResult = EBlackjackSeatResult::DealerWin;
		}
		else if (bDealerBust || PlayerValue > DealerValue)
		{
			Seat.LastResult = EBlackjackSeatResult::PlayerWin;
			Player->AddCurrency(static_cast<float>(Seat.BetAmount * 2));
		}
		else if (PlayerValue == DealerValue)
		{
			Seat.LastResult = EBlackjackSeatResult::Push;
			Player->AddCurrency(static_cast<float>(Seat.BetAmount));
		}
		else
		{
			Seat.LastResult = EBlackjackSeatResult::DealerWin;
		}

		BroadcastSeat(SeatIndex);
	}

	RoundState = EBlackjackRoundState::RoundComplete;
	OnTableChanged.Broadcast();
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
		|| RoundState == EBlackjackRoundState::PlayerTurns
		|| RoundState == EBlackjackRoundState::DealerTurn
		|| RoundState == EBlackjackRoundState::Resolving;
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
	bool bHasAceAsEleven = false;

	for (const FBlackjackCard& Card : Hand.Cards)
	{
		if (!Card.bFaceUp)
		{
			continue;
		}

		if (Card.Rank == EBlackjackRank::Ace)
		{
			Total += 11;
			bHasAceAsEleven = true;
		}
		else
		{
			Total += GetCardValue(Card);
		}
	}

	return Total == 17 && bHasAceAsEleven;
}
