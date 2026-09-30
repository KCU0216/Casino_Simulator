#if WITH_DEV_AUTOMATION_TESTS
#include "Interaction/InteractionSessionComponent.h"
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
        UInteractionSessionComponent::RestoreMovementAfterUse(Player);
        TestTrue(TEXT("Ordinary release restores walking"), Movement->MovementMode == MOVE_Walking);
    }
    for (ECasinoLoopPhase Phase : {ECasinoLoopPhase::Settling, ECasinoLoopPhase::DayPassed,
        ECasinoLoopPhase::GameOver, ECasinoLoopPhase::Cleared})
    {
        State->LoopStatus.Phase = Phase;
        Movement->DisableMovement();
        UInteractionSessionComponent::RestoreMovementAfterUse(Player);
        TestTrue(TEXT("Late release preserves payment/result lock"), Movement->MovementMode == MOVE_None);
    }
    State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
    UInteractionSessionComponent::RestoreMovementAfterUse(Player);
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
    auto* PokerSession = Table->FindComponentByClass<UInteractionSessionComponent>();
    if (!TestNotNull(TEXT("Poker session"), PokerSession)) return false;
    if (!Table->HasActorBegunPlay()) Table->DispatchBeginPlay();
    TestTrue(TEXT("Poker session accepts player"), PokerSession->TryJoin(Player));
    TestEqual(TEXT("Poker records current interaction"),
        Player->GetCurrentSeatedMachine().GetObject(), static_cast<UObject*>(Table));
    Player->GetCharacterMovement()->DisableMovement();
    // Poker now follows the same current-interaction dispatch as machines and NPCs.
    PC->Server_CloseCurrentInteraction_Implementation(Table);
    TestNull(TEXT("Controller dispatch releases poker player"), Table->GetInteractingPlayer());
    TestNull(TEXT("Poker close clears current interaction"), Player->GetCurrentSeatedMachine().GetObject());
    TestNull(TEXT("Poker releases its player"), Table->GetInteractingPlayer());
    // This isolated world has no network driver, so simulate delivery of the client completion RPC.
    PC->Client_CompleteInteractionClose_Implementation(Table);
    TestTrue(TEXT("Walking restored through poker release"),
        Player->GetCharacterMovement()->MovementMode == MOVE_Walking);
    return true;
}
#endif
