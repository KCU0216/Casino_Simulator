#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;

/** Restyles four existing menu assets, retaining their Blueprint interaction contracts. */
bool MigrateCasinoMenuLayouts(FString& OutError);

/** Matches the existing settings pages to the menu theme without editing their logic. */
bool MigrateCasinoSettingsLayout(FString& OutError);

/** Restyles the in-game menu while preserving its existing navigation and exit actions. */
bool MigrateCasinoPauseMenuLayout(FString& OutError);

/** Compiles and saves an approved presentation asset only after its original logic is verified. */
bool MigrateCasinoPresentationWidget(const TCHAR* AssetPath,
    TFunctionRef<bool(UWidgetBlueprint*, FString&)> ApplyLayout, FString& OutError,
    const TSet<FName>& AddedBindingTargets = {});
