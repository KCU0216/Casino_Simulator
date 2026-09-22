// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/HitResult.h"
#include "Logging/LogMacros.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Mining/MiningShopComponent.h"
#include "Interaction/WorldInteractable.h"
#include "casino_simulatorCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UAbilitySystemComponent;
class Ucasino_simulatorAbilitySystemComponent;
class Ucasino_simulatorAttributeSet;
class UCasinoShopComponent;
class UWorldInteractionDetectorComponent;
class UBlackjackPlayerComponent;
class UGameplayEffect;
class UGameplayAbility;
class ASeatedMachineBase;
class USphereComponent;
class UPrimitiveComponent;
struct FInputActionValue;
class ARaceManager;
class ANPC_Dice;
class AThreeCardPokerTableActor;
class AOrePickupBase;
class ACartBase;
class UWorldInteractionCandidateComponent;

/** A startup ability and the semantic input tag used to activate it (empty for passive/event abilities). */
USTRUCT(BlueprintType)
struct FStartupAbilityDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	FGameplayTag InputTag;
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class Acasino_simulatorCharacter : public ACharacter, public IAbilitySystemInterface, public IWorldInteractable
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

// Stat
protected:
	/** Walking speed at full Nicotine (ratio = 1). CharacterMovementComponent's MaxWalkSpeed is scaled from this as Nicotine depletes. */
	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	float MaxMoveSpeed = 600.0f;

	/** Jump launch speed at full Alcohol (ratio = 1). CharacterMovementComponent's JumpZVelocity is scaled from this as Alcohol depletes. */
	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	float MaxJumpSpeed = 420.0f;

// Input Action
protected:
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* InteractAction;

	/** Slot 1 Input Action (quick-use item in PlayerState's NumberSlots[0]) */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* Slot1Action;

	/** Slot 2 Input Action (quick-use item in PlayerState's NumberSlots[1]) */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* Slot2Action;

	/** Machine Exit Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MachineExitAction;

	/** Equip pickaxe Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* EquipPickaxeAction;

	/** Mining Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MiningAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

// Ability System
protected:
	/** Ability system component driving this character's abilities/attributes/effects */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	Ucasino_simulatorAbilitySystemComponent* AbilitySystemComponent;

	/** Attribute set holding this character's nicotine/alcohol intoxication levels */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	Ucasino_simulatorAttributeSet* AttributeSet;

	/** GameplayEffect (typically a Blueprint) applied once, server-side, to set starting Nicotine/Alcohol values */
	UPROPERTY(EditDefaultsOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> InitialAttributesEffectClass;

	/** Abilities granted once after this character is possessed, optionally bound to a semantic input tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<FStartupAbilityDefinition> StartupAbilities;

	/** Handles of abilities granted to this character, retained for future lookup/removal. */
	UPROPERTY(BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	/** Prevents a repeated possession of the same pawn from granting duplicate startup abilities. */
	bool bStartupAbilitiesGranted = false;

	/** Prevents repeated possession or PlayerState replication from stacking movement delegates. */
	bool bMovementAttributeChangesBound = false;

	/** Infinite periodic GameplayEffect (typically a Blueprint) that decays Nicotine/Alcohol over time. Applied once, server-side. */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> AttributeDecayEffectClass;

	/** Handle to the active decay effect, kept so it can be removed/reapplied later (e.g. to pause decay) */
	UPROPERTY(BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	FActiveGameplayEffectHandle AttributeDecayEffectHandle;

// Game Machine
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackjack", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBlackjackPlayerComponent> BlackjackPlayerComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Machine|Interaction", meta = (AllowPrivateAccess = "true"))
	TScriptInterface<IWorldInteractable> CurrentSeatedMachine;

	/** Most recent IWorldInteractable this character actually interacted with (E-pressed and passed
	 * CanInteract), regardless of type - set from Acasino_simulatorPlayerController::RequestWorldInteraction
	 * and Server_RequestWorldInteraction_Implementation. Kept even after the target goes out of range,
	 * unlike WorldInteractionDetectorComponent's FocusedTarget which only tracks what's currently aimed at. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TScriptInterface<IWorldInteractable> LastInteractionTarget;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Three Card Poker", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AThreeCardPokerTableActor> CurrentThreeCardPokerTable;

// Component
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldInteractionDetectorComponent> WorldInteractionDetector;

	/** Overlap sphere other characters' WorldInteractionDetectorComponent uses to discover this
	 * character as an IWorldInteractable candidate (same pattern as ANPC_Base/AWorldInteractableBase's
	 * own InteractionSphere - see OnInteractionSphereBeginOverlap/EndOverlap below). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldInteractionCandidateComponent> InteractionCandidateComponent;

	/** The one ore pickup currently carried by this character. Set and cleared by the server-side pickup/drop flow. */
	UPROPERTY(ReplicatedUsing = OnRep_CarriedOre, VisibleInstanceOnly, BlueprintReadOnly, Category = "OrePickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AOrePickupBase> CarriedOre;

	UPROPERTY(ReplicatedUsing = OnRep_CarriedCart, VisibleInstanceOnly, BlueprintReadOnly, Category = "Cart", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACartBase> CarriedCart;

	// Function
public:
	Acasino_simulatorCharacter();

	/** Grants an ability to this character's ASC. Authority-only; granted specs replicate to the owning client. */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level = 1, FGameplayTag InputTag = FGameplayTag());

// Return Get
public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	Ucasino_simulatorAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	UWorldInteractionDetectorComponent* GetWorldInteractionDetector() const { return WorldInteractionDetector; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

	UFUNCTION(BlueprintPure, Category="Blackjack")
	UBlackjackPlayerComponent* GetBlackjackPlayerComponent() const { return BlackjackPlayerComponent; }

	UFUNCTION(BlueprintPure, Category = "Machine|Interaction")
	TScriptInterface<IWorldInteractable> GetCurrentSeatedMachine() const { return CurrentSeatedMachine; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	TScriptInterface<IWorldInteractable> GetLastInteractionTarget() const { return LastInteractionTarget; }

	UFUNCTION(BlueprintPure, Category = "Three Card Poker")
	AThreeCardPokerTableActor* GetCurrentThreeCardPokerTable() const { return CurrentThreeCardPokerTable; }

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	AOrePickupBase* GetCarriedOre() const { return CarriedOre; }

	UFUNCTION(BlueprintPure, Category = "Cart")
	ACartBase* GetCarriedCart() const { return CarriedCart; }

	UFUNCTION(BlueprintPure, Category = "Equipment|Pickaxe")
	int32 GetPickaxeMiningPower() const;

	UFUNCTION(BlueprintPure, Category = "Equipment|Pickaxe")
	float GetPickaxeMiningSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Equipment|Pickaxe")
	float GetPickaxeMiningMontagePlayRate() const;

// Currency
public:
	UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
	bool TrySpendCurrency(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
	void AddCurrency(float Amount);

	UFUNCTION(BlueprintPure, Category = "Economy|Currency")
	float GetCurrency() const;

// Use Currency In Game 
public:
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Dice Game")
	void ServerPlaceDiceBet(ANPC_Dice* DiceNPC, int32 Select, int32 Betting);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Race|Bet")
	void ServerBuyRaceTicket(ARaceManager* Manager, int32 RunnerIndex, int32 Amount, int32 Count);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Race|Bet")
	void ServerClaimRaceWinnings(ARaceManager* Manager);

// Mining
public:
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Mining|Shop")
	void ServerBuyMiningShopUpgrade(UMiningShopComponent* MiningShopComponent, EMiningShopUpgradeType UpgradeType);

	UFUNCTION(Client, Reliable, Category = "Mining|Shop")
	void ClientMiningShopPurchaseCompleted(UMiningShopComponent* MiningShopComponent, EMiningShopUpgradeType UpgradeType, int32 TotalPrice);

	UFUNCTION(Client, Reliable, Category = "Mining|Shop")
	void ClientMiningShopPurchaseFailed(UMiningShopComponent* MiningShopComponent, EMiningShopUpgradeType UpgradeType, const FString& Reason);

	UFUNCTION(Server, Unreliable, BlueprintCallable, Category = "Mining|Data")
	void ServerUpdateCarriedOreTargetLocation(FVector TargetLocation);

	UFUNCTION(Server, Unreliable, BlueprintCallable, Category = "Mining|Data")
	void ServerUpdateCarriedCartTargetLocation(FVector TargetLocation);

	/** Server-side state update used by AOrePickupBase after a successful pickup or drop. */
	void SetCarriedOre(AOrePickupBase* NewCarriedOre);

	/** Server-side state update used by ACartBase after a successful carry or release. */
	void SetCarriedCart(ACartBase* NewCarriedCart);

// Three Poker
public:
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Three Card Poker")
	void ServerPlaceThreeCardPokerPlay(AThreeCardPokerTableActor* Table, int32 AnteAmount, int32 PairBetAmount);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Three Card Poker")
	void ServerPlayThreeCardPokerHand(AThreeCardPokerTableActor* Table);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Three Card Poker")
	void ServerFoldThreeCardPokerHand(AThreeCardPokerTableActor* Table);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Three Card Poker")
	void ServerLeaveThreeCardPokerTable(AThreeCardPokerTableActor* Table);

	void SetCurrentSeatedMachine(TScriptInterface<IWorldInteractable> NewMachine);
	void ClearCurrentSeatedMachine(IWorldInteractable* MachineToClear);

	void SetLastInteractionTarget(TScriptInterface<IWorldInteractable> NewTarget) { LastInteractionTarget = NewTarget; }

	/** Lets Blueprint-owned equipment meshes restore their visibility after shared UI/camera flows. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Equipment")
	void RefreshEquipmentVisuals();

	//~ Begin IWorldInteractable interface
	/** Lets another player aim-and-E at this character. Interact() itself is intentionally a no-op -
	 * OnLocalInteract_Implementation below is what actually opens the trade widget, same pattern as
	 * AThreeCardPokerTableActor/ABlackjackTableInteractionActor. */
	virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const override;
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) override;
	virtual void OnLocalInteract_Implementation(Acasino_simulatorCharacter* InteractingCharacter) override;
	virtual void OnInteractionFocusStarted_Implementation(Acasino_simulatorCharacter* InteractingCharacter) override;
	virtual void OnInteractionFocusEnded_Implementation(Acasino_simulatorCharacter* InteractingCharacter) override;
	//~ End IWorldInteractable interface

	/** Fired from OnLocalInteract_Implementation, on the interacting player's own machine before the
	 * server even processes the interaction (see IWorldInteractable::OnLocalInteract) - lets the
	 * Blueprint character spawn/open its trade widget (e.g. WBP_Trade) immediately, same pattern as
	 * AThreeCardPokerTableActor::BP_OnLocalThreeCardPokerInteract. 'this' is the character being
	 * interacted with; InteractingCharacter is the player who pressed E. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Trade", meta = (DisplayName = "On Local Trade Interact"))
	void BP_OnLocalTradeInteract(Acasino_simulatorCharacter* InteractingCharacter);

protected:

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	//~ End AActor interface

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ Begin APawn interface
	virtual void OnRep_PlayerState() override;
	//~ End APawn interface

	/** Applies InitialAttributesEffectClass to this character's own ability system. Server-only; call after InitAbilityActorInfo. */
	void InitializeDefaultAttributes() const;

	/** Grants all configured startup abilities. Server-only; call after InitAbilityActorInfo. */
	void GrantStartupAbilities();

	/** Applies AttributeDecayEffectClass to this character's own ability system so Nicotine/Alcohol decay over time. Server-only. */
	void ApplyAttributeDecayEffect();

	void BindMovementAttributeChanges();

	void UpdateMovementFromAttributes() const;

	UFUNCTION()
	void OnRep_CarriedOre();
	UFUNCTION()
	void OnRep_CarriedCart();

	/** Bound to InteractionSphere's begin/end-overlap events - registers/unregisters this character as
	 * an IWorldInteractable candidate on the OTHER character's WorldInteractionDetectorComponent, same
	 * pattern as AWorldInteractableBase/ANPC_Base's own sphere. Each character owning its own sphere
	 * makes registration mutual: A's sphere overlapping B registers A on B's detector, and vice versa. */
	UFUNCTION()
	void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void UpdateCarriedOreInteractionPrompt() const;
	void UpdateCarriedCartInteractionPrompt() const;
	void HandleCarriedOreChanged() const;
	void HandleCarriedCartChanged() const;
	void StartOreCarryAbility() const;
	void StopOreCarryAbility() const;
	void StartCartCarryAbility() const;
	void StopCartCarryAbility() const;

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Called from Input Actions for interaction input */
	void InteractInput(const FInputActionValue& Value);

	/** Called from Input Actions for slot 1 input */
	void Slot1Input(const FInputActionValue& Value);

	/** Called from Input Actions for slot 2 input */
	void Slot2Input(const FInputActionValue& Value);

	/** Shared Slot1Input/Slot2Input handler: routes to the server if we're not the authority, then refreshes local inventory UI. */
	void UseNumberSlotItem(int32 SlotIndex);

	/** Server RPC: a remote client isn't the authority, so it can't apply GameplayEffects/modify PlayerState itself - this asks the server to do it. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory")
	void ServerUseNumberSlotItem(int32 SlotIndex);

	/** Server-authoritative: consumes the item in PlayerState's NumberSlots[SlotIndex] and applies its OnUseEffect. */
	void ApplyNumberSlotItemEffect(int32 SlotIndex);

	void MachineExitInput();

	void EquipPickaxeInputStarted();
	void EquipPickaxeInputCompleted();

	void MiningInputStarted();
	void MiningInputCompleted();

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:
	UFUNCTION(BlueprintCallable)
	void SetMousePoint(bool value);

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

