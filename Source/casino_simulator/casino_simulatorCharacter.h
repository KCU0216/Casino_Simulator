// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
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
struct FInputActionValue;
class ARaceManager;
class ANPC_Dice;

/** A startup ability and the semantic input tag used to activate it (empty for passive/event abilities). */
USTRUCT(BlueprintType)
struct FStartupAbilityDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities", meta = (Categories = "Input"))
	FGameplayTag InputTag;
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class Acasino_simulatorCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

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

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

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

	/** Handles cigarette/alcohol shop purchases and forwards successful recovery to GAS */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shop", meta = (AllowPrivateAccess = "true"))
	UCasinoShopComponent* ShopComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldInteractionDetectorComponent> WorldInteractionDetector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBlackjackPlayerComponent> BlackjackPlayerComponent;

	/** Infinite periodic GameplayEffect (typically a Blueprint) that decays Nicotine/Alcohol over time. Applied once, server-side. */
	UPROPERTY(EditDefaultsOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> AttributeDecayEffectClass;

	/** Handle to the active decay effect, kept so it can be removed/reapplied later (e.g. to pause decay) */
	UPROPERTY(BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	FActiveGameplayEffectHandle AttributeDecayEffectHandle;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Machine|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ASeatedMachineBase> CurrentSeatedMachine;

	/** Walking speed at full Nicotine (ratio = 1). CharacterMovementComponent's MaxWalkSpeed is scaled from this as Nicotine depletes. */
	UPROPERTY(EditAnywhere, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	float MaxMoveSpeed = 600.0f;

	/** Jump launch speed at full Alcohol (ratio = 1). CharacterMovementComponent's JumpZVelocity is scaled from this as Alcohol depletes. */
	UPROPERTY(EditAnywhere, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	float MaxJumpSpeed = 420.0f;

public:
	Acasino_simulatorCharacter();

	//~ Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	/** Returns the attribute set holding nicotine/alcohol levels **/
	Ucasino_simulatorAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/** Returns the shop component used by shop/exchange UI blueprints **/
	UFUNCTION(BlueprintPure, Category="Shop")
	UCasinoShopComponent* GetShopComponent() const { return ShopComponent; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	UWorldInteractionDetectorComponent* GetWorldInteractionDetector() const { return WorldInteractionDetector; }

	UFUNCTION(BlueprintPure, Category="Blackjack")
	UBlackjackPlayerComponent* GetBlackjackPlayerComponent() const { return BlackjackPlayerComponent; }

	UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
	bool TrySpendCurrency(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
	void AddCurrency(float Amount);

	UFUNCTION(BlueprintPure, Category = "Economy|Currency")
	float GetCurrency() const;

	/** Grants an ability to this character's ASC. Authority-only; granted specs replicate to the owning client. */
	UFUNCTION(BlueprintCallable, Category="Abilities")
	FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level = 1, FGameplayTag InputTag = FGameplayTag());

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Race|Bet")
	void ServerBuyRaceTicket(ARaceManager* Manager, int32 RunnerIndex, int32 Amount, int32 Count);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Race|Bet")
	void ServerClaimRaceWinnings(ARaceManager* Manager);

	/** Forwards a dice game bet placed by this (locally-owned) character to the server, since a
	 * client can't call a Server RPC declared on DiceNPC directly (it isn't owned by that client). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Dice Game")
	void ServerPlaceDiceBet(ANPC_Dice* DiceNPC, int32 Select, int32 Betting);

	UFUNCTION(BlueprintPure, Category = "Machine|Interaction")
	ASeatedMachineBase* GetCurrentSeatedMachine() const { return CurrentSeatedMachine; }

	UFUNCTION(BlueprintPure, Category = "Machine|Interaction")
	bool IsUsingSeatedMachine() const { return CurrentSeatedMachine != nullptr; }

	void SetCurrentSeatedMachine(ASeatedMachineBase* NewMachine);
	void ClearCurrentSeatedMachine(ASeatedMachineBase* MachineToClear);

protected:

	//~ Begin AActor interface
	virtual void PossessedBy(AController* NewController) override;
	//~ End AActor interface

	//~ Begin APawn interface
	virtual void OnRep_PlayerState() override;
	//~ End APawn interface

	/** Applies InitialAttributesEffectClass to this character's own ability system. Server-only; call after InitAbilityActorInfo. */
	void InitializeDefaultAttributes() const;

	/** Grants all configured startup abilities. Server-only; call after InitAbilityActorInfo. */
	void GrantStartupAbilities();

	/** Applies AttributeDecayEffectClass to this character's own ability system so Nicotine/Alcohol decay over time. Server-only. */
	void ApplyAttributeDecayEffect();

	/** Subscribes UpdateMoveSpeedFromNicotine to the Nicotine/MaxNicotine attribute change delegates. Call after InitAbilityActorInfo, on every machine (not authority-only) since MaxWalkSpeed needs to match locally for movement prediction/simulation. */
	void BindMoveSpeedToNicotine();

	/** Rescales CharacterMovementComponent's MaxWalkSpeed to MaxMoveSpeed * (Nicotine / MaxNicotine). */
	void UpdateMoveSpeedFromNicotine() const;

	/** Subscribes UpdateJumpSpeedFromAlcohol to the Alcohol/MaxAlcohol attribute change delegates. Call after InitAbilityActorInfo, on every machine (not authority-only) since JumpZVelocity needs to match locally for movement prediction/simulation. */
	void BindJumpSpeedToAlcohol();

	/** Rescales CharacterMovementComponent's JumpZVelocity to MaxJumpSpeed * (Alcohol / MaxAlcohol). */
	void UpdateJumpSpeedFromAlcohol() const;

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

	void MachineExitInput();

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

