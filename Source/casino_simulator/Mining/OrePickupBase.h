// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mining/OreTypes.h"
#include "Interaction/WorldInteractableBase.h"
#include "OrePickupBase.generated.h"

class UStaticMeshComponent;
class Acasino_simulatorCharacter;

UCLASS()
class CASINO_SIMULATOR_API AOrePickupBase :  public AWorldInteractableBase
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOrePickupBase();

	virtual EWorldInteractionExecutionType GetInteractionExecutionType() const override
	{
		return EWorldInteractionExecutionType::LocalPredicted;
	}

	virtual void BeginLocalInteraction(Acasino_simulatorCharacter* InteractingCharacter) override;
	virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Claims this world pickup for Character. Called by the server-side pickup ability. */
	UFUNCTION(BlueprintCallable, Category = "OrePickup")
	bool TryPickUp(Acasino_simulatorCharacter* Character);

	/** Releases this pickup at the client-proposed DropLocation after server distance validation. */
	UFUNCTION(BlueprintCallable, Category = "OrePickup")
	bool TryDrop(Acasino_simulatorCharacter* Character);

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	float GetCarryMovementMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	UStaticMeshComponent* GetOrePickupMesh() const { return OrePickupMesh; }

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	EOreType GetOreType() const { return OreType; }

	/** Local presentation hook when a player tries to pick up ore while their pickaxe is equipped. */
	UFUNCTION(BlueprintImplementableEvent, Category = "OrePickup|Presentation")
	void ReceivePickupBlocked(Acasino_simulatorCharacter* InteractingCharacter);

	UFUNCTION(BlueprintCallable, Category = "OrePickup")
	bool CanJoinCarry(Acasino_simulatorCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "OrePickup")
	bool CanMoveCarry();

	UFUNCTION(BlueprintCallable, Category = "OrePickup")
	bool UpdateCarryTargetLocation(Acasino_simulatorCharacter* Character, FVector TargetLocation);

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	bool IsBeingCarried() const { return Carriers.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	int32 GetCarrierCount() const { return Carriers.Num(); }

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	int32 GetMaxCarryCount() const { return MaxCarryCount; }

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	bool IsCarryFull() const { return Carriers.Num() >= MaxCarryCount; }

	UFUNCTION(BlueprintImplementableEvent, Category = "OrePickup|Presentation")
	void ReceiveCarrierCountChanged(
		int32 CurrentCarrierCount,
		int32 InMinCarryCount,
		int32 InMaxCarryCount
	);

	const TArray<TObjectPtr<Acasino_simulatorCharacter>>& GetSaleParticipants() const
	{
		return LastCarriers;
	}

protected:

	virtual void Tick(float DeltaTime) override;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OrePickup|Components")
	TObjectPtr<UStaticMeshComponent> OrePickupMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OrePickup")
	EOreType OreType = EOreType::Iron;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OrePickup")
	float Weight = 10.0f;
	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "OrePickup")
	int32 MinCarryCount = 1;
	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "OrePickup")
	int32 MaxCarryCount = 1;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "OrePickup")
	float CarryDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "OrePickup")
	float MaxCarryTargetDistance = 700.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "OrePickup")
	float CarrySpringStrength = 8000.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "OrePickup")
	float CarryDampingStrength = 1200.f;

	UPROPERTY(EditDefaultsOnly, BluePrintReadWrite, Category = "OrePickup")
	float MaxCarryForce = 50000.f;



	/** Source of truth for whether this pickup is carried, and by whom. */
	UPROPERTY(ReplicatedUsing = OnRep_Carriers, VisibleInstanceOnly, BlueprintReadOnly, Category = "OrePickup")
	TArray<TObjectPtr<Acasino_simulatorCharacter>> Carriers = {};

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "OrePickup")
	TArray<TObjectPtr<Acasino_simulatorCharacter>> LastCarriers = {};

	TMap<TWeakObjectPtr<Acasino_simulatorCharacter>, FVector> CarrierTargetLocations;

	UFUNCTION()
	void OnRep_Carriers();

public:
	

};
