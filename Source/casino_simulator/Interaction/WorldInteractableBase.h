#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/WorldInteractable.h"
#include "WorldInteractableBase.generated.h"

class Acasino_simulatorCharacter;
class USceneComponent;
class USphereComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EWorldInteractionExecutionType : uint8
{
	ServerOnly,
	LocalPredicted
};

/**
 * Base actor for non-NPC world interactions such as machines, doors, and props.
 * It owns interaction range detection; child actors own the actual interaction result.
 *
 * Implements IWorldInteractable (shared with ANPC_Base's family) so UWorldInteractionDetectorComponent
 * can register/focus-resolve/interact with it through the same shared pipeline - see WorldInteractable.h.
 */
UCLASS(Abstract, Blueprintable)
class CASINO_SIMULATOR_API AWorldInteractableBase : public AActor, public IWorldInteractable
{
	GENERATED_BODY()

public:
	AWorldInteractableBase();

	//~ Begin IWorldInteractable interface
	UFUNCTION(BlueprintCallable, Category = "World Interaction")
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) override;

	UFUNCTION(BlueprintPure, Category = "World Interaction")
	virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const override;

	UFUNCTION(BlueprintPure, Category = "World Interaction")
	virtual FText GetInteractionPromptText() const override { return InteractionPromptText; }

	// OnLocalInteract/OnInteractionFocusStarted/OnInteractionFocusEnded are BlueprintNativeEvents owned
	// by the interface itself - only override the _Implementation here, never redeclare the plain
	// UFUNCTION (see [[unreal-interface-blueprintnativeevent-gotcha]]).
	virtual void OnLocalInteract_Implementation(Acasino_simulatorCharacter* InteractingCharacter) override;
	virtual void OnInteractionFocusStarted_Implementation(Acasino_simulatorCharacter* InteractingCharacter) override;
	virtual void OnInteractionFocusEnded_Implementation(Acasino_simulatorCharacter* InteractingCharacter) override;
	//~ End IWorldInteractable interface

	/** Selects whether this interaction begins through the server RPC or a locally predicted ability. */
	virtual EWorldInteractionExecutionType GetInteractionExecutionType() const
	{
		return EWorldInteractionExecutionType::ServerOnly;
	}

	/** Runs only on the initiating player's machine for LocalPredicted interactions. */
	virtual void BeginLocalInteraction(Acasino_simulatorCharacter* InteractingCharacter);

	UFUNCTION(BlueprintPure, Category = "World Interaction")
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Interaction|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Interaction|Components")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Interaction")
	FText InteractionPromptText;

	UFUNCTION()
	void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
