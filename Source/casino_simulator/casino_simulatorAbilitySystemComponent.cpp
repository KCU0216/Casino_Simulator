// Copyright Epic Games, Inc. All Rights Reserved.

#include "casino_simulatorAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"

namespace
{
	const FGameplayAbilityActivationInfo& GetSpecActivationInfo(const FGameplayAbilitySpec& Spec)
	{
		const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();

		// A gameplay ability Blueprint is instanced. The fallback keeps this router compatible
		// with an eventual non-instanced native ability.
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Instances.IsEmpty() ? Spec.ActivationInfo : Instances.Last()->GetCurrentActivationInfoRef();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

void Ucasino_simulatorAbilitySystemComponent::PressInputTag(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle>& PressedHandles = PressedInputHandles.FindOrAdd(InputTag);
	PressedHandles.Reset();

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		PressedHandles.AddUnique(Spec.Handle);
		Spec.InputPressed = true;

		if (!Spec.IsActive())
		{
			TryActivateAbility(Spec.Handle);
			continue;
		}

		if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
		{
			ServerSetInputPressed(Spec.Handle);
		}

		AbilitySpecInputPressed(Spec);
		InvokeReplicatedEvent(
			EAbilityGenericReplicatedEvent::InputPressed,
			Spec.Handle,
			GetSpecActivationInfo(Spec).GetActivationPredictionKey()
		);
	}
}

void Ucasino_simulatorAbilitySystemComponent::ReleaseInputTag(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> ReleasedHandles;
	if (!InputTag.IsValid() || !PressedInputHandles.RemoveAndCopyValue(InputTag, ReleasedHandles))
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpecHandle Handle : ReleasedHandles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}

		Spec->InputPressed = false;
		if (!Spec->Ability || !Spec->IsActive())
		{
			continue;
		}

		if (Spec->Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
		{
			ServerSetInputReleased(Spec->Handle);
		}

		AbilitySpecInputReleased(*Spec);
		InvokeReplicatedEvent(
			EAbilityGenericReplicatedEvent::InputReleased,
			Spec->Handle,
			GetSpecActivationInfo(*Spec).GetActivationPredictionKey()
		);
	}
}
