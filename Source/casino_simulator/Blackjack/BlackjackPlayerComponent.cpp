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

	DOREPLIFETIME(UBlackjackPlayerComponent, SeatSession);

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

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerRequestExitBlackjackSeat();
		return;
	}

	ExecuteRequestExitBlackjackSeat();
}

void UBlackjackPlayerComponent::ExecuteRequestExitBlackjackSeat()
{
	if (!CanLeaveCurrentSeat())
	{
		ToggleLeaveAfterRound();
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

bool UBlackjackPlayerComponent::ToggleLeaveAfterRound()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteAction(EBlackjackRequestAction::ToggleLeaveAfterRound);
	}

	ServerToggleLeaveAfterRound();
	return true;
}

bool UBlackjackPlayerComponent::ToggleSitOut()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteAction(EBlackjackRequestAction::ToggleSitOut);
	}

	ServerToggleSitOut();
	return true;
}

bool UBlackjackPlayerComponent::IsLeaveAfterRoundRequested() const
{
	if (!CurrentBlackjackTable)
	{
		return false;
	}

	if (Acasino_simulatorCharacter* Character = GetOwnerCharacter())
	{
		return CurrentBlackjackTable->IsLeaveAfterRoundRequested(Character);
	}

	return false;
}

bool UBlackjackPlayerComponent::IsSitOutRequested() const
{
	if (!CurrentBlackjackTable)
	{
		return false;
	}

	if (Acasino_simulatorCharacter* Character = GetOwnerCharacter())
	{
		return CurrentBlackjackTable->IsSitOutRequested(Character);
	}

	return false;
}

bool UBlackjackPlayerComponent::PlaceBet(int32 Amount)
{
	if (!IsInBlackjackSeat() || Amount <= 0)
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteAction(EBlackjackRequestAction::PlaceBet, Amount);
	}

	ServerPlaceBet(Amount);
	return true;
}

bool UBlackjackPlayerComponent::NotifyBettingInteractionStarted()
{
	if (!IsInBlackjackSeat())
	{
		return false;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return ExecuteAction(EBlackjackRequestAction::NotifyBettingInteraction);
	}

	ServerNotifyBettingInteractionStarted();
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
		return ExecuteAction(EBlackjackRequestAction::StartRound);
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
		return ExecuteAction(EBlackjackRequestAction::Hit);
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
		return ExecuteAction(EBlackjackRequestAction::Stand);
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
		return ExecuteAction(EBlackjackRequestAction::DoubleDown);
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
		return ExecuteAction(EBlackjackRequestAction::Split);
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
		return ExecuteAction(EBlackjackRequestAction::PlaceInsurance, Amount);
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
		return ExecuteAction(EBlackjackRequestAction::SkipInsurance);
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

void UBlackjackPlayerComponent::ServerRequestExitBlackjackSeat_Implementation()
{
	ExecuteRequestExitBlackjackSeat();
}

void UBlackjackPlayerComponent::ServerCompleteExitBlackjackSeat_Implementation()
{
	CompleteExitBlackjackSeat();
}

void UBlackjackPlayerComponent::ServerToggleLeaveAfterRound_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::ToggleLeaveAfterRound);
}

void UBlackjackPlayerComponent::ServerToggleSitOut_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::ToggleSitOut);
}

void UBlackjackPlayerComponent::ServerPlaceBet_Implementation(int32 Amount)
{
	ExecuteAction(EBlackjackRequestAction::PlaceBet, Amount);
}

void UBlackjackPlayerComponent::ServerNotifyBettingInteractionStarted_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::NotifyBettingInteraction);
}

void UBlackjackPlayerComponent::ServerStartRound_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::StartRound);
}

void UBlackjackPlayerComponent::ServerHit_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::Hit);
}

void UBlackjackPlayerComponent::ServerStand_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::Stand);
}

void UBlackjackPlayerComponent::ServerDoubleDown_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::DoubleDown);
}

void UBlackjackPlayerComponent::ServerSplit_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::Split);
}

void UBlackjackPlayerComponent::ServerPlaceInsurance_Implementation(int32 Amount)
{
	ExecuteAction(EBlackjackRequestAction::PlaceInsurance, Amount);
}

void UBlackjackPlayerComponent::ServerSkipInsurance_Implementation()
{
	ExecuteAction(EBlackjackRequestAction::SkipInsurance);
}

void UBlackjackPlayerComponent::OnRep_BlackjackSeatMode()
{
	CurrentBlackjackTable = SeatSession.Table;
	CurrentSeatIndex = SeatSession.SeatIndex;
	RefreshSeatNotifications();
}

void UBlackjackPlayerComponent::RefreshSeatNotifications()
{
	if (NotifiedTable == CurrentBlackjackTable && NotifiedSeatIndex == CurrentSeatIndex) { return; }
	ABlackjackTableActor* PreviousTable = NotifiedTable;
	const int32 PreviousIndex = NotifiedSeatIndex;
	NotifiedTable = CurrentBlackjackTable;
	NotifiedSeatIndex = CurrentSeatIndex;
	if (PreviousTable && PreviousIndex != INDEX_NONE)
	{
		OnBlackjackSeatModeEnded.Broadcast(PreviousTable, PreviousIndex);
	}
	if (IsInBlackjackSeat())
	{
		ApplyMovementLock();
		OnBlackjackSeatModeStarted.Broadcast(CurrentBlackjackTable, CurrentSeatIndex);
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

bool UBlackjackPlayerComponent::ExecuteAction(EBlackjackRequestAction Action, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return false; }
	ABlackjackTableActor* RequestTable = CurrentBlackjackTable;
	EBlackjackRequestResult Result = EBlackjackRequestResult::Rejected;
	bool bAccepted = false;
	if (!IsInBlackjackSeat() || !GetOwnerCharacter()
		|| RequestTable->GetSeatIndexForPlayer(GetOwnerCharacter()) != CurrentSeatIndex)
	{
		Result = EBlackjackRequestResult::NotSeated;
	}
	else if ((Action == EBlackjackRequestAction::PlaceBet || Action == EBlackjackRequestAction::PlaceInsurance) && Amount <= 0)
	{
		Result = EBlackjackRequestResult::InvalidAmount;
	}
	else
	{
		switch (Action)
		{
		case EBlackjackRequestAction::PlaceBet: bAccepted = ExecutePlaceBet(Amount); break;
		case EBlackjackRequestAction::NotifyBettingInteraction: bAccepted = ExecuteNotifyBettingInteractionStarted(); break;
		case EBlackjackRequestAction::StartRound: bAccepted = ExecuteStartRound(); break;
		case EBlackjackRequestAction::Hit: bAccepted = ExecuteHit(); break;
		case EBlackjackRequestAction::Stand: bAccepted = ExecuteStand(); break;
		case EBlackjackRequestAction::DoubleDown: bAccepted = ExecuteDoubleDown(); break;
		case EBlackjackRequestAction::Split: bAccepted = ExecuteSplit(); break;
		case EBlackjackRequestAction::PlaceInsurance: bAccepted = ExecutePlaceInsurance(Amount); break;
		case EBlackjackRequestAction::SkipInsurance: bAccepted = ExecuteSkipInsurance(); break;
		case EBlackjackRequestAction::ToggleLeaveAfterRound: bAccepted = ExecuteToggleLeaveAfterRound(); break;
		case EBlackjackRequestAction::ToggleSitOut: bAccepted = ExecuteToggleSitOut(); break;
		default: break;
		}
		if (bAccepted) { Result = EBlackjackRequestResult::Success; }
	}
	ClientActionCompleted(RequestTable, Action, Result);
	return bAccepted;
}

void UBlackjackPlayerComponent::ClientActionCompleted_Implementation(ABlackjackTableActor* Table,
	EBlackjackRequestAction Action, EBlackjackRequestResult Result)
{
	OnActionCompleted.Broadcast(Table, Action, Result);
}

void UBlackjackPlayerComponent::SetBlackjackSeatMode(ABlackjackTableActor* Table, int32 SeatIndex)
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !Table || SeatIndex == INDEX_NONE || Table->GetSeatIndexForPlayer(Character) != SeatIndex
		|| (IsInBlackjackSeat() && (CurrentBlackjackTable != Table || CurrentSeatIndex != SeatIndex)))
	{
		return;
	}

	CurrentBlackjackTable = Table;
	CurrentSeatIndex = SeatIndex;
	SeatSession.Table = Table;
	SeatSession.SeatIndex = SeatIndex;
	RefreshSeatNotifications();
}

void UBlackjackPlayerComponent::ClearBlackjackSeatMode()
{
	CurrentBlackjackTable = nullptr;
	CurrentSeatIndex = INDEX_NONE;
	SeatSession = FBlackjackSeatSession();
	RefreshSeatNotifications();
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

	if (Acasino_simulatorCharacter* Character = GetOwnerCharacter())
	{
		return CurrentBlackjackTable->CanLeaveSeat(Character);
	}

	return false;
}

bool UBlackjackPlayerComponent::ExecuteToggleLeaveAfterRound()
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->ToggleLeaveAfterRound(Character);
}

bool UBlackjackPlayerComponent::ExecuteToggleSitOut()
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->ToggleSitOut(Character);
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

bool UBlackjackPlayerComponent::ExecuteNotifyBettingInteractionStarted()
{
	Acasino_simulatorCharacter* Character = GetOwnerCharacter();
	if (!Character || !CurrentBlackjackTable)
	{
		return false;
	}

	return CurrentBlackjackTable->NotifyBettingInteractionStarted(Character);
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
