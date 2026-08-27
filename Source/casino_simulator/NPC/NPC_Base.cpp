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

ANPC_Base::ANPC_Base()
{
	// Pure overlap detection (interaction range, aggro range, etc.) - doesn't block movement/physics.
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(GetCapsuleComponent());
	InteractionSphere->InitSphereRadius(150.0f);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

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
}

void ANPC_Base::GrantStartupAbilities()
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		GrantAbility(AbilityClass);
	}
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

void ANPC_Base::SetCanInterection(bool value)
{
	CanInterection = value;

	if (value && OverlappingPlayer)
	{
		Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(OverlappingPlayer->GetController());
		if (PC)
		{
			PC->OpenInteraction();
		}
	}
}

void ANPC_Base::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (!InteractingCharacter)
	{
		return;
	}

	BP_OnInteract(InteractingCharacter);
}

void ANPC_Base::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only the player character (Acasino_simulatorCharacter) opens the interaction - other NPCs/objects overlapping the sphere are ignored.
	if (Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor))
	{
		OverlappingPlayer = PlayerCharacter;

		if (Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(PlayerCharacter->GetController()))
		{
			PlayerController->SetInteractionTarget(this);
		}
	}
}

void ANPC_Base::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor))
	{
		if (OverlappingPlayer == PlayerCharacter)
		{
			OverlappingPlayer = nullptr;
		}

		if (Acasino_simulatorPlayerController* PlayerController = Cast<Acasino_simulatorPlayerController>(PlayerCharacter->GetController()))
		{
			PlayerController->ClearInteractionTarget(this);
		}
	}
}
