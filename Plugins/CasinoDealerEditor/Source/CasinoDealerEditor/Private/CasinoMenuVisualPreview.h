#pragma once

#include "CoreMinimal.h"

/** Render actual menu WidgetBlueprints into transient render targets in a running editor. */
bool RenderCasinoMenuPreviews(FString& OutError);
/** Design-only settings pages, including the real settings child embedded in the main menu. */
bool RenderCasinoSettingsPreviews(FString& OutError);
/** Transparent in-game Tab menu and settings; designer-only, no gameplay/settings calls. */
bool RenderCasinoPausePreviews(FString& OutError);
void RegisterCasinoMenuPreviewCommands();
void UnregisterCasinoMenuPreviewCommands();
