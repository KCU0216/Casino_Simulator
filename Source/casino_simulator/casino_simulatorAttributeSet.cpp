// Copyright Epic Games, Inc. All Rights Reserved.

#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

Ucasino_simulatorAttributeSet::Ucasino_simulatorAttributeSet()
{
	InitCurrency(10000.0f);
}

void Ucasino_simulatorAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(Ucasino_simulatorAttributeSet, Currency, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(Ucasino_simulatorAttributeSet, Nicotine, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(Ucasino_simulatorAttributeSet, NicotineDecayRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(Ucasino_simulatorAttributeSet, MaxNicotine, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(Ucasino_simulatorAttributeSet, Alcohol, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(Ucasino_simulatorAttributeSet, AlcoholDecayRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(Ucasino_simulatorAttributeSet, MaxAlcohol, COND_None, REPNOTIFY_Always);
}

void Ucasino_simulatorAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 현재 값
	if (Attribute == GetNicotineAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, MaxNicotine.GetCurrentValue());
	}
	else if (Attribute == GetAlcoholAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, MaxAlcohol.GetCurrentValue());
	}
	// 차감 값
	else if (Attribute == GetNicotineDecayRateAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, MaxNicotine.GetCurrentValue());
	}
	else if (Attribute == GetAlcoholDecayRateAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, MaxAlcohol.GetCurrentValue());
	}
	// 최대 값
	else if (Attribute == GetMaxNicotineAttribute())
	{
		AdjustAttributeForMaxChange(Nicotine, MaxNicotine, NewValue, GetNicotineAttribute());
	}
	else if (Attribute == GetMaxAlcoholAttribute())
	{
		AdjustAttributeForMaxChange(Alcohol, MaxAlcohol, NewValue, GetAlcoholAttribute());
	}
}

void Ucasino_simulatorAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Clamp again after instant/periodic GameplayEffects, which modify the attribute's base
	// value directly and don't go through PreAttributeChange.
	if (Data.EvaluatedData.Attribute == GetNicotineAttribute())
	{
		SetNicotine(FMath::Clamp(GetNicotine(), 0.0f, GetMaxNicotine()));
	}
	else if (Data.EvaluatedData.Attribute == GetAlcoholAttribute())
	{
		SetAlcohol(FMath::Clamp(GetAlcohol(), 0.0f, GetMaxAlcohol()));
	}
}

void Ucasino_simulatorAttributeSet::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();

	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilityComp)
	{
		// Rescale the current value so it keeps the same fill percentage of the new max.
		const float CurrentValue = AffectedAttribute.GetCurrentValue();
		const float NewDelta = (CurrentMaxValue > 0.0f) ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;

		AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
	}
}

void Ucasino_simulatorAttributeSet::OnRep_Currency(const FGameplayAttributeData& OldCurrency)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(Ucasino_simulatorAttributeSet, Currency, OldCurrency);
}

void Ucasino_simulatorAttributeSet::OnRep_Nicotine(const FGameplayAttributeData& OldNicotine)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(Ucasino_simulatorAttributeSet, Nicotine, OldNicotine);
}

void Ucasino_simulatorAttributeSet::OnRep_NicotineDecayRate(const FGameplayAttributeData& OldNicotineRate)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(Ucasino_simulatorAttributeSet, NicotineDecayRate, OldNicotineRate);
}

void Ucasino_simulatorAttributeSet::OnRep_MaxNicotine(const FGameplayAttributeData& OldMaxNicotine)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(Ucasino_simulatorAttributeSet, MaxNicotine, OldMaxNicotine);
}

void Ucasino_simulatorAttributeSet::OnRep_Alcohol(const FGameplayAttributeData& OldAlcohol)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(Ucasino_simulatorAttributeSet, Alcohol, OldAlcohol);
}

void Ucasino_simulatorAttributeSet::OnRep_AlcoholDecayRate(const FGameplayAttributeData& OldAlcoholDecayRate)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(Ucasino_simulatorAttributeSet, AlcoholDecayRate, OldAlcoholDecayRate);
}

void Ucasino_simulatorAttributeSet::OnRep_MaxAlcohol(const FGameplayAttributeData& OldMaxAlcohol)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(Ucasino_simulatorAttributeSet, MaxAlcohol, OldMaxAlcohol);
}
