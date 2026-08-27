// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "casino_simulatorPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class Ucasino_simulatorPlayerHUD;
class UWorldInteractionPromptWidget;
class UAbilitySystemComponent;
class ANPC_Base;
class ASeatedMachineBase;
class AWorldInteractableBase;
struct FOnAttributeChangeData;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class CASINO_SIMULATOR_API Acasino_simulatorPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	Acasino_simulatorPlayerController();

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Input Action bound to the "I" key (toggles the inventory UI) */
	UPROPERTY(EditAnywhere, Category="Input|Input Actions")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	/** Inventory widget class to spawn (e.g. WBP_Inventory) */
	UPROPERTY(EditAnywhere, Category="Inventory")
	TSubclassOf<UUserWidget> InventoryWidgetClass;

	/** Pointer to the spawned inventory widget, created lazily the first time it's toggled on */
	UPROPERTY()
	TObjectPtr<UUserWidget> InventoryWidget;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Player HUD widget class to spawn (nicotine/alcohol meters) */
	UPROPERTY(EditAnywhere, Category="HUD")
	TSubclassOf<Ucasino_simulatorPlayerHUD> PlayerHUDWidgetClass;

	/** Pointer to the spawned player HUD widget */
	UPROPERTY()
	TObjectPtr<Ucasino_simulatorPlayerHUD> PlayerHUDWidget;

	UPROPERTY(EditAnywhere, Category="HUD")
	TSubclassOf<UWorldInteractionPromptWidget> WorldInteractionPromptWidgetClass;

	UPROPERTY()
	TObjectPtr<UWorldInteractionPromptWidget> WorldInteractionPromptWidget;

	/** Ability system component we're currently listening to for attribute changes, so we can unbind cleanly when the pawn changes */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Unbinds from any ability system component we're still listening to */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Re-checks/rebinds once PlayerState actually replicates in — handles the case where it's
	 *  still null at BeginPlay on remote clients */
	virtual void OnRep_PlayerState() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Re-binds to the newly possessed pawn's ability system whenever it changes */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

	/**
	 * Creates and adds PlayerHUDWidget once the PlayerState has actually replicated in, then binds
	 * to it. No-ops if the HUD already exists or PlayerState isn't valid yet (e.g. still null on a
	 * remote client at BeginPlay) - safe to call from both BeginPlay and OnRep_PlayerState.
	 * Deliberately creating the widget only once PlayerState is ready means Blueprint (Construct)
	 * can read PlayerState immediately instead of racing its replication.
	 */
	void TryInitializePlayerHUD();

	/** (Re)binds to the given PlayerState's OnInventoryChanged and immediately refreshes the HUD slots */
	void BindToPlayerState(class Acasino_simulatorPlayerState* NewPlayerState);

	/** Pushes current NumberSlots item counts into the HUD; safe to call anytime (handles null PlayerState/HUD/short NumberSlots) */
	UFUNCTION()
	void RefreshInventorySlotCounts();

	/** PlayerState we're currently subscribed to, so we can unbind cleanly when it changes */
	UPROPERTY()
	TObjectPtr<class Acasino_simulatorPlayerState> BoundPlayerState;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ANPC_Base> CurrentInteractionTarget;

	/** True while an interaction UI (shop/dialogue/exchange, etc.) owns input. */
	UPROPERTY(BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	bool bInteractionUIOpen = false;

	UPROPERTY(BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	bool bInteractionPromptSuppressed = false;

	bool bInteractionPawnMeshesHidden = false;

	bool bPreviousFirstPersonMeshVisibility = true;

	bool bPreviousWorldMeshVisibility = true;

	/** Subscribes to the given ability system's Nicotine/Alcohol attribute change delegates */
	void BindToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent);

	/** Unsubscribes from the currently bound ability system, if any */
	void UnbindFromAbilitySystem();

	/** Pushes the bound ability system's current Nicotine/Alcohol/Currency values into the HUD; safe to call anytime (no-ops if either isn't ready yet) */
	void PushInitialAttributeValues();

	/** Called whenever the possessed pawn's Nicotine attribute changes */
	void OnNicotineChanged(const FOnAttributeChangeData& Data);

	/** Called whenever the possessed pawn's Alcohol attribute changes */
	void OnAlcoholChanged(const FOnAttributeChangeData& Data);

	/** Called whenever the possessed pawn's Currency attribute changes */
	void OnCurrencyChanged(const FOnAttributeChangeData& Data);

	/** Hides/restores only this local player's pawn meshes while an interaction camera is active. */
	void SetLocalPawnMeshesHiddenForInteraction(bool bShouldHide);

	/** Bound to ToggleInventoryAction; toggles the inventory widget on/off */
	void ToggleInventoryInput();

public:

	/** Shows the inventory widget if hidden, hides it if shown. Spawns it from InventoryWidgetClass on first use. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintPure, Category="Inventory")
	bool IsInventoryOpen() const;

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetInteractionTarget(ANPC_Base* NewInteractionTarget);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void ClearInteractionTarget(ANPC_Base* InteractionTargetToClear);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void InteractWithCurrentTarget();

	UFUNCTION(BlueprintCallable, Category="Machine|Interaction")
	void ExitCurrentMachine();

	void RequestWorldInteraction(AWorldInteractableBase* Target);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	bool OpenWorldInteraction(const FText& PromptText);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void CloseWorldInteraction();

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetWorldInteractionPromptControls(bool bPrimaryVisible, bool bExitVisible);

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsInteractionUIOpen() const { return bInteractionUIOpen; }

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetInteractionPromptSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsInteractionPromptSuppressed() const { return bInteractionPromptSuppressed; }

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void EnterInteractionUIMode(AActor* CameraTarget, float BlendTime = 0.35f);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void ExitInteractionUIMode(float BlendTime = 0.25f);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OpenInteraction();
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CloseInteraction();

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestWorldInteraction(AWorldInteractableBase* Target);

	UFUNCTION(Server, Reliable)
	void Server_HandleMachinePrimaryInput(ASeatedMachineBase* Machine);

	UFUNCTION(Server, Reliable)
	void Server_ExitMachine(ASeatedMachineBase* Machine);
};
