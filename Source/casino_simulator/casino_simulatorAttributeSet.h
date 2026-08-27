// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "casino_simulatorAttributeSet.generated.h"

struct FGameplayEffectModCallbackData;

// Boilerplate accessor macro for a gameplay attribute. AttributeSet.h only provides the
// individual GAMEPLAYATTRIBUTE_* macros; this combines the ones we need per-attribute.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Tracks the character's nicotine and alcohol intoxication levels.
 * Each stat has a Current value clamped to [0, Max].
 */
UCLASS()
class Ucasino_simulatorAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	Ucasino_simulatorAttributeSet();

	//~ Begin UAttributeSet interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet interface

	/** Current nicotine level */
	UPROPERTY(BlueprintReadOnly, Category = "Currency", ReplicatedUsing = OnRep_Currency)
	FGameplayAttributeData Currency;
	ATTRIBUTE_ACCESSORS(Ucasino_simulatorAttributeSet, Currency)

	UPROPERTY(BlueprintReadOnly, Category = "Nicotine", ReplicatedUsing = OnRep_Nicotine)
	FGameplayAttributeData Nicotine;
	ATTRIBUTE_ACCESSORS(Ucasino_simulatorAttributeSet, Nicotine)

	UPROPERTY(BlueprintReadOnly, Category = "Nicotine", ReplicatedUsing = OnRep_NicotineDecayRate)
	FGameplayAttributeData NicotineDecayRate;   // 초당(또는 Period당) 감쇠량. 기본 1.0
	ATTRIBUTE_ACCESSORS(Ucasino_simulatorAttributeSet, NicotineDecayRate)

	/** Maximum nicotine level */
	UPROPERTY(BlueprintReadOnly, Category = "Nicotine", ReplicatedUsing = OnRep_MaxNicotine)
	FGameplayAttributeData MaxNicotine;
	ATTRIBUTE_ACCESSORS(Ucasino_simulatorAttributeSet, MaxNicotine)

	/** Current alcohol level */
	UPROPERTY(BlueprintReadOnly, Category = "Alcohol", ReplicatedUsing = OnRep_Alcohol)
	FGameplayAttributeData Alcohol;
	ATTRIBUTE_ACCESSORS(Ucasino_simulatorAttributeSet, Alcohol)

	UPROPERTY(BlueprintReadOnly, Category = "Alcohol", ReplicatedUsing = OnRep_AlcoholDecayRate)
	FGameplayAttributeData AlcoholDecayRate;
	ATTRIBUTE_ACCESSORS(Ucasino_simulatorAttributeSet, AlcoholDecayRate)

	/** Maximum alcohol level */
	UPROPERTY(BlueprintReadOnly, Category = "Alcohol", ReplicatedUsing = OnRep_MaxAlcohol)
	FGameplayAttributeData MaxAlcohol;
	ATTRIBUTE_ACCESSORS(Ucasino_simulatorAttributeSet, MaxAlcohol)

protected:

	UFUNCTION()
	virtual void OnRep_Currency(const FGameplayAttributeData& OldCurrency);
	UFUNCTION()
	virtual void OnRep_Nicotine(const FGameplayAttributeData& OldNicotine);

	UFUNCTION()
	virtual void OnRep_NicotineDecayRate(const FGameplayAttributeData& OldNicotineRate);

	UFUNCTION()
	virtual void OnRep_MaxNicotine(const FGameplayAttributeData& OldMaxNicotine);

	UFUNCTION()
	virtual void OnRep_Alcohol(const FGameplayAttributeData& OldAlcohol);

	UFUNCTION()
	virtual void OnRep_AlcoholDecayRate(const FGameplayAttributeData& OldAlcoholDecayRate);

	UFUNCTION()
	virtual void OnRep_MaxAlcohol(const FGameplayAttributeData& OldMaxAlcohol);

private:

	/** When a Max attribute changes, rescales its paired Current attribute to keep the same fill ratio. */
	void AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty);
};
