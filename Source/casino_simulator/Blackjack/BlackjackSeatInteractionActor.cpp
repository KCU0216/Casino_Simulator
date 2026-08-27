// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackSeatInteractionActor.h"

#include "Blackjack/BlackjackTableActor.h"

ABlackjackSeatInteractionActor::ABlackjackSeatInteractionActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABlackjackSeatInteractionActor::BeginPlay()
{
	Super::BeginPlay();

	if (!BlackjackTable)
	{
		BlackjackTable = ResolveBlackjackTable();
	}
}

void ABlackjackSeatInteractionActor::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (!InteractingCharacter)
	{
		BP_OnSeatClaimFailed(InteractingCharacter, EBlackjackSeatClaimResult::InvalidPlayer);
		return;
	}

	ABlackjackTableActor* Table = ResolveBlackjackTable();
	if (!Table)
	{
		BP_OnSeatClaimFailed(InteractingCharacter, EBlackjackSeatClaimResult::InvalidSeat);
		return;
	}

	const EBlackjackSeatClaimResult Result = Table->GetSeatClaimResult(InteractingCharacter, SeatIndex);
	if (Result != EBlackjackSeatClaimResult::Accepted)
	{
		BP_OnSeatClaimFailed(InteractingCharacter, Result);
		return;
	}

	if (!Table->TryClaimSeat(InteractingCharacter, SeatIndex))
	{
		BP_OnSeatClaimFailed(InteractingCharacter, EBlackjackSeatClaimResult::RequestFailed);
		return;
	}

	BP_OnSeatClaimSucceeded(InteractingCharacter, Table, SeatIndex);
}

ABlackjackTableActor* ABlackjackSeatInteractionActor::GetBlackjackTable() const
{
	return ResolveBlackjackTable();
}

EBlackjackSeatClaimResult ABlackjackSeatInteractionActor::GetCurrentClaimResult(Acasino_simulatorCharacter* InteractingCharacter) const
{
	if (!InteractingCharacter)
	{
		return EBlackjackSeatClaimResult::InvalidPlayer;
	}

	ABlackjackTableActor* Table = ResolveBlackjackTable();
	if (!Table)
	{
		return EBlackjackSeatClaimResult::InvalidSeat;
	}

	return Table->GetSeatClaimResult(InteractingCharacter, SeatIndex);
}

ABlackjackTableActor* ABlackjackSeatInteractionActor::ResolveBlackjackTable() const
{
	if (BlackjackTable)
	{
		return BlackjackTable;
	}

	if (ABlackjackTableActor* OwnerTable = Cast<ABlackjackTableActor>(GetOwner()))
	{
		return OwnerTable;
	}

	return Cast<ABlackjackTableActor>(GetAttachParentActor());
}
