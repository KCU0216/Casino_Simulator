#pragma once

#include "CoreMinimal.h"

/** Creates only the isolated, whitelisted dealer showcase map; never changes the open level. */
bool CreateDealerVisualPreview(FString& OutError);
void RegisterDealerVisualPreviewCallbacks();
void UnregisterDealerVisualPreviewCallbacks();
