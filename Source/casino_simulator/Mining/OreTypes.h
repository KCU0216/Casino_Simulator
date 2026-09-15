#pragma once

#include "CoreMinimal.h"
#include "OreTypes.generated.h"

/** Shared identity for both mineable ore nodes and their dropped pickup actors. */
UENUM(BlueprintType)
enum class EOreType : uint8
{
	Iron,
	Gold,
	Diamond
};
