#include "Interaction/InteractionSessionComponent.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"

UInteractionSessionComponent::UInteractionSessionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UInteractionSessionComponent::CanRestoreMovement(const Acasino_simulatorCharacter* Character)
{
	if (!IsValid(Character) || !Character->GetWorld())
	{
		return false;
	}

	const Acasino_simulatorPlayerController* PlayerController =
		Cast<Acasino_simulatorPlayerController>(Character->GetController());
	if (PlayerController && PlayerController->IsDailyPaymentControlLocked())
	{
		return false;
	}

	const ACasinoLoopGameState* GameState = Character->GetWorld()->GetGameState<ACasinoLoopGameState>();
	return !GameState ||
		GameState->LoopStatus.Phase == ECasinoLoopPhase::Waiting ||
		GameState->LoopStatus.Phase == ECasinoLoopPhase::Playing;
}

void UInteractionSessionComponent::RestoreMovementAfterUse(Acasino_simulatorCharacter* Character)
{
	if (!CanRestoreMovement(Character))
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
}

void UInteractionSessionComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInteractionSessionComponent, MaxUsers);
	DOREPLIFETIME(UInteractionSessionComponent, Users);
}

bool UInteractionSessionComponent::TryJoin(Acasino_simulatorCharacter* User)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(User))
	{
		return false;
	}

	if (ContainsUser(User) || !HasCapacity())
	{
		return false;
	}

	Users.Add(User);
	ForceOwnerNetUpdate();
	RefreshNotifications();
	return true;
}

bool UInteractionSessionComponent::TryLeave(Acasino_simulatorCharacter* User)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(User))
	{
		return false;
	}

	if (Users.RemoveSingle(User) == 0)
	{
		return false;
	}

	ForceOwnerNetUpdate();
	RefreshNotifications();
	return true;
}

bool UInteractionSessionComponent::ContainsUser(const Acasino_simulatorCharacter* User) const
{
	return User && Users.ContainsByPredicate([User](const TObjectPtr<Acasino_simulatorCharacter>& ExistingUser)
	{
		return ExistingUser == User;
	});
}

bool UInteractionSessionComponent::HasCapacity() const
{
	return MaxUsers == 0 || Users.Num() < MaxUsers;
}

void UInteractionSessionComponent::OnRep_Users()
{
	RefreshNotifications();
}

void UInteractionSessionComponent::RefreshNotifications()
{
	for (const TWeakObjectPtr<Acasino_simulatorCharacter>& PreviousUser : NotifiedUsers)
	{
		Acasino_simulatorCharacter* PreviousUserPtr = PreviousUser.Get();
		if (PreviousUserPtr && !ContainsUser(PreviousUserPtr))
		{
			OnUserLeft.Broadcast(PreviousUserPtr);
		}
	}

	for (Acasino_simulatorCharacter* User : Users)
	{
		const bool bWasAlreadyNotified = NotifiedUsers.ContainsByPredicate(
			[User](const TWeakObjectPtr<Acasino_simulatorCharacter>& PreviousUser)
			{
				return PreviousUser.Get() == User;
			});

		if (User && !bWasAlreadyNotified)
		{
			OnUserJoined.Broadcast(User);
		}
	}

	NotifiedUsers.Reset(Users.Num());
	for (Acasino_simulatorCharacter* User : Users)
	{
		NotifiedUsers.Add(User);
	}
}

void UInteractionSessionComponent::ForceOwnerNetUpdate() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}
