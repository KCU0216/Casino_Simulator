#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/WorldInteractableBase.h"
#include "CartBase.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UPrimitiveComponent;
class Acasino_simulatorCharacter;

UCLASS(Blueprintable)
class CASINO_SIMULATOR_API ACartBase : public AWorldInteractableBase
{
	GENERATED_BODY()

public:
	ACartBase();

	virtual EWorldInteractionExecutionType GetInteractionExecutionType() const override
	{
		return EWorldInteractionExecutionType::LocalPredicted;
	}
	virtual void BeginLocalInteraction(Acasino_simulatorCharacter* InteractingCharacter) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Cart|Components")
	TObjectPtr<UStaticMeshComponent> CartMesh;

	UPROPERTY(ReplicatedUsing = OnRep_Carrier, VisibleInstanceOnly, BlueprintReadOnly, Category = "Mining|Cart")
	TObjectPtr<Acasino_simulatorCharacter> Carrier = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Cart")
	FVector CarrierTargetLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "Mining|Cart")
	float MaxCarryTargetDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "Mining|Cart")
	float CarrySpringStrength = 30000.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "Mining|Cart")
	float CarryDampingStrength = 800.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "Mining|Cart")
	float MaxCarryForce = 500000.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "Mining|Cart")
	float TurnTorqueStrength = 10.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "Mining|Cart")
	float TurnDampingStrength = 2.5f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "Mining|Cart")
	float MaxTurnTorque = 30.f;






public:

	UFUNCTION(BlueprintCallable, Category = "Mining|Cart")
	bool TryCarry(Acasino_simulatorCharacter* Character);
	UFUNCTION(BlueprintCallable, Category = "Mining|Cart")
	bool TryRelease(Acasino_simulatorCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Mining|Cart")
	bool UpdateCarryTargetLocation(Acasino_simulatorCharacter* Character, FVector TargetLocation);

	UFUNCTION()
	void OnRep_Carrier();

};
