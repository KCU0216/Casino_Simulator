#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_MiningTargetData.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMiningTargetDataReceived, AActor*, HitActor);

/** Waits on the server for a mining target-data packet sent by the owning client. */
UCLASS()
class CASINO_SIMULATOR_API UAbilityTask_WaitMiningTargetData : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Wait Mining Target Data", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UAbilityTask_WaitMiningTargetData* WaitMiningTargetData(UGameplayAbility* OwningAbility, float MaxRange = 300.f);

	UPROPERTY(BlueprintAssignable)
	FMiningTargetDataReceived OnValidHit;

	virtual void Activate() override;

private:
	void OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);
	float MaximumRange = 300.f;
};

/** Traces from the locally controlled player's eyes and sends that hit result as GAS Target Data. */
UCLASS()
class CASINO_SIMULATOR_API UAbilityTask_SendMiningTargetData : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Send Mining Target Data", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UAbilityTask_SendMiningTargetData* SendMiningTargetData(UGameplayAbility* OwningAbility, float TraceRange = 300.f);

	virtual void Activate() override;

private:
	float Range = 300.f;
};
