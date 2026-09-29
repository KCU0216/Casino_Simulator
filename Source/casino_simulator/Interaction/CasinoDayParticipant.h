#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CasinoDayParticipant.generated.h"

/** Implement on each game actor, including Blueprint-only machines. Server calls this before payment. */
UINTERFACE(BlueprintType)
class CASINO_SIMULATOR_API UCasinoDayParticipant : public UInterface
{
    GENERATED_BODY()
};

class CASINO_SIMULATOR_API ICasinoDayParticipant
{
    GENERATED_BODY()
public:
    /** Stop pending payouts/timers and release users. Must finish synchronously; do not use Delay. */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Casino|Day")
    void EndCasinoDay();
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Casino|Day")
    void BeginCasinoDay();
};

/** No loop GameState means a standalone game test map. */
CASINO_SIMULATOR_API bool IsCasinoGameplayAllowed(const UObject* Context);
