#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldInteractableBase.generated.h"

class Acasino_simulatorCharacter;
class USceneComponent;
class USphereComponent;
class UPrimitiveComponent;

/**
 * Base actor for non-NPC world interactions such as machines, doors, and props.
 * It owns interaction range detection; child actors own the actual interaction result.
 */
UCLASS(Abstract, Blueprintable)
class CASINO_SIMULATOR_API AWorldInteractableBase : public AActor
{
	GENERATED_BODY()

public:
	AWorldInteractableBase();

	UFUNCTION(BlueprintCallable, Category = "World Interaction")
	virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter);

	UFUNCTION(BlueprintPure, Category = "World Interaction")
	virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const;

	UFUNCTION(BlueprintPure, Category = "World Interaction")
	FText GetInteractionPromptText() const { return InteractionPromptText; }

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
