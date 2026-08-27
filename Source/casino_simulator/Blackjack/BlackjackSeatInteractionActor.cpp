// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackSeatInteractionActor.h"

#include "Blackjack/BlackjackTableActor.h"
#include "Components/SceneComponent.h"

ABlackjackSeatInteractionActor::ABlackjackSeatInteractionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	InteractionPromptText = FText::FromString(TEXT("E Sit"));
}

void ABlackjackSeatInteractionActor::BeginPlay()
{
	Super::BeginPlay();

	if (!BlackjackTable)
	{
		BlackjackTable = ResolveBlackjackTable();
	}

	SyncSeatIndexFromNearestSeatPoint();
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

bool ABlackjackSeatInteractionActor::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	if (!Super::CanInteract(InteractingCharacter))
	{
		return false;
	}

	ABlackjackTableActor* Table = ResolveBlackjackTable();
	return Table && Table->CanClaimSeat(InteractingCharacter, SeatIndex);
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

void ABlackjackSeatInteractionActor::SyncSeatIndexFromNearestSeatPoint()
{
	ABlackjackTableActor* Table = ResolveBlackjackTable();
	if (!Table)
	{
		return;
	}

	const FVector SeatActorLocation = GetActorLocation();
	int32 BestSeatIndex = INDEX_NONE;
	double BestDistanceSq = TNumericLimits<double>::Max();

	for (int32 Index = 0; Index < 4; ++Index)
	{
		USceneComponent* SeatPoint = Table->GetSeatPoint(Index);
		if (!SeatPoint)
		{
			continue;
		}

		const double DistanceSq = FVector::DistSquared(SeatActorLocation, SeatPoint->GetComponentLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestSeatIndex = Index;
		}
	}

	if (BestSeatIndex != INDEX_NONE)
	{
		SeatIndex = BestSeatIndex;
		BlackjackTable = Table;
	}
}
