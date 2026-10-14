#pragma once

#include "CoreMinimal.h"

/** Imports the existing card slap recording and adds a single local landing sound to the card flight. */
bool SetupBlackjackCardPlacementSound(FString& OutError);

/** Checks the shipped sound mapping, attenuation and the exact timeline completion hookup without modifying assets. */
bool ValidateBlackjackCardPlacementSound(FString& OutError);
