#pragma once

#include "CoreMinimal.h"
#include "LadderDisplayState.generated.h"

USTRUCT(BlueprintType)
struct FLadderDisplayState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 Rails = 3;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 Rows = 12;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 SelectedRail = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	TArray<int32> Odds;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	float TraceDuration = 5.0f;
};
