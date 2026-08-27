// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/NPC_Base.h"
#include "NPC_InteractionCameraBase.generated.h"

class UCameraComponent;

/**
 * NPC variant for interactions that need their own dialogue/shop camera.
 * Generic NPCs should inherit from ANPC_Base directly.
 */
UCLASS(abstract)
class CASINO_SIMULATOR_API ANPC_InteractionCameraBase : public ANPC_Base
{
	GENERATED_BODY()

public:
	ANPC_InteractionCameraBase();

	/** Returns the camera component used by this NPC's interaction view. */
	UCameraComponent* GetInteractionCameraComponent() const { return InteractionCameraComponent; }

	UFUNCTION(BlueprintPure, Category="Interaction|Camera")
	AActor* GetInteractionCameraTarget() const;

private:
	/** Camera used by default while this NPC owns an interaction UI. Adjust it per NPC Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> InteractionCameraComponent;

	/** Optional external view actor override. Leave empty to use this NPC's InteractionCameraComponent owner. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Interaction|Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AActor> InteractionCameraTarget;
};
