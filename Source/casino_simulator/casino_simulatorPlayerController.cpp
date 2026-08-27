// Copyright Epic Games, Inc. All Rights Reserved.


#include "casino_simulatorPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "casino_simulatorCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "casino_simulator.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/casino_simulatorPlayerHUD.h"
#include "UI/WorldInteractionPromptWidget.h"
#include "casino_simulatorPlayerState.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulatorCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interaction/WorldInteractionDetectorComponent.h"
#include "Interaction/WorldInteractableBase.h"
#include "Machine/SeatedMachineBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NPC/NPC_Base.h"

Acasino_simulatorPlayerController::Acasino_simulatorPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = Acasino_simulatorCameraManager::StaticClass();
}

void Acasino_simulatorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(Logcasino_simulator, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	if (IsLocalPlayerController() && WorldInteractionPromptWidgetClass)
	{
		WorldInteractionPromptWidget = CreateWidget<UWorldInteractionPromptWidget>(this, WorldInteractionPromptWidgetClass);
		if (WorldInteractionPromptWidget)
		{
			WorldInteractionPromptWidget->AddToPlayerScreen(120);
			WorldInteractionPromptWidget->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			UE_LOG(Logcasino_simulator, Error, TEXT("Could not spawn world interaction prompt widget."));
		}
	}

	// only spawn the player HUD on local player controllers
	if (IsLocalPlayerController() && PlayerHUDWidgetClass)
	{
		// keep the HUD bound to whichever pawn we currently/next possess
		OnPossessedPawnChanged.AddDynamic(this, &Acasino_simulatorPlayerController::HandlePossessedPawnChanged);

		if (APawn* CurrentPawn = GetPawn())
		{
			HandlePossessedPawnChanged(nullptr, CurrentPawn);
		}

		// PlayerState is already valid here on the server/listen host; on a remote client it may
		// still be null, in which case OnRep_PlayerState retries this once it replicates in.
		TryInitializePlayerHUD();
	}
}

void Acasino_simulatorPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAbilitySystem();
	BindToPlayerState(nullptr);

	Super::EndPlay(EndPlayReason);
}

void Acasino_simulatorPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// PlayerState may have been null at BeginPlay on remote clients; this fires once it actually arrives.
	if (PlayerHUDWidget)
	{
		BindToPlayerState(GetPlayerState<Acasino_simulatorPlayerState>());
	}
	else
	{
		TryInitializePlayerHUD();
	}
}

void Acasino_simulatorPlayerController::TryInitializePlayerHUD()
{
	if (PlayerHUDWidget || !PlayerHUDWidgetClass)
	{
		return;
	}

	Acasino_simulatorPlayerState* CurrentPlayerState = GetPlayerState<Acasino_simulatorPlayerState>();
	if (!CurrentPlayerState)
	{
		return;
	}

	PlayerHUDWidget = CreateWidget<Ucasino_simulatorPlayerHUD>(this, PlayerHUDWidgetClass);

	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->AddToPlayerScreen(100);

		BindToPlayerState(CurrentPlayerState);

		// The pawn/ability system may already have been bound (e.g. from BeginPlay) before the HUD
		// existed to receive them; push current values now instead of waiting for the next change.
		PushInitialAttributeValues();
	}
	else
	{
		UE_LOG(Logcasino_simulator, Error, TEXT("Could not spawn player HUD widget."));
	}
}

void Acasino_simulatorPlayerController::BindToPlayerState(Acasino_simulatorPlayerState* NewPlayerState)
{
	if (BoundPlayerState == NewPlayerState)
	{
		return;
	}

	if (BoundPlayerState)
	{
		BoundPlayerState->OnInventoryChanged.RemoveDynamic(this, &Acasino_simulatorPlayerController::RefreshInventorySlotCounts);
	}

	BoundPlayerState = NewPlayerState;

	if (BoundPlayerState)
	{
		BoundPlayerState->OnInventoryChanged.AddDynamic(this, &Acasino_simulatorPlayerController::RefreshInventorySlotCounts);
	}

	RefreshInventorySlotCounts();
}

void Acasino_simulatorPlayerController::RefreshInventorySlotCounts()
{
	if (!PlayerHUDWidget)
	{
		return;
	}

	if (BoundPlayerState && BoundPlayerState->NumberSlots.Num() >= 2)
	{
		PlayerHUDWidget->BP_Slot_1Count(BoundPlayerState->GetItemQuantity(BoundPlayerState->NumberSlots[0]));
		PlayerHUDWidget->BP_Slot_2Count(BoundPlayerState->GetItemQuantity(BoundPlayerState->NumberSlots[1]));
	}
	else
	{
		PlayerHUDWidget->BP_Slot_1Count(0);
		PlayerHUDWidget->BP_Slot_2Count(0);
	}

	// Also push the full inventory so Blueprint can rebuild/refresh dynamic item slot widgets (e.g. WBP_ItemSlot).
	PlayerHUDWidget->BP_InventoryUpdated(BoundPlayerState ? BoundPlayerState->GetInventory() : TArray<FInventoryEntry>());
}

void Acasino_simulatorPlayerController::HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
	UnbindFromAbilitySystem();

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(NewPawn))
	{
		BindToAbilitySystem(AbilitySystemInterface->GetAbilitySystemComponent());
	}
}

void Acasino_simulatorPlayerController::BindToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	BoundAbilitySystemComponent = AbilitySystemComponent;

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetNicotineAttribute())
		.AddUObject(this, &Acasino_simulatorPlayerController::OnNicotineChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetAlcoholAttribute())
		.AddUObject(this, &Acasino_simulatorPlayerController::OnAlcoholChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetCurrencyAttribute())
		.AddUObject(this, &Acasino_simulatorPlayerController::OnCurrencyChanged);

	// push the current values immediately so the bars/text aren't left stale until the next change
	PushInitialAttributeValues();
}

void Acasino_simulatorPlayerController::PushInitialAttributeValues()
{
	if (!PlayerHUDWidget || !BoundAbilitySystemComponent)
	{
		return;
	}

	if (const Ucasino_simulatorAttributeSet* Attributes = BoundAbilitySystemComponent->GetSet<Ucasino_simulatorAttributeSet>())
	{
		PlayerHUDWidget->BP_NicotineUpdated(Attributes->GetNicotine(), Attributes->GetMaxNicotine());
		PlayerHUDWidget->BP_AlcoholUpdated(Attributes->GetAlcohol(), Attributes->GetMaxAlcohol());
		PlayerHUDWidget->BP_CurrencyUpdated(Attributes->GetCurrency());
	}
}

void Acasino_simulatorPlayerController::UnbindFromAbilitySystem()
{
	if (BoundAbilitySystemComponent)
	{
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetNicotineAttribute()).RemoveAll(this);
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetAlcoholAttribute()).RemoveAll(this);
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Ucasino_simulatorAttributeSet::GetCurrencyAttribute()).RemoveAll(this);
		BoundAbilitySystemComponent = nullptr;
	}
}

void Acasino_simulatorPlayerController::OnNicotineChanged(const FOnAttributeChangeData& Data)
{
	if (PlayerHUDWidget && BoundAbilitySystemComponent)
	{
		if (const Ucasino_simulatorAttributeSet* Attributes = BoundAbilitySystemComponent->GetSet<Ucasino_simulatorAttributeSet>())
		{
			PlayerHUDWidget->BP_NicotineUpdated(Data.NewValue, Attributes->GetMaxNicotine());
		}
	}
}

void Acasino_simulatorPlayerController::OnAlcoholChanged(const FOnAttributeChangeData& Data)
{
	if (PlayerHUDWidget && BoundAbilitySystemComponent)
	{
		if (const Ucasino_simulatorAttributeSet* Attributes = BoundAbilitySystemComponent->GetSet<Ucasino_simulatorAttributeSet>())
		{
			PlayerHUDWidget->BP_AlcoholUpdated(Data.NewValue, Attributes->GetMaxAlcohol());
		}
	}
}

void Acasino_simulatorPlayerController::OnCurrencyChanged(const FOnAttributeChangeData& Data)
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->BP_CurrencyUpdated(Data.NewValue);
	}
}

void Acasino_simulatorPlayerController::SetInteractionTarget(ANPC_Base* NewInteractionTarget)
{
	if (!NewInteractionTarget)
	{
		return;
	}

	CurrentInteractionTarget = NewInteractionTarget;
	CloseWorldInteraction();
	OpenInteraction();
}

void Acasino_simulatorPlayerController::ClearInteractionTarget(ANPC_Base* InteractionTargetToClear)
{
	if (InteractionTargetToClear && CurrentInteractionTarget != InteractionTargetToClear)
	{
		return;
	}

	CurrentInteractionTarget = nullptr;
	CloseInteraction();
}

void Acasino_simulatorPlayerController::InteractWithCurrentTarget()
{
	if (bInteractionUIOpen)
	{
		return;
	}

	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	if (ASeatedMachineBase* CurrentMachine = PlayerCharacter->GetCurrentSeatedMachine())
	{
		if (HasAuthority())
		{
			CurrentMachine->HandleMachinePrimaryInput(PlayerCharacter);
		}
		else
		{
			Server_HandleMachinePrimaryInput(CurrentMachine);
		}
		return;
	}

	if (CurrentInteractionTarget && CurrentInteractionTarget->GetCanInterection())
	{
		FVector Location = PlayerCharacter->GetActorLocation();
		FVector ForwardLocation = PlayerCharacter->GetActorForwardVector() * 1000.f;
		FVector LineLocation = Location + ForwardLocation;

		FHitResult OutHit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(PlayerCharacter);

		GetWorld()->LineTraceSingleByChannel(OutHit, Location, LineLocation, ECollisionChannel::ECC_Visibility, Params);

		ANPC_Base* HitNPC = Cast<ANPC_Base>(OutHit.GetActor());

		if (HitNPC)
		{
			CloseInteraction();
			CurrentInteractionTarget->Interact(PlayerCharacter);
			if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement())
			{
				MovementComponent->DisableMovement();
			}
			return;
		}
	}

	if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
	{
		Detector->TryInteract();
	}
}

void Acasino_simulatorPlayerController::ExitCurrentMachine()
{
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	ASeatedMachineBase* CurrentMachine = PlayerCharacter->GetCurrentSeatedMachine();
	if (!CurrentMachine)
	{
		return;
	}

	if (HasAuthority())
	{
		CurrentMachine->RequestReleaseMachine(PlayerCharacter);
	}
	else
	{
		Server_ExitMachine(CurrentMachine);
	}
}

void Acasino_simulatorPlayerController::RequestWorldInteraction(AWorldInteractableBase* Target)
{
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(GetPawn());
	if (!PlayerCharacter || !Target || !Target->CanInteract(PlayerCharacter))
	{
		return;
	}

	CloseWorldInteraction();

	if (HasAuthority())
	{
		Target->Interact(PlayerCharacter);
	}
	else
	{
		Server_RequestWorldInteraction(Target);
	}
}

void Acasino_simulatorPlayerController::Server_RequestWorldInteraction_Implementation(AWorldInteractableBase* Target)
{
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(GetPawn());
	if (!PlayerCharacter || !Target || !Target->CanInteract(PlayerCharacter))
	{
		return;
	}

	Target->Interact(PlayerCharacter);
}

void Acasino_simulatorPlayerController::Server_HandleMachinePrimaryInput_Implementation(ASeatedMachineBase* Machine)
{
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(GetPawn());
	if (!PlayerCharacter || !Machine || PlayerCharacter->GetCurrentSeatedMachine() != Machine)
	{
		return;
	}

	Machine->HandleMachinePrimaryInput(PlayerCharacter);
}

void Acasino_simulatorPlayerController::Server_ExitMachine_Implementation(ASeatedMachineBase* Machine)
{
	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(GetPawn());
	if (!PlayerCharacter || !Machine || PlayerCharacter->GetCurrentSeatedMachine() != Machine)
	{
		return;
	}

	Machine->RequestReleaseMachine(PlayerCharacter);
}

void Acasino_simulatorPlayerController::SetInteractionPromptSuppressed(bool bSuppressed)
{
	if (bInteractionPromptSuppressed == bSuppressed)
	{
		return;
	}

	bInteractionPromptSuppressed = bSuppressed;

	if (bInteractionPromptSuppressed)
	{
		CloseInteraction();
		CloseWorldInteraction();
		return;
	}

	if (CurrentInteractionTarget && !bInteractionUIOpen)
	{
		OpenInteraction();
	}
}

void Acasino_simulatorPlayerController::EnterInteractionUIMode(AActor* CameraTarget, float BlendTime)
{
	if (bInteractionUIOpen)
	{
		return;
	}

	bInteractionUIOpen = true;
	CloseInteraction();

	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	SetLocalPawnMeshesHiddenForInteraction(true);

	if (CameraTarget)
	{
		SetViewTargetWithBlend(CameraTarget, BlendTime);
	}
}

void Acasino_simulatorPlayerController::ExitInteractionUIMode(float BlendTime)
{
	if (!bInteractionUIOpen)
	{
		return;
	}

	bInteractionUIOpen = false;

	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);

	if (APawn* ControlledPawn = GetPawn())
	{
		SetViewTargetWithBlend(ControlledPawn, BlendTime);
	}

	SetLocalPawnMeshesHiddenForInteraction(false);

	if (CurrentInteractionTarget)
	{
		OpenInteraction();
	}
}

void Acasino_simulatorPlayerController::SetLocalPawnMeshesHiddenForInteraction(bool bShouldHide)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	USkeletalMeshComponent* FirstPersonMesh = PlayerCharacter->GetFirstPersonMesh();
	USkeletalMeshComponent* WorldMesh = PlayerCharacter->GetMesh();

	if (bShouldHide)
	{
		if (bInteractionPawnMeshesHidden)
		{
			return;
		}

		bPreviousFirstPersonMeshVisibility = FirstPersonMesh ? FirstPersonMesh->IsVisible() : true;
		bPreviousWorldMeshVisibility = WorldMesh ? WorldMesh->IsVisible() : true;

		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(false, true);
		}

		if (WorldMesh)
		{
			WorldMesh->SetVisibility(false, true);
		}

		bInteractionPawnMeshesHidden = true;
		return;
	}

	if (!bInteractionPawnMeshesHidden)
	{
		return;
	}

	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(bPreviousFirstPersonMeshVisibility, true);
	}

	if (WorldMesh)
	{
		WorldMesh->SetVisibility(bPreviousWorldMeshVisibility, true);
	}

	bInteractionPawnMeshesHidden = false;
}

void Acasino_simulatorPlayerController::OpenInteraction()
{
	if (!CurrentInteractionTarget || bInteractionUIOpen || bInteractionPromptSuppressed || !CurrentInteractionTarget->GetCanInterection())
	{
		return;
	}

	// PlayerHUDWidget may still be null here: its creation now waits on PlayerState replicating in
	// (see TryInitializePlayerHUD), so there's a brief window on remote clients where it doesn't exist yet.

	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->BP_OpenInterection();
	}
}

void Acasino_simulatorPlayerController::CloseInteraction()
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->BP_CloseInterection();
	}
}

bool Acasino_simulatorPlayerController::OpenWorldInteraction(const FText& PromptText)
{
	if (bInteractionUIOpen || bInteractionPromptSuppressed || (CurrentInteractionTarget && CurrentInteractionTarget->GetCanInterection()))
	{
		return false;
	}

	if (const Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(GetPawn()))
	{
		if (PlayerCharacter->IsUsingSeatedMachine())
		{
			return false;
		}
	}

	if (!WorldInteractionPromptWidget && WorldInteractionPromptWidgetClass && IsLocalPlayerController())
	{
		WorldInteractionPromptWidget = CreateWidget<UWorldInteractionPromptWidget>(this, WorldInteractionPromptWidgetClass);
		if (WorldInteractionPromptWidget)
		{
			WorldInteractionPromptWidget->AddToPlayerScreen(120);
		}
	}

	if (!WorldInteractionPromptWidget)
	{
		return false;
	}

	WorldInteractionPromptWidget->BP_SetPromptText(PromptText);
	WorldInteractionPromptWidget->BP_SetPrimaryPromptVisible(true);
	WorldInteractionPromptWidget->BP_SetExitPromptVisible(false);
	WorldInteractionPromptWidget->SetVisibility(ESlateVisibility::Visible);
	return true;
}

void Acasino_simulatorPlayerController::CloseWorldInteraction()
{
	if (WorldInteractionPromptWidget)
	{
		WorldInteractionPromptWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void Acasino_simulatorPlayerController::SetWorldInteractionPromptControls(bool bPrimaryVisible, bool bExitVisible)
{
	if (!WorldInteractionPromptWidget && WorldInteractionPromptWidgetClass && IsLocalPlayerController())
	{
		WorldInteractionPromptWidget = CreateWidget<UWorldInteractionPromptWidget>(this, WorldInteractionPromptWidgetClass);
		if (WorldInteractionPromptWidget)
		{
			WorldInteractionPromptWidget->AddToPlayerScreen(120);
		}
	}

	if (!WorldInteractionPromptWidget)
	{
		return;
	}

	WorldInteractionPromptWidget->BP_SetPrimaryPromptVisible(bPrimaryVisible);
	WorldInteractionPromptWidget->BP_SetExitPromptVisible(bExitVisible);

	const bool bShouldShowPrompt = bPrimaryVisible || bExitVisible;
	WorldInteractionPromptWidget->SetVisibility(bShouldShowPrompt ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void Acasino_simulatorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ToggleInventoryAction)
		{
			EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &Acasino_simulatorPlayerController::ToggleInventoryInput);
		}
	}
}

void Acasino_simulatorPlayerController::ToggleInventoryInput()
{
	ToggleInventory();
}

void Acasino_simulatorPlayerController::ToggleInventory()
{
	if (!InventoryWidgetClass)
	{
		UE_LOG(Logcasino_simulator, Warning, TEXT("'%s' has no InventoryWidgetClass set - cannot toggle inventory."), *GetNameSafe(this));
		return;
	}

	if (!InventoryWidget)
	{
		InventoryWidget = CreateWidget<UUserWidget>(this, InventoryWidgetClass);
		if (InventoryWidget)
		{
			InventoryWidget->AddToViewport();
			SetShowMouseCursor(InventoryWidget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
			return;
		}
	}

	if (!InventoryWidget)
	{
		return;
	}

	if (InventoryWidget->GetVisibility() == ESlateVisibility::Collapsed)
	{
		InventoryWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetShowMouseCursor(InventoryWidget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
}
bool Acasino_simulatorPlayerController::IsInventoryOpen() const
{
	return InventoryWidget && InventoryWidget->IsInViewport();
}

bool Acasino_simulatorPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
