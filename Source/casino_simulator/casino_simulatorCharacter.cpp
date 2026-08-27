// Copyright Epic Games, Inc. All Rights Reserved.

#include "casino_simulatorCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Economy/CasinoShopComponent.h"
#include "Interaction/WorldInteractionDetectorComponent.h"
#include "Machine/SeatedMachineBase.h"
#include "casino_simulatorPlayerController.h"
#include "RaceGame/RaceManager.h"
#include "casino_simulatorPlayerState.h"
#include "casino_simulatorAttributeSet.h"
#include "Item/ItemData.h"
#include "casino_simulator.h"

Acasino_simulatorCharacter::Acasino_simulatorCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	// Create the ability system component. Attributes/abilities/effects are replicated
	// via the ASC itself, so the actor doesn't need to replicate it separately.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Created as a subobject of this actor so the ASC (also owned by this actor) auto-discovers
	// it when InitAbilityActorInfo runs.
	AttributeSet = CreateDefaultSubobject<Ucasino_simulatorAttributeSet>(TEXT("AttributeSet"));

	ShopComponent = CreateDefaultSubobject<UCasinoShopComponent>(TEXT("ShopComponent"));
	WorldInteractionDetector = CreateDefaultSubobject<UWorldInteractionDetectorComponent>(TEXT("WorldInteractionDetector"));
}

UAbilitySystemComponent* Acasino_simulatorCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

bool Acasino_simulatorCharacter::TrySpendCurrency(float Amount)
{
	if (Amount <= 0.0f || !AbilitySystemComponent)
	{
		return false;
	}

	const FGameplayAttribute CurrencyAttribute =
		Ucasino_simulatorAttributeSet::GetCurrencyAttribute();

	const float CurrentCurrency =
		AbilitySystemComponent->GetNumericAttribute(CurrencyAttribute);

	if (CurrentCurrency < Amount)
	{
		return false;
	}

	AbilitySystemComponent->ApplyModToAttribute(
		CurrencyAttribute,
		EGameplayModOp::Additive,
		-Amount
	);

	return true;
}

void Acasino_simulatorCharacter::AddCurrency(float Amount)
{
	if (Amount <= 0.0f || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->ApplyModToAttribute(
		Ucasino_simulatorAttributeSet::GetCurrencyAttribute(),
		EGameplayModOp::Additive,
		Amount
	);
	
}

float Acasino_simulatorCharacter::GetCurrency() const
{
	if (!AbilitySystemComponent)
	{
		return 0.0f;
	}

	return AbilitySystemComponent->GetNumericAttribute(
		Ucasino_simulatorAttributeSet::GetCurrencyAttribute()
	);
}

void Acasino_simulatorCharacter::SetCurrentSeatedMachine(ASeatedMachineBase* NewMachine)
{
	CurrentSeatedMachine = NewMachine;
}

void Acasino_simulatorCharacter::ClearCurrentSeatedMachine(ASeatedMachineBase* MachineToClear)
{
	if (!MachineToClear || CurrentSeatedMachine == MachineToClear)
	{
		CurrentSeatedMachine = nullptr;
	}
}

void Acasino_simulatorCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server-side init. The ASC lives on this character, so it is both owner and avatar.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Set starting attribute values. GameplayEffects should only ever be applied on the
		// authority - clients pick the result up through normal attribute replication.
		if (HasAuthority())
		{
			InitializeDefaultAttributes();
			ApplyAttributeDecayEffect();
		}

		// Every machine (server and each client) needs its own local MaxWalkSpeed/JumpZVelocity to
		// match, since movement prediction/simulation runs locally - Nicotine/Alcohol themselves
		// replicate, so this just needs to react to them wherever it's bound.
		BindMoveSpeedToNicotine();
		BindJumpSpeedToAlcohol();
	}
}

void Acasino_simulatorCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client-side init, mirrors PossessedBy for clients that receive the player state after possession.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		BindMoveSpeedToNicotine();
		BindJumpSpeedToAlcohol();
	}
}

void Acasino_simulatorCharacter::InitializeDefaultAttributes() const
{
	if (!AbilitySystemComponent || !InitialAttributesEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffectClass, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void Acasino_simulatorCharacter::ApplyAttributeDecayEffect()
{
	if (!AbilitySystemComponent || !AttributeDecayEffectClass)
	{
		UE_LOG(Logcasino_simulator, Warning, TEXT("'%s' could not apply attribute decay effect: %s missing."), *GetNameSafe(this), !AbilitySystemComponent ? TEXT("AbilitySystemComponent") : TEXT("AttributeDecayEffectClass"));
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(AttributeDecayEffectClass, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		AttributeDecayEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	UE_LOG(Logcasino_simulator, Log, TEXT("'%s' applied attribute decay effect (active: %s)."), *GetNameSafe(this), AttributeDecayEffectHandle.IsValid() ? TEXT("true") : TEXT("false"));
}

void Acasino_simulatorCharacter::BindMoveSpeedToNicotine()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetNicotineAttribute())
		.AddLambda([this](const FOnAttributeChangeData&) { UpdateMoveSpeedFromNicotine(); });
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetMaxNicotineAttribute())
		.AddLambda([this](const FOnAttributeChangeData&) { UpdateMoveSpeedFromNicotine(); });

	// Apply immediately so movement speed matches the current ratio without waiting for the next change.
	UpdateMoveSpeedFromNicotine();
}

void Acasino_simulatorCharacter::UpdateMoveSpeedFromNicotine() const
{
	if (!AttributeSet || !GetCharacterMovement())
	{
		return;
	}

	const float MaxNicotineValue = AttributeSet->GetMaxNicotine();
	const float Ratio = (MaxNicotineValue > 0.0f) ? FMath::Clamp(AttributeSet->GetNicotine() / MaxNicotineValue, 0.0f, 1.0f) : 1.0f;

	GetCharacterMovement()->MaxWalkSpeed = MaxMoveSpeed * Ratio;
}

void Acasino_simulatorCharacter::BindJumpSpeedToAlcohol()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetAlcoholAttribute())
		.AddLambda([this](const FOnAttributeChangeData&) { UpdateJumpSpeedFromAlcohol(); });
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetMaxAlcoholAttribute())
		.AddLambda([this](const FOnAttributeChangeData&) { UpdateJumpSpeedFromAlcohol(); });

	// Apply immediately so jump speed matches the current ratio without waiting for the next change.
	UpdateJumpSpeedFromAlcohol();
}

void Acasino_simulatorCharacter::UpdateJumpSpeedFromAlcohol() const
{
	if (!AttributeSet || !GetCharacterMovement())
	{
		return;
	}

	const float MaxAlcoholValue = AttributeSet->GetMaxAlcohol();
	const float Ratio = (MaxAlcoholValue > 0.0f) ? FMath::Clamp(AttributeSet->GetAlcohol() / MaxAlcoholValue, 0.0f, 1.0f) : 1.0f;

	GetCharacterMovement()->JumpZVelocity = MaxJumpSpeed * Ratio;
}

void Acasino_simulatorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &Acasino_simulatorCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &Acasino_simulatorCharacter::DoJumpEnd);

		// Interacting
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &Acasino_simulatorCharacter::InteractInput);
		}

		// Quick-use slots
		if (Slot1Action)
		{
			EnhancedInputComponent->BindAction(Slot1Action, ETriggerEvent::Started, this, &Acasino_simulatorCharacter::Slot1Input);
			UE_LOG(LogTemp, Log, TEXT("Bind"));
		}

		if (Slot2Action)
		{
			EnhancedInputComponent->BindAction(Slot2Action, ETriggerEvent::Started, this, &Acasino_simulatorCharacter::Slot2Input);
		}

		if (MachineExitAction)
		{
			EnhancedInputComponent->BindAction(MachineExitAction, ETriggerEvent::Started, this, &Acasino_simulatorCharacter::MachineExitInput);
		}

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &Acasino_simulatorCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &Acasino_simulatorCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &Acasino_simulatorCharacter::LookInput);
	}
	else
	{
		UE_LOG(Logcasino_simulator, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void Acasino_simulatorCharacter::SetMousePoint(bool value)
{
	Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(GetController());
	
	if (PC)
	{
		PC->SetShowMouseCursor(value);
	}
}


void Acasino_simulatorCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void Acasino_simulatorCharacter::LookInput(const FInputActionValue& Value)
{
	if (Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(GetController()))
	{
		if (PC->bShowMouseCursor == true)
		{
			return;
		}
	}
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void Acasino_simulatorCharacter::InteractInput(const FInputActionValue& Value)
{
	if (Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(GetController()))
	{
		PC->InteractWithCurrentTarget();
	}
}

void Acasino_simulatorCharacter::Slot1Input(const FInputActionValue& Value)
{
	Acasino_simulatorPlayerState* State = GetPlayerState<Acasino_simulatorPlayerState>();
	
	if (!State|| !State->NumberSlots.IsValidIndex(0))
	{
		return;
	}

	const int32 ItemID = State->NumberSlots[0];

	FItemData ItemData;
	if (!State->FindItemData(ItemID, ItemData) || State->GetItemQuantity(ItemID) <= 0)
	{
		return;
	}
	
	if (!ItemData.bConsumeOnUse || !ItemData.OnUseEffect || !AbilitySystemComponent)
	{
		return;
	}

	// GameplayEffects should only ever be applied on the authority (see PossessedBy/InitializeDefaultAttributes).
	// On a remote client this input is handled locally but won't have authority - a Server RPC would be
	// needed there to actually apply the effect; out of scope for this pass.
	if (!HasAuthority())
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(ItemData.OnUseEffect, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// ItemData.ItemCategory doubles as the SetByCaller tag the item's OnUseEffect modifier should be
	// configured to read (Magnitude Calculation Type = Set by Caller, Data Tag = that item's category).
	// EffectMagnitude is per-item (e.g. how much Nicotine/Alcohol this specific item restores), so it's
	// supplied here rather than baked into the GameplayEffect asset itself.
	SpecHandle.Data->SetSetByCallerMagnitude(ItemData.ItemCategory, ItemData.EffectMagnitude);

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	
	if(State)
	{
		State->RemoveItem(ItemID, 1);
	}
}

void Acasino_simulatorCharacter::Slot2Input(const FInputActionValue& Value)
{
	// TODO: quick-use the item in PlayerState's NumberSlots[1] once that use-item flow exists.
}

void Acasino_simulatorCharacter::MachineExitInput()
{
	if (Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(GetController()))
	{
		PC->ExitCurrentMachine();
	}
}

void Acasino_simulatorCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void Acasino_simulatorCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void Acasino_simulatorCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void Acasino_simulatorCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void Acasino_simulatorCharacter::ServerBuyRaceTicket_Implementation(ARaceManager* Manager, int32 RunnerIndex, int32 Amount, int32 Count)
{
	if (Manager) Manager->ServerBuyTicket(this, RunnerIndex, Amount, Count);
}

void Acasino_simulatorCharacter::ServerClaimRaceWinnings_Implementation(ARaceManager* Manager)
{
	if (Manager) Manager->ServerClaimWinnings(this);
}
