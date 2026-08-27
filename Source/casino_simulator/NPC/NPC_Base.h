// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/HitResult.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "NPC_Base.generated.h"

class USphereComponent;
class UPrimitiveComponent;
class Acasino_simulatorCharacter;
class UAbilitySystemComponent;
class Ucasino_simulatorAttributeSet;
class UGameplayAbility;

/** Identifies what kind of NPC this is (e.g. which minigame/interaction it hosts). */
UENUM(BlueprintType)
enum class ENPCType : uint8
{
	None UMETA(DisplayName="None"),
	Dice UMETA(DisplayName="Dice"),
	Shop UMETA(DisplayName="Shop"),
};

/**
 *  Base class for all NPCs. ACharacter with its own AbilitySystemComponent/AttributeSet (the
 *  NPC is both owner and avatar - there's no PlayerState to host the ASC) and a sphere
 *  collision for interaction/detection use.
 */
UCLASS(abstract)
class CASINO_SIMULATOR_API ANPC_Base : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Sphere collision used for interaction/detection (e.g. player walking into range). Attached to the capsule root. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* InteractionSphere;

protected:

	/** What kind of NPC this is. Editable per-instance in the level or per-Blueprint in defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (AllowPrivateAccess = "true"))
	ENPCType NPCType = ENPCType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	bool CanInterection = true;

	/** Ability system component driving this NPC's abilities/attributes/effects */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	UAbilitySystemComponent* AbilitySystemComponent;

	/** Attribute set holding this NPC's attributes (currency, nicotine/alcohol, etc.) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	Ucasino_simulatorAttributeSet* AttributeSet;

	/** Abilities (typically Blueprints) granted to this NPC's ASC once, server-side, on BeginPlay */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Handles for every ability granted so far (startup + runtime), kept so they can be looked up/removed later */
	UPROPERTY(BlueprintReadOnly, Category="Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	/** Player character currently overlapping the interaction sphere (null when none is). Set/cleared in OnInteractionSphereBeginOverlap/EndOverlap. */
	UPROPERTY(BlueprintReadOnly, Category="Interaction", meta = (AllowPrivateAccess = "true"))
	Acasino_simulatorCharacter* OverlappingPlayer = nullptr;

public:

	ANPC_Base();

	/** Returns the interaction/detection sphere component **/
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

	/** Returns what kind of NPC this is **/
	UFUNCTION(BlueprintPure, Category="NPC")
	ENPCType GetNPCType() const { return NPCType; }

	UFUNCTION(BlueprintPure, Category = "NPC")
	bool GetCanInterection() const { return CanInterection; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetCanInterection(bool value);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter);

	//~ Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	/** Returns the attribute set holding this NPC's attributes **/
	Ucasino_simulatorAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/** Grants a single ability to this NPC's ASC. Server-only (no-ops on clients); returns an invalid handle if it couldn't be granted. */
	UFUNCTION(BlueprintCallable, Category="Abilities")
	FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level = 1);

	/** Returns the player character currently overlapping the interaction sphere, or null if none is. */
	UFUNCTION(BlueprintPure, Category="Interaction")
	Acasino_simulatorCharacter* GetOverlappingPlayer() const { return OverlappingPlayer; }

protected:

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	//~ End AActor interface

	/** Bound to InteractionSphere's begin-overlap event */
	UFUNCTION()
	void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Bound to InteractionSphere's end-overlap event */
	UFUNCTION()
	void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(BlueprintImplementableEvent, Category="Interaction", meta=(DisplayName="On Interact"))
	void BP_OnInteract(Acasino_simulatorCharacter* InteractingCharacter);

	/** Grants every ability in StartupAbilities to this NPC's ASC. Server-only; call after InitAbilityActorInfo. */
	void GrantStartupAbilities();
};
