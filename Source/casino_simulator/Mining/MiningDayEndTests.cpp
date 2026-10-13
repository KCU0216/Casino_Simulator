#if WITH_DEV_AUTOMATION_TESTS
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "casino_simulatorGameMode.h"
#include "casino_simulatorAbilitySystemComponent.h"
#include "Mining/MiningGameplayTags.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoMiningDayEndTest, "Casino.Mining.DayEndStopsHeldMining",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoMiningDayEndTest::RunTest(const FString& Parameters)
{
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    UClass* ModeClass = LoadClass<Acasino_simulatorGameMode>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
    UClass* MiningClass = LoadClass<UGameplayAbility>(nullptr,
        TEXT("/Game/KCU/Mine/GA_Mining.GA_Mining_C"));
    if (!TestNotNull(TEXT("Player BP"), PlayerClass) || !TestNotNull(TEXT("Mode BP"), ModeClass)
        || !TestNotNull(TEXT("Mining ability BP"), MiningClass)) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    World->SetGameState(State);
    auto* Mode = World->SpawnActor<Acasino_simulatorGameMode>(ModeClass);
    Mode->GameState = State;
    auto* PC = World->SpawnActor<Acasino_simulatorPlayerController>(Mode->PlayerControllerClass);
    auto* Player = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass);
    if (!TestNotNull(TEXT("Controller"), PC) || !TestNotNull(TEXT("Character"), Player)) return false;
    // This lightweight world skips controller PostInitializeComponents.
    World->AddController(PC);
    PC->SetAsLocalPlayerController();
    PC->Possess(Player);
    auto* ASC = Cast<Ucasino_simulatorAbilitySystemComponent>(Player->GetAbilitySystemComponent());
    if (!TestNotNull(TEXT("Input router"), ASC)) return false;
    const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(TEXT("Input.Mining"));
    const FGameplayTag MiningTag = FGameplayTag::RequestGameplayTag(TEXT("State.Mining"));
    ASC->AddLooseGameplayTag(MiningGameplayTags::PickaxeEquipped);
    FGameplayAbilitySpecHandle MiningHandle;
    for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        if (Spec.Ability && Spec.Ability->GetClass() == MiningClass) MiningHandle = Spec.Handle;
    if (!MiningHandle.IsValid()) MiningHandle = Player->GrantAbility(MiningClass, 1, InputTag);
    if (!TestTrue(TEXT("Mining spec granted"), MiningHandle.IsValid())) return false;
    auto IsMining = [&]() { return ASC->FindAbilitySpecFromHandle(MiningHandle)->IsActive(); };
    auto VerifyStopped = [&]()
    {
        TestFalse(TEXT("Mining ability ended"), IsMining());
        TestFalse(TEXT("Mining input no longer held"), ASC->FindAbilitySpecFromHandle(MiningHandle)->InputPressed);
        TestFalse(TEXT("Mining activation tag removed"), ASC->HasMatchingGameplayTag(MiningTag));
        TestTrue(TEXT("Pickaxe stays equipped"), ASC->HasMatchingGameplayTag(MiningGameplayTags::PickaxeEquipped));
    };

    // The existing input router emits its normal diagnostics at Error severity.
    AddExpectedError(TEXT("PressInputTag"), EAutomationExpectedErrorFlags::Contains, 0);
    ASC->PressInputTag(InputTag);
    if (!TestTrue(TEXT("Held mining starts"), IsMining())) return false;
    ASC->ReleaseInputTag(InputTag);
    VerifyStopped();

    // No mouse release: the owning client's payment transition must end the ability.
    ASC->PressInputTag(InputTag);
    if (!TestTrue(TEXT("Mining restarts before payment UI"), IsMining())) return false;
    PC->ClientPrepareDailyPayment_Implementation(FRotator::ZeroRotator);
    VerifyStopped();
    PC->ClientPrepareDailyPayment_Implementation(FRotator::ZeroRotator);
    ASC->ReleaseInputTag(InputTag); // A late physical release remains harmless.
    VerifyStopped();

    PC->ClientPrepareCasinoDay_Implementation(FRotator::ZeroRotator);
    ASC->PressInputTag(InputTag);
    TestTrue(TEXT("A new press can mine the next day"), IsMining());
    ASC->ReleaseInputTag(InputTag);
    VerifyStopped();

    // A remote client's server spec has no local PressInputHandles entry.
    // Use a plain controller so the client payment RPC cannot mask a missing
    // server-side stop in BeginPaymentPhase.
    PC->UnPossess();
    auto* ServerOnlyController = World->SpawnActor<APlayerController>();
    World->AddController(ServerOnlyController);
    ServerOnlyController->SetAsLocalPlayerController();
    ServerOnlyController->Possess(Player);
    ASC->FindAbilitySpecFromHandle(MiningHandle)->InputPressed = true;
    if (!TestTrue(TEXT("Server mining starts without local input cache"), ASC->TryActivateAbility(MiningHandle))) return false;
    if (!TestTrue(TEXT("Server mining remains active before day end"), IsMining())) return false;
    auto* Point = World->SpawnActor<ATargetPoint>();
    Point->SetActorLocation(FVector(5000.f, 0.f, 100.f));
    Mode->PaymentSpawns.Add(Point);
    FCasinoLoopStatus Status;
    Status.Phase = ECasinoLoopPhase::Playing;
    State->SetLoopStatus(Status);
    Mode->BeginPaymentPhase();
    VerifyStopped();
    TestTrue(TEXT("Player relocated for payment"), Player->GetActorLocation().Equals(Point->GetActorLocation(), 1.f));
    TestEqual(TEXT("Day reaches settlement"), State->LoopStatus.Phase, ECasinoLoopPhase::Settling);
    return true;
}
#endif
