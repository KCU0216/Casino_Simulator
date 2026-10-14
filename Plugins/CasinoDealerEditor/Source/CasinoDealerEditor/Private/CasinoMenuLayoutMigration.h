#pragma once

#include "CoreMinimal.h"

/** Restyles four existing menu assets, retaining their Blueprint interaction contracts. */
bool MigrateCasinoMenuLayouts(FString& OutError);

/** Matches the existing settings pages to the menu theme without editing their logic. */
bool MigrateCasinoSettingsLayout(FString& OutError);
