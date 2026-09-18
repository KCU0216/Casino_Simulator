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
#include "GameFramework/CharacterMovementComponent.h"	
#include "Interaction/WorldInteractionDetectorComponent.h"

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

void ANPC_Base::Server_RequestUseMachine_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	HandleMachineRequestUseMachine(RequestingCharacter);
	Multicast_MachineUseStarted(RequestingCharacter);

	if (UCharacterMovementComponent* MovementComponent = RequestingCharacter->GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}
}

void ANPC_Base::HandleMachineRequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter)
{
}

void ANPC_Base::Multicast_MachineUseStarted_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	OverlappingPlayer = RequestingCharacter;
	RequestingCharacter->SetCurrentSeatedMachine(this);

	if (OverlappingPlayer && OverlappingPlayer->IsLocallyControlled())
	{
		BP_OnInteract(OverlappingPlayer);
	}                                                                                                                                                                                                                                                                                                                                                                                                                                   	HandleMachineUseStarted(RequestingCharacter);
}

void ANPC_Base::HandleMachineUseStarted(Acasino_simulatorCharacter* Character)
{
}

void ANPC_Base::Multicast_MachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter)
{
	if (OverlappingPlayer && OverlappingPlayer == ReleasingCharacter)
	{
		OverlappingPlayer = nullptr;
		ReleasingCharacter->SetCurrentSeatedMachine(nullptr);

		HandleMachineUseReleased(ReleasingCharacter);
	}
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
	if (GetNPCType() == ENPCType::Shop)
	{
		return true;
	}
	else
	{
		return OverlappingPlayer == nullptr && IsAnimPlay == false;
	}
}

void ANPC_Base::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	// Interact() is now called both locally (by whichever machine the interacting player is on, for
	// immediate UI/cosmetic feedback) and on the server via RPC (to update authoritative NPC state,
	// e.g. NPC_Dice's InteractingPlayer). BP_OnInteract is the cosmetic half, so it should only ever
	// actually fire on the one machine where InteractingCharacter is locally controlled - otherwise a
	// listen server would also run it for the host when a remote client is the one who interacted.
	if (!InteractingCharacter)
	{
		return;
	}

	/*OverlappingPlayer = InteractingCharacter;
	InteractingCharacter->SetCurrentSeatedMachine(this);*/

	if (HasAuthority())
	{
		Server_RequestUseMachine_Implementation(InteractingCharacter);
		return;
	}

	Server_RequestUseMachine(InteractingCharacter);
}

void ANPC_Base::OnInteractionFocusStarted_Implementation(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (InteractingCharacter == nullptr)
	{
		return;
	}
	// 위젯이 떠있으면 막기
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

	if (NPCType == ENPCType::Shop)
	{
		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->RegisterCandidate(this);
		}
	}
	else
	{
		if (OverlappingPlayer == nullptr)
		{
			if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
			{
				Detector->RegisterCandidate(this);
			}
		}
	}
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

	if (OverlappingPlayer != nullptr && OverlappingPlayer == PlayerCharacter)
	{
		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->UnregisterCandidate(this);
		}
	}
}
