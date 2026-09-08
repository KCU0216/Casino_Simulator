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
	bool TryDrop(Acasino_simulatorCharacter* Character, FVector DropLocation);

	/** Releases this pickup and throws it in the client-proposed view direction. */
	UFUNCTION(BlueprintCallable, Category = "OrePickup")
	bool TryThrow(Acasino_simulatorCharacter* Character, FVector ThrowDirection);

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	float GetCarryMovementMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	UStaticMeshComponent* GetOrePickupMesh() const { return OrePickupMesh; }

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	EOreType GetOreType() const { return OreType; }

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	Acasino_simulatorCharacter* GetCarrier() const { return Carrier; }

	UFUNCTION(BlueprintPure, Category = "OrePickup")
	Acasino_simulatorCharacter* GetLastCarrier() const { return LastCarrier; }

	/** Local presentation hook when a player tries to pick up ore while their pickaxe is equipped. */
	UFUNCTION(BlueprintImplementableEvent, Category = "OrePickup|Presentation")
	void ReceivePickupBlocked(Acasino_simulatorCharacter* InteractingCharacter);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void UpdatePickupCollision();
	void UpdatePickupPhysics();
	void AttachToCarrier(Acasino_simulatorCharacter* Character);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OrePickup|Components")
	TObjectPtr<UStaticMeshComponent> OrePickupMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OrePickup")
	EOreType OreType = EOreType::Iron;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OrePickup")
	float Weight = 10.0f;

	/** Source of truth for whether this pickup is carried, and by whom. */
	UPROPERTY(ReplicatedUsing = OnRep_Carrier, VisibleInstanceOnly, BlueprintReadOnly, Category = "OrePickup")
	TObjectPtr<Acasino_simulatorCharacter> Carrier = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "OrePickup")
	TObjectPtr<Acasino_simulatorCharacter> LastCarrier = nullptr;

	/** Physical state is separate from ownership: newly spawned pickups are static, dropped pickups simulate. */
	UPROPERTY(ReplicatedUsing = OnRep_UsePhysics, VisibleInstanceOnly, BlueprintReadOnly, Category = "OrePickup")
	bool bUsePhysics = false;


	UFUNCTION()
	void OnRep_Carrier();

	UFUNCTION()
	void OnRep_UsePhysics();

public:
	

};
