#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mining/OrePickupBase.h"
#include "Mining/OreTypes.h"
#include "OreBase.generated.h"

class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;
class AOrePickupBase;
class Acasino_simulatorCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOreDurabilityChanged, int32, NewDurability, int32, MaxDurability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOreDepleted);

/**
 * Server-authoritative base actor for mineable world resources.
 *
 * Create Blueprint children for each ore type and set their mesh, OreId,
 * and future reward data there. Durability defaults from OreId unless disabled.
 */

UCLASS(Abstract, Blueprintable)
class CASINO_SIMULATOR_API AOreBase : public AActor
{
	GENERATED_BODY()

public:
	AOreBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Applies one or more successful mining hits. Must be called on the server. */
	UFUNCTION(BlueprintCallable, Category="Ore|Mining")
	bool ApplyMiningHit(int32 Damage = 1);

	/** Applies a mining hit using the interacting character's current pickaxe power. Must be called on the server. */
	UFUNCTION(BlueprintCallable, Category="Ore|Mining")
	bool ApplyMiningHitFromCharacter(Acasino_simulatorCharacter* MiningCharacter);

	/** Applies a validated mining hit and multicasts its impact before this ore can be destroyed. */
	UFUNCTION(BlueprintCallable, Category="Ore|Mining")
	bool ApplyMiningHitFromCharacterAtHit(
		Acasino_simulatorCharacter* MiningCharacter,
		const FHitResult& HitResult);

	UFUNCTION(BlueprintPure, Category="Ore|Mining")
	bool IsDepleted() const { return CurrentDurability <= 0; }

	UFUNCTION(BlueprintPure, Category="Ore|Mining")
	int32 GetCurrentDurability() const { return CurrentDurability; }

	UFUNCTION(BlueprintPure, Category="Ore|Mining")
	int32 GetMaxDurability() const { return MaxDurability; }

	UFUNCTION(BlueprintPure, Category="Ore|Mining")
	int32 GetDefaultMaxDurabilityForOreType() const;

	UFUNCTION(BlueprintPure, Category="Ore|Identity")
	EOreType GetOreId() const { return OreId; }

	UFUNCTION(BlueprintPure, Category="Ore|Components")
	UStaticMeshComponent* GetOreMesh() const { return OreMesh; }

	UPROPERTY(BlueprintAssignable, Category="Ore|Events")
	FOreDurabilityChanged OnDurabilityChanged;

	UPROPERTY(BlueprintAssignable, Category="Ore|Events")
	FOreDepleted OnOreDepleted;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ore|Components")
	TObjectPtr<UStaticMeshComponent> OreMesh;

	/** Identifier used later for inventory rewards, e.g. IronOre or GoldOre. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ore|Identity")
	EOreType OreId = EOreType::Iron;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ore|Mining", meta=(ClampMin="1"))
	int32 MaxDurability = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ore|Mining")
	bool bUseOreTypeDefaultDurability = true;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentDurability, BlueprintReadOnly, Category="Ore|Mining")
	int32 CurrentDurability = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OrePickup")
	TSubclassOf<AOrePickupBase> OrePickupClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ore|Effects")
	TObjectPtr<UNiagaraSystem> MiningHitEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ore|Effects")
	TObjectPtr<USoundBase> MiningHitSound;

	UFUNCTION()
	void OnRep_CurrentDurability(int32 PreviousDurability);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMiningHitEffect(
		FVector_NetQuantize ImpactPoint,
		FVector_NetQuantizeNormal ImpactNormal);

	/** Blueprint hook for hit VFX, cracks, sound, and rewards. Runs on the server. */
	UFUNCTION(BlueprintImplementableEvent, Category="Ore|Events")
	void OnMiningHitApplied(int32 NewDurability);

	/** Blueprint hook before this ore actor is destroyed. Runs on the server. */
	UFUNCTION(BlueprintImplementableEvent, Category="Ore|Events")
	void ReceiveOreDepleted();

private:
	bool ApplyMiningHitInternal(int32 Damage, const FHitResult* HitResult);
};
