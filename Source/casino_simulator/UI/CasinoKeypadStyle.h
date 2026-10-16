#pragma once

#include "CoreMinimal.h"

class UUserWidget;

/** Applies the casino keypad palette without changing placement or click behavior. */
CASINO_SIMULATOR_API void ApplyCasinoKeypadStyle(UUserWidget* Keypad);

/** Centers only the keypad embedded in the daily payment panel. */
CASINO_SIMULATOR_API void ApplyCasinoPaymentKeypadLayout(UUserWidget* Keypad);

/** Keeps the trade keypad beside its form while retaining the full-screen dismiss target. */
CASINO_SIMULATOR_API void ApplyCasinoTradeKeypadLayout(UUserWidget* Keypad);
