#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldInteractionDetectorComponent.generated.h"

class Acasino_simulatorCharacter;
class AWorldInteractableBase;

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UWorldInteractionDetectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldInteractionDetectorComponent();

	UFUNCTION(BlueprintCallable, Category = "World Interaction")
	void RegisterCandidate(AWorldInteractableBase* Candidate);

	UFUNCTION(BlueprintCallable, Category = "World Interaction")
	void UnregisterCandidate(AWorldInteractableBase* Candidate);

	UFUNCTION(BlueprintCallable, Category = "World Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintPure, Category = "World Interaction")
	AWorldInteractableBase* GetFocusedTarget() const { return FocusedTarget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<Acasino_simulatorCharacter> OwnerCharacter;

	UPROPERTY()
	TArray<TObjectPtr<AWorldInteractableBase>> NearbyTargets;

	UPROPERTY()
	TObjectPtr<AWorldInteractableBase> FocusedTarget;

	bool bWorldPromptOpen = false;

	void UpdateFocusedTarget();
	void SetFocusedTarget(AWorldInteractableBase* NewFocusedTarget);
	void CloseWorldPrompt();
};
