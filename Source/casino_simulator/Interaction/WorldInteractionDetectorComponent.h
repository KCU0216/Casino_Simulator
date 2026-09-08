#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/WorldInteractable.h"
#include "WorldInteractionDetectorComponent.generated.h"

class Acasino_simulatorCharacter;

/**
 * Owned by Acasino_simulatorCharacter. Tracks every IWorldInteractable candidate currently overlapping
 * the character (registered/unregistered by each candidate's own overlap sphere - see
 * AWorldInteractableBase and ANPC_Base), resolves which one the player is actually aiming at via a
 * per-tick line trace, and routes E-press input (TryInteract) to it. One shared pipeline for both
 * world props/machines and NPCs - see IWorldInteractable's class comment.
 */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UWorldInteractionDetectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldInteractionDetectorComponent();

	UFUNCTION(BlueprintCallable, Category = "World Interaction")
	void RegisterCandidate(TScriptInterface<IWorldInteractable> Candidate);

	UFUNCTION(BlueprintCallable, Category = "World Interaction")
	void UnregisterCandidate(TScriptInterface<IWorldInteractable> Candidate);

	UFUNCTION(BlueprintCallable, Category = "World Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintPure, Category = "World Interaction")
	TScriptInterface<IWorldInteractable> GetFocusedTarget() const { return FocusedTarget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<Acasino_simulatorCharacter> OwnerCharacter;

	UPROPERTY()
	TArray<TScriptInterface<IWorldInteractable>> NearbyTargets;

	UPROPERTY()
	TScriptInterface<IWorldInteractable> FocusedTarget;

	void UpdateFocusedTarget();
	void SetFocusedTarget(const TScriptInterface<IWorldInteractable>& NewFocusedTarget);
};
