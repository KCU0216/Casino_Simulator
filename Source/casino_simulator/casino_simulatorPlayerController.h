// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/WorldInteractable.h"
#include "casino_loop_gamestate.h"
#include "UI/CasinoUIRoot.h"
#include "casino_simulatorPlayerController.generated.h"

class UCasinoUIManagerComponent;
class UInputMappingContext;
class UInputAction;
class UUserWidget;
class Ucasino_simulatorPlayerHUD;
class UAbilitySystemComponent;
class ANPC_Base;
class ASeatedMachineBase;
class UInventoryWidget;
class UPauseMenuWidget;
class UCasinoShopComponent;
class APoliceCharacter;
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

	friend class UCasinoUIManagerComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Casino|UI")
    TObjectPtr<UCasinoUIManagerComponent> UIManager;
    UFUNCTION(BlueprintPure, Category="Casino|UI")
    UCasinoUIManagerComponent* GetUIManager() const { return UIManager; }

	/** Constructor */
	Acasino_simulatorPlayerController();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|UI")
    TMap<ECasinoUIScreen, TSubclassOf<UUserWidget>> ScreenWidgetClasses;
    UPROPERTY(BlueprintReadOnly, Category="Casino|UI")
    TObjectPtr<UCasinoUIRoot> UIRoot;
    UPROPERTY(BlueprintReadOnly, Category="Casino|UI")
    TObjectPtr<UUserWidget> ActiveScreenWidget;
    UPROPERTY(BlueprintReadOnly, Category="Casino|UI")
    ECasinoUIScreen UIScreen = ECasinoUIScreen::Loading;
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    bool ShowInteractionUI(UUserWidget* Widget);
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    void CloseInteractionUI(UUserWidget* Widget);
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    bool ShowScreenUI(UUserWidget* Widget, ECasinoUIScreen ExpectedScreen);
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|UI")
    void OnUIScreenChanged(ECasinoUIScreen Screen, const FCasinoLoopStatus& Status);
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|UI")
    void OnUILoopStatusUpdated(const FCasinoLoopStatus& Status);
    // Unbind external delegates and clear BP references here. Widget-owned timers and latent actions are cancelled natively.
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|UI")
    void OnManagedWidgetClosed(UUserWidget* Widget);
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    void ReturnToMainMenu();
    UFUNCTION(BlueprintPure, Category="Casino|UI")
    bool CanRestartCasinoRun() const;
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Casino|UI")
    void ServerRestartCasinoRun();
    virtual void PlayerTick(float DeltaTime) override;
    virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;

	UFUNCTION(BlueprintPure, Category = "Police|Cinematic")
	bool IsPoliceCinematicActive() const
	{
		return bPoliceCinematicActive;
	}

	UFUNCTION(BlueprintImplementableEvent, Category = "Police|Cinematic")
	void OnPoliceArrivalRequested();
	UFUNCTION(BlueprintImplementableEvent, Category = "Police|Cinematic")
	void OnPoliceArrestRequested(APoliceCharacter* Police);

	UFUNCTION(BlueprintImplementableEvent, Category = "Police|Cinematic")
	void OnPoliceCinematicStopped();

	void BeginPoliceArrival();
	void BeginPoliceArrest(APoliceCharacter* Police);
	void EndPoliceCinematic();

private:
    void RefreshCasinoUIScreen();
    void EnsureUIRoot();
    void SetCasinoUIScreen(ECasinoUIScreen Screen);
    void CloseManagedWidget(UUserWidget* Widget);
    void ClearInteractionWidgets();
    void ApplyUIScreenInput();
	bool bPoliceCinematicActive = false;

	UFUNCTION(Client, Reliable)
	void ClientBeginPoliceArrival();

	UFUNCTION(Client, Reliable)
	void ClientBeginPoliceArrest(APoliceCharacter* Police);

	UFUNCTION(Client, Reliable)
	void ClientEndPoliceCinematic();
public:

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Casino|Shop")
    void ServerBuyShopItem(UCasinoShopComponent* Shop, FName ItemId, int32 Quantity);
    UFUNCTION(Client, Reliable)
    void ClientShopPurchaseResult(UCasinoShopComponent* Shop, FName ItemId, int32 Quantity, int32 TotalPrice, bool bSuccess, const FString& Reason);
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|Shop")
    void OnShopPurchaseResult(UCasinoShopComponent* Shop, FName ItemId, int32 Quantity, int32 TotalPrice, bool bSuccess, const FString& Reason);

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Casino|Lobby")
    void ServerSetLobbyReady(bool bReady);
    UFUNCTION(Client, Reliable)
    void ClientEnterCasinoMatch();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Casino|Loop")
    void ServerSubmitDailyPayment(int32 Amount);
    UFUNCTION(Client, Reliable)
    void ClientPrepareDailyPayment(FRotator Facing);
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|Loop")
    void OnPrepareDailyPayment();
    // Called locally BEFORE payment UI is created. Keep the balance HUD; close transient game/shop UIs.
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|Loop")
    void OnCloseGameplayUIForPayment();
    UFUNCTION(Client, Reliable)
    void ClientPrepareCasinoDay(FRotator Facing);
    UFUNCTION(Client, Reliable)
    void ClientShowWorldEventAnnouncement(const FText& Message, float Duration);
    UFUNCTION(Client, Reliable)
    void ClientPrepareDayIntro(FRotator Facing);
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|Loop")
    void OnPrepareCasinoDay();
    UFUNCTION(Client, Reliable)
    void ClientDailyPaymentResult(bool bSuccess);
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|Loop")
    void OnDailyPaymentResult(bool bSuccess);
    // Close payment/waiting widgets and show the supplied shared outcome.
    UFUNCTION(Client, Reliable)
    void ClientFinishDailyPayment(ECasinoLoopPhase Phase);
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|Loop")
    void OnFinishDailyPayment(ECasinoLoopPhase Phase);

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

	UPROPERTY(EditAnywhere, Category = "Input|Input Actions")
	TObjectPtr<UInputAction> TogglePauseMenuAction;

	/** Inventory widget class to spawn (e.g. WBP_Inventory) */
	UPROPERTY(EditAnywhere, Category="Inventory")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditAnywhere, Category = "PauseMenu")
	TSubclassOf<UPauseMenuWidget> PauseMenuWidgetClass;

	/** Pointer to the spawned inventory widget, created lazily the first time it's toggled on */
	UPROPERTY()
	TObjectPtr<UInventoryWidget> InventoryWidget;

	UPROPERTY()
	TObjectPtr<UPauseMenuWidget> PauseMenuWidget;

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
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(AllowPrivateAccess="true"))
	TObjectPtr<Ucasino_simulatorPlayerHUD> PlayerHUDWidget;

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

	UPROPERTY(BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	bool bWorldInteractionTargetFocused = false;

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

	void TogglePauseMenuInput();

	/** Uses the existing DropOre ability when E is pressed while carrying ore. */
	bool TryDropCarriedOre(class Acasino_simulatorCharacter* PlayerCharacter);

	bool TryReleaseCarriedCart(class Acasino_simulatorCharacter* PlayerCharacter);

public:

	/** Shows the inventory widget if hidden, hides it if shown. Spawns it from InventoryWidgetClass on first use. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "PauseMenu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintPure, Category="Inventory")
	bool IsInventoryOpen() const;

	UFUNCTION(BlueprintPure, Category="PauseMenu")
	bool IsPauseMenuOpen() const;

	/** True while inventory, pause, or interaction UI owns normal gameplay input. */
	UFUNCTION(BlueprintPure, Category="Input")
	bool IsAnyGameplayUIOpen() const;

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void InteractWithCurrentTarget();

	UFUNCTION(BlueprintCallable, Category="Machine|Interaction")
	void ExitCurrentMachine();

	void RequestWorldInteraction(TScriptInterface<IWorldInteractable> Target);

	/** NPC counterpart to RequestWorldInteraction, called after the detector resolves an ANPC_Base
	 * as the focused target. Runs Interact() locally, forwards to the
	 * server via Server_InteractWithNPC on a client, and disables movement for non-Shop NPCs - same
	 * behavior this used to run from inline inside InteractWithCurrentTarget. */
	//void RequestNPCInteraction(ANPC_Base* Target);
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetIsInteractionUIOpen(bool Value);

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsInteractionUIOpen() const { return bInteractionUIOpen; }

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetInteractionPromptSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsInteractionPromptSuppressed() const { return bInteractionPromptSuppressed; }

	// TODO: Consolidate interaction prompt display into one state-driven refresh path.
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void EnterInteractionUIMode(AActor* CameraTarget, float BlendTime = 0.35f);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void ExitInteractionUIMode(float BlendTime = 0.25f);

 UFUNCTION(BlueprintCallable, Category="Interaction")
 void CloseCurrentInteraction();
 UFUNCTION(Server, Reliable)
 void Server_CloseCurrentInteraction(AActor* ExpectedTarget);
 UFUNCTION(Client, Reliable)
 void Client_CompleteInteractionClose(AActor* ExpectedTarget);
 bool IsDailyPaymentControlLocked() const { return bDailyPaymentControlLocked; }
private:
 bool bDailyPaymentControlLocked = false;
public:

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OpenInteraction();
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CloseInteraction();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OpenCarriedOreInteraction();
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OpenCarriedCartInteraction();

	/** World-type counterpart to SetInteractionTarget/ClearInteractionTarget - called from
	 * AWorldInteractableBase::OnInteractionFocusStarted/Ended_Implementation so machines/tables/props
	 * open the same PlayerHUDWidget panel NPCs do. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetWorldInteractionTargetFocused(bool bFocused);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void RefreshInventroy();

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestWorldInteraction(const TScriptInterface<IWorldInteractable>& Target);

	UFUNCTION(Server, Reliable)
	void Server_HandleMachinePrimaryInput(ASeatedMachineBase* Machine);

	UFUNCTION(Server, Reliable)
	void Server_ExitMachine(ASeatedMachineBase* Machine);
};
