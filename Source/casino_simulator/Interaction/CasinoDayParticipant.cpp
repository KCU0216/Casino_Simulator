#include "Interaction/CasinoDayParticipant.h"
#include "casino_loop_gamestate.h"
#include "Engine/World.h"

bool IsCasinoGameplayAllowed(const UObject* Context)
{
    const UWorld* World = Context ? Context->GetWorld() : nullptr;
    if (!World) return false;
    const auto* GS = World->GetGameState<ACasinoLoopGameState>();
    return !GS || GS->LoopStatus.Phase == ECasinoLoopPhase::Playing;
}
