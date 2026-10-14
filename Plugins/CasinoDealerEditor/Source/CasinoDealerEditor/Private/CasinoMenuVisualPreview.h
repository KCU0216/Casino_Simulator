#pragma once

#include "CoreMinimal.h"

/** Render actual menu WidgetBlueprints into transient render targets in a running editor. */
bool RenderCasinoMenuPreviews(FString& OutError);
/** Design-only settings pages, including the real settings child embedded in the main menu. */
bool RenderCasinoSettingsPreviews(FString& OutError);
void RegisterCasinoMenuPreviewCommands();
void UnregisterCasinoMenuPreviewCommands();
