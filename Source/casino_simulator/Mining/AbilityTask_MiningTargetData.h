#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_MiningTargetData.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMiningTargetDataReceived, AActor*, HitActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOreThrowDirectionReceived, FVector, ThrowDirection);

USTRUCT()
struct CASINO_SIMULATOR_API FGameplayAbilityTargetData_OreThrowDirection : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	FGameplayAbilityTargetData_OreThrowDirection() = default;

	explicit FGameplayAbilityTargetData_OreThrowDirection(const FVector& InThrowDirection)
		: ThrowDirection(InThrowDirection.GetSafeNormal())
	{
	}

	UPROPERTY()
	FVector_NetQuantizeNormal ThrowDirection = FVector::ForwardVector;

	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_OreThrowDirection> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_OreThrowDirection>
{
	enum
	{
		WithNetSerializer = true
	};
};

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

/** Waits on the server for the owning client's camera-facing ore throw direction. */
UCLASS()
class CASINO_SIMULATOR_API UAbilityTask_WaitOreThrowTargetData : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Wait Ore Throw Target Data", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UAbilityTask_WaitOreThrowTargetData* WaitOreThrowTargetData(UGameplayAbility* OwningAbility);

	UPROPERTY(BlueprintAssignable)
	FOreThrowDirectionReceived OnValidDirection;

	virtual void Activate() override;

private:
	void OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);
};

/** Sends the locally controlled player's view direction as GAS Target Data for ore throwing. */
UCLASS()
class CASINO_SIMULATOR_API UAbilityTask_SendOreThrowTargetData : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Send Ore Throw Target Data", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UAbilityTask_SendOreThrowTargetData* SendOreThrowTargetData(UGameplayAbility* OwningAbility);

	virtual void Activate() override;
};
