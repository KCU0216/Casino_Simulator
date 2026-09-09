#pragma once

#include "CoreMinimal.h"
#include "Blackjack/BlackjackPlayerComponent.h"
#include "BlackjackTestObserver.generated.h"

/** Transient delegate receiver used by blackjack automation tests. */
UCLASS(Transient, NotBlueprintable)
class UBlackjackTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Starts = 0;
	int32 Ends = 0;
	int32 Results = 0;
	EBlackjackRequestResult LastResult = EBlackjackRequestResult::Rejected;
	UFUNCTION() void Started(ABlackjackTableActor* Table, int32 SeatIndex) { ++Starts; }
	UFUNCTION() void Ended(ABlackjackTableActor* Table, int32 SeatIndex) { ++Ends; }
	UFUNCTION() void Completed(ABlackjackTableActor* Table, EBlackjackRequestAction Action, EBlackjackRequestResult Result)
	{
		++Results;
		LastResult = Result;
	}
};
