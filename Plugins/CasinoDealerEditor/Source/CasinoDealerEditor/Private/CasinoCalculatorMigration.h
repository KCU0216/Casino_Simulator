#pragma once

#include "CoreMinimal.h"

/** Reparents only the shared keypad, preserving its original click graphs and dispatcher. */
bool MigrateCasinoCalculatorKeyboard(FString& OutError);
