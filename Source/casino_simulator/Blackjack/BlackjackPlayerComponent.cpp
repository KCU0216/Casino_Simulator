// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackPlayerComponent.h"

#include "Blackjack/BlackjackTableActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"

UBlackjackPlayerComponent::UBlackjackPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UBlackjackPlayerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBlackjackPlayerComponent, CurrentBlackjackTable);
	DOREPLIFETIME(UBlackjackPlayerComponent, CurrentSeatIndex);
}

void UBlackjackPlayerComponent::EnterBlackjackSeatMode(ABlackjackTableActor* Table, int32 SeatIndex)
{
	if (!Table || SeatIndex == INDEX_NONE)
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SetBlackjackSeatMode(Table, SeatIndex);
		return;
	}

	ServerEnterBlackjackSeatMode(Table, SeatIndex);
}

void UBlackjackPlayerComponent::RequestExitBlackjackSeat()
{
	if (!IsInBlackjackSeat())
	{
		return;
	}

	if (OnBlackjackSeatExitRequested.IsBound())
	{
		OnBlackjackSeatExitRequested.Broadcast(CurrentBlackjackTable, CurrentSeatIndex);
		return;
	}

	CompleteExitBlackjackSeat();
}

void UBlackjackPlayerComponent::CompleteExitBlackjackSeat()
{
	if (!IsInBlackjackSeat())
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (!CanLeaveCurrentSeat())
		{
			return;
		}

		if (Acasino_simulatorCharacter* Character = GetOwnerCharacter())
		{
			if (CurrentBlackjackTable)
			{
				CurrentBlackjackTable->LeaveSeat(Character);
			}
		}

		ClearBlackjackSeatMode();
		return;
	}

	ServerCompleteExitBlackjackSeat();
}

bool UBlackjackPlayerComponent::PlaceBet(int32 Amount)
{
	if (!IsInBlackjackSeat() || Amount <= 0)
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecutePlaceBet(Amount);
	}

	ServerPlaceBet(Amount);
	return true;
}

bool UBlackjackPlayerComponent::StartRound()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteStartRound();
	}

	ServerStartRound();
	return true;
}

bool UBlackjackPlayerComponent::Hit()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteHit();
	}

	ServerHit();
	return true;
}

bool UBlackjackPlayerComponent::Stand()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteStand();
	}

	ServerStand();
	return true;
}

bool UBlackjackPlayerComponent::DoubleDown()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteDoubleDown();
	}

	ServerDoubleDown();
	return true;
}

bool UBlackjackPlayerComponent::Split()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteSplit();
	}

	ServerSplit();
	return true;
}

bool UBlackjackPlayerComponent::PlaceInsurance(int32 Amount)
{
	if (!IsInBlackjackSeat() || Amount <= 0)
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecutePlaceInsurance(Amount);
	}

	ServerPlaceInsurance(Amount);
	return true;
}

bool UBlackjackPlayerComponent::SkipInsurance()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteSkipInsurance();
	}

	ServerSkipInsurance();
	return true;
}

void UBlackjackPlayerComponent::ServerEnterBlackjackSeatMode_Implementation(ABlackjackTableActor* Table, int32 SeatIndex)
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !Table || Table->GetSeatIndexForPlayer(Character) != SeatIndex)
	{
		return;
	}

	SetBlackjackSeatMode(Table, SeatIndex);
}

void UBlackjackPlayerComponent::ServerCompleteExitBlackjackSeat_Implementation()
{
	CompleteExitBlackjackSeat();
}

void UBlackjackPlayerComponent::ServerPlaceBet_Implementation(int32 Amount)
{
	ExecutePlaceBet(Amount);
}

void UBlackjackPlayerComponent::ServerStartRound_Implementation()
{
	ExecuteStartRound();
}

void UBlackjackPlayerComponent::ServerHit_Implementation()
{
	ExecuteHit();
}

void UBlackjackPlayerComponent::ServerStand_Implementation()
{
	ExecuteStand();
}

void UBlackjackPlayerComponent::ServerDoubleDown_Implementation()
{
	ExecuteDoubleDown();
}

void UBlackjackPlayerComponent::ServerSplit_Implementation()
{
	ExecuteSplit();
}

void UBlackjackPlayerComponent::ServerPlaceInsurance_Implementation(int32 Amount)
{
	ExecutePlaceInsurance(Amount);
}

void UBlackjackPlayerComponent::ServerSkipInsurance_Implementation()
{
	ExecuteSkipInsurance();
}

void UBlackjackPlayerComponent::OnRep_BlackjackSeatMode()
{
	if (IsInBlackjackSeat())
	{
		ApplyMovementLock();
	}
	else
	{
		ClearMovementLock();
	}
}

Acasino_simulatorCharacter* UBlackjackPlayerComponent::GetOwnerCharacter() const
{
	return Cast<Acasino_simulatorCharacter>(GetOwner());
}

void UBlackjackPlayerComponent::SetBlackjackSeatMode(ABlackjackTableActor* Table, int32 SeatIndex)
{
	if (!Table || SeatIndex == INDEX_NONE)
	{
		return;
	}

	CurrentBlackjackTable = Table;
	CurrentSeatIndex = SeatIndex;
	ApplyMovementLock();
	OnBlackjackSeatModeStarted.Broadcast(CurrentBlackjackTable, CurrentSeatIndex);
}

void UBlackjackPlayerComponent::ClearBlackjackSeatMode()
{
	ABlackjackTableActor* PreviousTable = CurrentBlackjackTable;
	const int32 PreviousSeatIndex = CurrentSeatIndex;

	CurrentBlackjackTable = nullptr;
	CurrentSeatIndex = INDEX_NONE;
	ClearMovementLock();

	if (PreviousTable && PreviousSeatIndex != INDEX_NONE)
	{
		OnBlackjackSeatModeEnded.Broadcast(PreviousTable, PreviousSeatIndex);
	}
}

void UBlackjackPlayerComponent::ApplyMovementLock()
{
	if (bMovementLockApplied)
	{
		return;
	}

	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(true);
	}

	bMovementLockApplied = true;
}

void UBlackjackPlayerComponent::ClearMovementLock()
{
	if (!bMovementLockApplied)
	{
		return;
	}

	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		bMovementLockApplied = false;
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(false);
	}

	bMovementLockApplied = false;
}

bool UBlackjackPlayerComponent::CanLeaveCurrentSeat() const
{
	if (!CurrentBlackjackTable)
	{
		return true;
	}

	const EBlackjackRoundState RoundState = CurrentBlackjackTable->GetRoundState();
	return RoundState == EBlackjackRoundState::WaitingForPlayers
		|| RoundState == EBlackjackRoundState::Betting
		|| RoundState == EBlackjackRoundState::RoundComplete;
}

bool UBlackjackPlayerComponent::ExecutePlaceBet(int32 Amount)
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable || Amount <= 0)
	{
		return false;
	}

	return CurrentBlackjackTable->PlaceBet(Character, Amount);
}

bool UBlackjackPlayerComponent::ExecuteStartRound()
{
	if (!CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->StartRound();
}

bool UBlackjackPlayerComponent::ExecuteHit()
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->PlayerHit(Character);
}

bool UBlackjackPlayerComponent::ExecuteStand()
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->PlayerStand(Character);
}

bool UBlackjackPlayerComponent::ExecuteDoubleDown()
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->PlayerDoubleDown(Character);
}

bool UBlackjackPlayerComponent::ExecuteSplit()
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->PlayerSplit(Character);
}

bool UBlackjackPlayerComponent::ExecutePlaceInsurance(int32 Amount)
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable || Amount <= 0)
	{
		return false;
	}

	return CurrentBlackjackTable->PlaceInsurance(Character, Amount);
}

bool UBlackjackPlayerComponent::ExecuteSkipInsurance()
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->SkipInsurance(Character);
}
