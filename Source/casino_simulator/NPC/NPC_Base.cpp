// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC_Base.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulator.h"
#include "Interaction/WorldInteractionCandidateComponent.h"
#include "Interaction/InteractionSessionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ANPC_Base::ANPC_Base()
{
	// Pure overlap detection (interaction range, aggro range, etc.) - doesn't block movement/physics.
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(GetCapsuleComponent());
	InteractionSphere->InitSphereRadius(150.0f);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	InteractionCandidateComponent = CreateDefaultSubobject<UWorldInteractionCandidateComponent>(TEXT("InteractionCandidateComponent"));
	InteractionSessionComponent = CreateDefaultSubobject<UInteractionSessionComponent>(TEXT("InteractionSessionComponent"));

	// Create the ability system component. Attributes/abilities/effects are replicated
	// via the ASC itself, so the actor doesn't need to replicate it separately.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Created as a subobject of this actor so the ASC (also owned by this actor) auto-discovers
	// it when InitAbilityActorInfo runs.
	AttributeSet = CreateDefaultSubobject<Ucasino_simulatorAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* ANPC_Base::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ANPC_Base::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ANPC_Base::OnInteractionSphereBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ANPC_Base::OnInteractionSphereEndOverlap);
	}

	// NPCs have no PlayerState to host the ASC, so this actor is both owner and avatar.
	// Unlike the player character, there's no controller-driven PossessedBy/OnRep_PlayerState
	// to hook, so BeginPlay is the single init point on every machine (server and clients).
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Abilities should only ever be granted on the authority - GiveAbility replicates the
		// resulting spec to clients on its own.
		if (HasAuthority())
		{
			GrantStartupAbilities();
		}
	}

	if (InteractionSessionComponent)
	{
		InteractionSessionComponent->OnUserJoined.AddUObject(
			this,
			&ANPC_Base::HandleSessionUserJoined);

		InteractionSessionComponent->OnUserLeft.AddUObject(
			this,
			&ANPC_Base::HandleSessionUserLeft);
	}

}

void ANPC_Base::GrantStartupAbilities()
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		GrantAbility(AbilityClass);
	}
}

void ANPC_Base::HandleMachineUseStarted(Acasino_simulatorCharacter* Character)
{
}

void ANPC_Base::ReleaseInteraction(
	Acasino_simulatorCharacter* Character)
{
	if (!HasAuthority() ||
		!IsValid(Character) ||
		!IsValid(InteractionSessionComponent) ||
		!InteractionSessionComponent->ContainsUser(Character))
	{
		return;
	}

	InteractionSessionComponent->TryLeave(Character);
}

void ANPC_Base::HandleMachineUseReleased(Acasino_simulatorCharacter* Character)
{
}

FGameplayAbilitySpecHandle ANPC_Base::GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
	if (!AbilitySystemComponent || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}

	if (!HasAuthority())
	{
		UE_LOG(Logcasino_simulator, Warning, TEXT("'%s' attempted to grant ability '%s' on a non-authority instance - ignored."), *GetNameSafe(this), *AbilityClass->GetName());
		return FGameplayAbilitySpecHandle();
	}

	const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, Level, INDEX_NONE, this));
	if (Handle.IsValid())
	{
		GrantedAbilityHandles.Add(Handle);
	}

	return Handle;
}

void ANPC_Base::SetIsAnimPlay(bool Value)
{
	IsAnimPlay = Value;
}

bool ANPC_Base::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{

	USphereComponent* Sphere = GetInteractionSphere();

	if (!IsValid(InteractingCharacter) ||
		!IsValid(Sphere) ||
		!IsValid(InteractionSessionComponent))
	{
		return false;
	}

	const float MaxDistance =
		Sphere->GetScaledSphereRadius() + 150.0f;

	const float DistanceSquared = FVector::DistSquared(
		InteractingCharacter->GetActorLocation(),
		GetActorLocation());
	const bool bInteractionStateAllowed =
		GetNPCType() == ENPCType::Shop || !IsAnimPlay;

	return DistanceSquared <= FMath::Square(MaxDistance) &&
		bInteractionStateAllowed &&
		InteractionSessionComponent->HasCapacity();
}

void ANPC_Base::Interact(
	Acasino_simulatorCharacter* InteractingCharacter)
{
	if (!HasAuthority() ||
		!IsValid(InteractingCharacter) ||
		!IsValid(InteractionSessionComponent))
	{
		return;
	}

	InteractionSessionComponent->TryJoin(InteractingCharacter);
}

void ANPC_Base::OnInteractionFocusStarted_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (InteractingCharacter == nullptr)
	{
		return;
	}
	Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController());
	if (PC == nullptr || PC->IsInteractionUIOpen())
	{
		return;
	}

	if (PC != nullptr && CanInteract(InteractingCharacter))
	{
		PC->SetWorldInteractionTargetFocused(true);
		//InteractingCharacter->SetCurrentSeatedMachine(this);
	}
}

void ANPC_Base::OnInteractionFocusEnded_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (InteractingCharacter == nullptr)
	{
		return;
	}

	Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(InteractingCharacter->GetController());
	if (PlayerController != nullptr)
	{
		PlayerController->SetWorldInteractionTargetFocused(false);
		//InteractingCharacter->SetCurrentSeatedMachine(nullptr);
	}
}

void ANPC_Base::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only the player character (Acasino_simulatorCharacter) registers as a candidate - other
	// NPCs/objects overlapping the sphere are ignored. Registering here (rather than opening the
	// interaction UI directly, like before) hands "who's actually focused" off to the player's own
	// UWorldInteractionDetectorComponent, same as AWorldInteractableBase - see OnInteractionFocusStarted
	// above for where the UI actually opens once this NPC wins that resolution.
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor);
	if (PlayerCharacter == nullptr)
	{
		return;
	}
	
	if (!Players.Contains(PlayerCharacter))
	{
		Players.Add(PlayerCharacter);
	}

	InteractionCandidateComponent->RegisterOwnerAsCandidate(PlayerCharacter);
}

void ANPC_Base::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor);
	
	if (PlayerCharacter == nullptr)
	{
		return;
	}

	if (PlayerCharacter)
	{
		Players.Remove(PlayerCharacter);
	}

	InteractionCandidateComponent->UnregisterOwnerAsCandidate(PlayerCharacter);
}

void ANPC_Base::HandleSessionUserJoined(
	Acasino_simulatorCharacter* JoinedUser)
{
	if (!IsValid(JoinedUser))
	{
		return;
	}

	// 기존 코드 호환용 상태
	OverlappingPlayer = JoinedUser;
	JoinedUser->SetCurrentSeatedMachine(this);

	if (JoinedUser->IsLocallyControlled())
	{
		BP_OnInteract(JoinedUser);
	}

	HandleMachineUseStarted(JoinedUser);

	if (JoinedUser->HasAuthority())
	{
		if (UCharacterMovementComponent* Movement =
			JoinedUser->GetCharacterMovement())
		{
			Movement->DisableMovement();
		}
	}
}

void ANPC_Base::HandleSessionUserLeft(
	Acasino_simulatorCharacter* LeftUser)
{
	if (!IsValid(LeftUser) ||
		LeftUser->GetCurrentSeatedMachine().GetObject() != this)
	{
		return;
	}

	if (OverlappingPlayer == LeftUser)
	{
		const TArray<TObjectPtr<Acasino_simulatorCharacter>>& Users =
			InteractionSessionComponent->GetUsers();
		OverlappingPlayer = Users.IsEmpty() ? nullptr : Users[0].Get();
	}

	LeftUser->ClearCurrentSeatedMachine(this);
	HandleMachineUseReleased(LeftUser);
	UInteractionSessionComponent::RestoreMovementAfterUse(LeftUser);
}
