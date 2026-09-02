// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "casino_simulatorAbilitySystemComponent.generated.h"

/**
 * Routes semantic input tags (for example Input.Mining) to granted ability specs.
 * The component caches exactly which specs received a press so the matching release is
 * delivered only to those specs, then uses GAS's normal replicated input-event path.
 */
UCLASS()
class CASINO_SIMULATOR_API Ucasino_simulatorAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** Finds granted specs carrying InputTag, remembers their handles, and activates inactive matches. */
	UFUNCTION(BlueprintCallable, Category = "Gameplay Abilities|Input")
	void PressInputTag(FGameplayTag InputTag);

	/** Releases only the specs that were recorded when this InputTag was pressed. */
	UFUNCTION(BlueprintCallable, Category = "Gameplay Abilities|Input")
	void ReleaseInputTag(FGameplayTag InputTag);

private:
	/** Pressed Input Tag -> exact specs that should receive its matching release. */
	TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle>> PressedInputHandles;
};
