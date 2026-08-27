#pragma once

#include "CoreMinimal.h"
#include "SlotMachineTypes.h"

struct FSlotMachinePaylines
{
	static TArray<int32> GetPaylineIndexes(int32 LineIndex);
	static ESlotLineType GetPaylineType(int32 LineIndex);
	static int32 GetPaylineCount();
};