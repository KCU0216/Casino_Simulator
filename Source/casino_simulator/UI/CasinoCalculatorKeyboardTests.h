#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CasinoCalculatorKeyboardTests.generated.h"

/** Transient receiver used to verify the shipped Blueprint's existing delegates. */
UCLASS(Transient, NotBlueprintable)
class UCasinoCalculatorKeyboardTestReceiver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION() void RecordNumber(int32 Value);
	UFUNCTION() void RecordConfirm();
	TArray<int32> Values;
	int32 ConfirmCount = 0;
};
