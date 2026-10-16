#pragma once

#include "CoreMinimal.h"

/** Adds the native voice controls to the existing settings pages without changing their Blueprint logic. */
bool MigrateCasinoVoiceSettings(FString& OutError);
