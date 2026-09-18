// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackTableInteractionActor.h"

#include "Blackjack/BlackjackPlayerComponent.h"
#include "Blackjack/BlackjackTableActor.h"
#include "casino_simulatorCharacter.h"

ABlackjackTableInteractionActor::ABlackjackTableInteractionActor()
{
	InteractionPromptText = FText::FromString(TEXT("E Bet"));
}

void ABlackjackTableInteractionActor::BeginPlay()
{
	Super::BeginPlay();

	if (!BlackjackTable)
	{
		BlackjackTable = ResolveBlackjackTable();
	}

	if (bUseActionPromptText)
	{
		InteractionPromptText = GetActionPromptText();
	}
}

void ABlackjackTableInteractionActor::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	ABlackjackTableActor* Table = ResolveBlackjackTable();
	UBlackjackPlayerComponent* BlackjackPlayerComponent = GetPlayerBlackjackComponent(InteractingCharacter);
	if (!Table || !BlackjackPlayerComponent || !CanInteract(InteractingCharacter))
	{
		BP_OnBlackjackInteractionRejected(InteractingCharacter, Table, InteractionAction);
		return;
	}

	bool bHandled = false;
	switch (InteractionAction)
	{
	case EBlackjackTableInteractionAction::OpenBetting:
		bHandled = BlackjackPlayerComponent->NotifyBettingInteractionStarted();
		break;

	case EBlackjackTableInteractionAction::BetFixedAmount:
		BlackjackPlayerComponent->NotifyBettingInteractionStarted();
		bHandled = BlackjackPlayerComponent->PlaceBet(FixedBetAmount);
		break;

	case EBlackjackTableInteractionAction::SitOut:
		bHandled = BlackjackPlayerComponent->ToggleSitOut();
		break;

	case EBlackjackTableInteractionAction::Hit:
		bHandled = BlackjackPlayerComponent->Hit();
		break;

	case EBlackjackTableInteractionAction::Stand:
		bHandled = BlackjackPlayerComponent->Stand();
		break;

	case EBlackjackTableInteractionAction::DoubleDown:
		bHandled = BlackjackPlayerComponent->DoubleDown();
		break;

	case EBlackjackTableInteractionAction::Split:
		bHandled = BlackjackPlayerComponent->Split();
		break;

	case EBlackjackTableInteractionAction::ExitSeat:
		BlackjackPlayerComponent->RequestExitBlackjackSeat();
		bHandled = true;
		break;

	case EBlackjackTableInteractionAction::InspectCards:
		bHandled = true;
		break;

	case EBlackjackTableInteractionAction::PlaceInsurance:
		bHandled = BlackjackPlayerComponent->PlaceInsurance(FixedBetAmount);
		break;
	case EBlackjackTableInteractionAction::SkipInsurance:
		bHandled = BlackjackPlayerComponent->SkipInsurance();
		break;

	default:
		break;
	}

	if (bHandled)
	{
		BP_OnBlackjackInteractionPerformed(InteractingCharacter, Table, InteractionAction);
	}
	else
	{
		BP_OnBlackjackInteractionRejected(InteractingCharacter, Table, InteractionAction);
	}
}

bool ABlackjackTableInteractionActor::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	if (!Super::CanInteract(InteractingCharacter))
	{
		return false;
	}

	const ABlackjackTableActor* Table = ResolveBlackjackTable();
	const UBlackjackPlayerComponent* BlackjackPlayerComponent = GetPlayerBlackjackComponent(InteractingCharacter);
	if (!Table || !BlackjackPlayerComponent || BlackjackPlayerComponent->GetCurrentBlackjackTable() != Table)
	{
		return false;
	}

	const int32 PlayerSeat = Table->GetSeatIndexForPlayer(InteractingCharacter);
	if (PlayerSeat == INDEX_NONE || (AllowedSeatIndex != INDEX_NONE && AllowedSeatIndex != PlayerSeat))
	{
		return false;
	}
	switch (InteractionAction)
	{
	case EBlackjackTableInteractionAction::OpenBetting:
		return Table->CanOpenBetting(InteractingCharacter);
	case EBlackjackTableInteractionAction::BetFixedAmount:
		return Table->CanPlaceBet(InteractingCharacter, FixedBetAmount);
	case EBlackjackTableInteractionAction::SitOut:
		return Table->CanSitOut(InteractingCharacter);

	case EBlackjackTableInteractionAction::Hit:
		return Table->CanHit(InteractingCharacter);
	case EBlackjackTableInteractionAction::Stand:
		return Table->CanStand(InteractingCharacter);
	case EBlackjackTableInteractionAction::DoubleDown:
		return Table->CanDoubleDown(InteractingCharacter);
	case EBlackjackTableInteractionAction::Split:
		return Table->CanSplit(InteractingCharacter);
	case EBlackjackTableInteractionAction::PlaceInsurance:
		return Table->CanPlaceInsurance(InteractingCharacter, FixedBetAmount);
	case EBlackjackTableInteractionAction::SkipInsurance:
		return Table->CanSkipInsurance(InteractingCharacter);

	case EBlackjackTableInteractionAction::ExitSeat:
	case EBlackjackTableInteractionAction::InspectCards:
		return BlackjackPlayerComponent->IsInBlackjackSeat();

	default:
		return false;
	}
}

void ABlackjackTableInteractionActor::OnLocalInteract_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (CanInteract(InteractingCharacter))
	{
		BP_OnLocalBlackjackInteract(InteractingCharacter, ResolveBlackjackTable(), InteractionAction);
	}
}

ABlackjackTableActor* ABlackjackTableInteractionActor::GetBlackjackTable() const
{
	return ResolveBlackjackTable();
}

ABlackjackTableActor* ABlackjackTableInteractionActor::ResolveBlackjackTable() const
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

UBlackjackPlayerComponent* ABlackjackTableInteractionActor::GetPlayerBlackjackComponent(Acasino_simulatorCharacter* InteractingCharacter) const
{
	return InteractingCharacter ? InteractingCharacter->GetBlackjackPlayerComponent() : nullptr;
}

FText ABlackjackTableInteractionActor::GetActionPromptText() const
{
	switch (InteractionAction)
	{
	case EBlackjackTableInteractionAction::OpenBetting:
		return FText::FromString(TEXT("E Bet"));
	case EBlackjackTableInteractionAction::BetFixedAmount:
		return FText::Format(FText::FromString(TEXT("E Bet {0}")), FText::AsNumber(FixedBetAmount));
	case EBlackjackTableInteractionAction::SitOut:
		return FText::FromString(TEXT("E Sit Out"));
	case EBlackjackTableInteractionAction::Hit:
		return FText::FromString(TEXT("E Hit"));
	case EBlackjackTableInteractionAction::Stand:
		return FText::FromString(TEXT("E Stand"));
	case EBlackjackTableInteractionAction::DoubleDown:
		return FText::FromString(TEXT("E Double"));
	case EBlackjackTableInteractionAction::Split:
		return FText::FromString(TEXT("E Split"));
	case EBlackjackTableInteractionAction::ExitSeat:
		return FText::FromString(TEXT("E Leave"));
	case EBlackjackTableInteractionAction::InspectCards:
		return FText::FromString(TEXT("E Inspect"));
	case EBlackjackTableInteractionAction::PlaceInsurance:
		return FText::FromString(TEXT("E Insurance"));
	case EBlackjackTableInteractionAction::SkipInsurance:
		return FText::FromString(TEXT("E Skip Insurance"));
	default:
		return FText::FromString(TEXT("E Use"));
	}
}
