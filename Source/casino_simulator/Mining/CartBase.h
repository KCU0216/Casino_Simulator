#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/WorldInteractableBase.h"
#include "CartBase.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
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

	UFUNCTION(BlueprintImplementableEvent, Category = "Mining|Cart|Presentation")
	void ReceivePickupBlocked(Acasino_simulatorCharacter* InteractingCharacter);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Mining|Cart")
	Acasino_simulatorCharacter* GetCarrier() const { return Carrier.Get(); }

	UFUNCTION(BlueprintPure, Category = "Mining|Cart")
	UBoxComponent* GetCargoVolume() const { return CargoVolume.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Cart|Components")
	TObjectPtr<UStaticMeshComponent> CartMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mining|Cart|Components")
	TObjectPtr<UBoxComponent> CargoVolume;

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


	// Clearance above the traced ground, measured from the mesh's lowest bound (cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Step Assist", meta = (ClampMin = "0.0"))
	float MaxStepAssistHeight = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Step Assist", meta = (ClampMin = "0.0"))
	float StepAssistStrength = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Step Assist", meta = (ClampMin = "0.0"))
	float MaxStepAssistAcceleration = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Step Assist", meta = (ClampMin = "0.0"))
	float StepAssistBlockedSpeed = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Step Assist", meta = (ClampMin = "0.0"))
	float StepAssistBlockedDelay = 0.15f;

	// Soft tilt limit: corrective torque grows beyond this angle; rotation is never teleported.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Stability", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float MaxTiltAngle = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Stability", meta = (ClampMin = "0.0"))
	float TiltSpringStrength = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Stability", meta = (ClampMin = "0.0"))
	float TiltDampingStrength = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mining|Cart|Stability", meta = (ClampMin = "0.0"))
	float MaxTiltTorque = 100.f;

public:

	UFUNCTION(BlueprintCallable, Category = "Mining|Cart")
	bool TryCarry(Acasino_simulatorCharacter* Character);
	UFUNCTION(BlueprintCallable, Category = "Mining|Cart")
	bool TryRelease(Acasino_simulatorCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Mining|Cart")
	bool UpdateCarryTargetLocation(Acasino_simulatorCharacter* Character, FVector TargetLocation);

	UFUNCTION()
	void OnRep_Carrier();

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCartCarryAssistTest;
#endif
	void UpdateCargoLastCarrier();
	void ApplyStepAssist(float DeltaTime, const FVector& ToTarget);
	void ApplyTiltStabilization();
	float CalculateStepAssistAcceleration(float GroundZ, float BottomZ) const;
	FVector CalculateTiltTorque(const FVector& AngularVelocity) const;
	float CarryBlockedTime = 0.f;

};
