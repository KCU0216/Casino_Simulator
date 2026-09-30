#if WITH_DEV_AUTOMATION_TESTS
#include "Interaction/MachineInteractionComponent.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "ThreeCardPoker/ThreeCardPokerTableActor.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldInteractionReleaseTest,
    "Casino.Interaction.WorldParentRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldInteractionReleaseTest::RunTest(const FString& Parameters)
{
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
    if (!TestNotNull(TEXT("Player class"), PlayerClass) ||
        !TestNotNull(TEXT("Controller class"), ControllerClass)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass);
    auto* PC = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass);
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    auto* Table = World->SpawnActor<AThreeCardPokerTableActor>();
    if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Controller"), PC) ||
        !TestNotNull(TEXT("State"), State) || !TestNotNull(TEXT("Table"), Table)) return false;
    World->SetGameState(State);
    PC->Possess(Player);
    State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
    Table->SetInteractingPlayer(Player);
    Player->SetCurrentSeatedMachine(Table);
    Player->GetCharacterMovement()->DisableMovement();
    // A non-seated WorldInteractable must pass the controller's generic dispatch.
    PC->Server_CloseCurrentInteraction_Implementation(Table);
    TestNull(TEXT("Controller dispatch releases poker player"), Table->GetInteractingPlayer());
    // This isolated world has no network driver: simulate delivery of the release multicast.
    Table->Multicast_MachineReleased_Implementation(Player);
    TestNull(TEXT("Generic release clears current interaction"), Player->GetCurrentSeatedMachine().GetObject());
    TestNull(TEXT("Poker releases its player"), Table->GetInteractingPlayer());
    TestTrue(TEXT("Walking restored through parent release"),
        Player->GetCharacterMovement()->MovementMode == MOVE_Walking);

    // A stale release of this table must not unlock or detach another interaction.
    auto* OtherTable = World->SpawnActor<AThreeCardPokerTableActor>();
    OtherTable->SetInteractingPlayer(Player);
    Player->SetCurrentSeatedMachine(OtherTable);
    Player->GetCharacterMovement()->DisableMovement();
    Table->RequestReleaseMachine(Player);
    Table->Multicast_MachineReleased_Implementation(Player);
    TestEqual(TEXT("Stale release preserves new target"),
        Player->GetCurrentSeatedMachine().GetObject(), static_cast<UObject*>(OtherTable));
    TestTrue(TEXT("Stale release preserves movement mode"),
        Player->GetCharacterMovement()->MovementMode == MOVE_None);

    State->LoopStatus.Phase = ECasinoLoopPhase::Settling;
    PC->Server_CloseCurrentInteraction_Implementation(OtherTable);
    TestNull(TEXT("Payment releases poker player"), OtherTable->GetInteractingPlayer());
    OtherTable->Multicast_MachineReleased_Implementation(Player);
    TestNull(TEXT("Payment still releases interaction"), Player->GetCurrentSeatedMachine().GetObject());
    TestTrue(TEXT("Payment lock preserved after release"),
        Player->GetCharacterMovement()->MovementMode == MOVE_None);
    return true;
}
#endif
