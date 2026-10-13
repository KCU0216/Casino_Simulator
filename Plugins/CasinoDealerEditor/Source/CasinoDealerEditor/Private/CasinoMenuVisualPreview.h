#pragma once

#include "CoreMinimal.h"

/** Render actual menu WidgetBlueprints into transient render targets in a running editor. */
bool RenderCasinoMenuPreviews(FString& OutError);
void RegisterCasinoMenuPreviewCommands();
void UnregisterCasinoMenuPreviewCommands();
