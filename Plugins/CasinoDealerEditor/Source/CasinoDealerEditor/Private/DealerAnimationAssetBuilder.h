#pragma once

#include "CoreMinimal.h"

// Builds and validates only the generated dealer animation assets. Source assets are never overwritten.
bool BuildBlackjackDealerAnimationAssets(FString& OutError);
