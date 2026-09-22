#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CasinoMainMenuWidget.generated.h"

/**
 * Blueprint-owned main menu base class.
 * Widget layout, navigation, and event binding live in WBP_MainMenu.
 */
UCLASS()
class CASINO_SIMULATOR_API UCasinoMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
};
