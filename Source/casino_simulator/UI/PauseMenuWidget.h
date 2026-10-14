// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/CasinoManagedWidget.h"
#include "PauseMenuWidget.generated.h"

/** In-game menu parent; keeps its embedded settings translucent over gameplay. */
UCLASS(abstract)
class CASINO_SIMULATOR_API UPauseMenuWidget : public UCasinoManagedWidget
{
	GENERATED_BODY()

public:
	/** Presentation only, also safe for transient designer previews. */
	void ApplyInGameSettingsAppearance();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
};
