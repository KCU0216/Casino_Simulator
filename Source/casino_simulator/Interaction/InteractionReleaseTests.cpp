#if WITH_DEV_AUTOMATION_TESTS
#include "Interaction/MachineInteractionComponent.h"
#include "casino_simulatorCharacter.h"
#include "casino_loop_gamestate.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionReleaseMovementTest,
    "Casino.Interaction.ReleaseMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionReleaseMovementTest::RunTest(const FString& Parameters)
{
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Player class"), PlayerClass)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass);
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Game state"), State)) return false;
    World->SetGameState(State);
    auto* Movement = Player->GetCharacterMovement();
    for (ECasinoLoopPhase Phase : {ECasinoLoopPhase::Waiting, ECasinoLoopPhase::Playing})
    {
        State->LoopStatus.Phase = Phase;
        Movement->DisableMovement();
        UMachineInteractionComponent::RestoreMovementAfterUse(Player);
        TestTrue(TEXT("Ordinary release restores walking"), Movement->MovementMode == MOVE_Walking);
    }
    for (ECasinoLoopPhase Phase : {ECasinoLoopPhase::Settling, ECasinoLoopPhase::DayPassed,
        ECasinoLoopPhase::GameOver, ECasinoLoopPhase::Cleared})
    {
        State->LoopStatus.Phase = Phase;
        Movement->DisableMovement();
        UMachineInteractionComponent::RestoreMovementAfterUse(Player);
        TestTrue(TEXT("Late release preserves payment/result lock"), Movement->MovementMode == MOVE_None);
    }
    State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
    UMachineInteractionComponent::RestoreMovementAfterUse(Player);
    TestTrue(TEXT("Next day permits movement again"), Movement->MovementMode == MOVE_Walking);
    return true;
}
#endif
