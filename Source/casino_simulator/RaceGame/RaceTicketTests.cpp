#if WITH_DEV_AUTOMATION_TESTS

#include "RaceManager.h"
#include "RaceRunner.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceTicketSnapshotTest, "Casino.Race.TicketSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaceTicketSnapshotTest::RunTest(const FString& Parameters)
{
    UClass* RunnerClass = LoadClass<ARaceRunner>(nullptr,
        TEXT("/Game/KCU/HorseRacing/BP_RunnerBase.BP_RunnerBase_C"));
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Runner Blueprint"), RunnerClass)
        || !TestNotNull(TEXT("Player Blueprint"), PlayerClass)) return false;
    auto* PortraitProperty = FindFProperty<FObjectPropertyBase>(RunnerClass, TEXT("RunnerPortrait"));
    if (!TestNotNull(TEXT("Native runner portrait property"), PortraitProperty)) return false;
    TestTrue(TEXT("Portrait belongs to the C++ parent"), PortraitProperty->GetOwnerClass() == ARaceRunner::StaticClass());
    TestTrue(TEXT("Portrait is replicated"), PortraitProperty->HasAnyPropertyFlags(CPF_Net));
    TestNull(TEXT("Blueprint no longer defines a separate portrait"), FindFProperty<FObjectPropertyBase>(RunnerClass, TEXT("Portrait")));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    auto* Manager = World->SpawnActor<ARaceManager>();
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass,
        FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    auto* Buyer = World->SpawnActor<APlayerState>();
    auto* SpawnPoint = World->SpawnActor<ATargetPoint>();
    if (!TestNotNull(TEXT("Manager"), Manager) || !TestNotNull(TEXT("Player"), Player)
        || !TestNotNull(TEXT("Buyer"), Buyer) || !TestNotNull(TEXT("Spawn point"), SpawnPoint)) return false;
    auto* PlayerStateProperty = FindFProperty<FObjectPropertyBase>(APawn::StaticClass(), TEXT("PlayerState"));
    if (!TestNotNull(TEXT("Player-state property"), PlayerStateProperty)) return false;
    PlayerStateProperty->SetObjectPropertyValue_InContainer(Player, Buyer);
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 1000.f);

    Manager->RunnerClass = RunnerClass;
    Manager->RunnerSpawnPoints.Add(SpawnPoint);
    Manager->StartNewRound();
    if (!TestEqual(TEXT("First round has a runner"), Manager->GetRunners().Num(), 1)) return false;
    ARaceRunner* Runner = Manager->GetRunners()[0];
    const int32 Ages[] = { 65, 75, 85, 95 };
    const TCHAR* PortraitPaths[] = {
        TEXT("/Game/KCU/HorseRacing/runner_blind_man.runner_blind_man"),
        TEXT("/Game/KCU/HorseRacing/runner_cane_man.runner_cane_man"),
        TEXT("/Game/KCU/HorseRacing/runner_wheelchair_man.runner_wheelchair_man"),
        TEXT("/Game/KCU/HorseRacing/runner_blind_woman.runner_blind_woman")
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ages); ++Index)
    {
        auto* ExpectedPortrait = LoadObject<UTexture2D>(nullptr, PortraitPaths[Index]);
        if (!TestNotNull(TEXT("Age-group portrait asset"), ExpectedPortrait)) return false;
        FRaceRunnerStats Stats = Runner->Stats;
        Stats.Age = Ages[Index];
        Runner->RunnerPortrait = nullptr;
        Runner->InitStats(Stats);
        TestTrue(FString::Printf(TEXT("Age %d selects the native portrait through Blueprint"), Ages[Index]),
            Runner->GetRunnerPortrait() == ExpectedPortrait);
    }
    auto* OriginalPortrait = NewObject<UTexture2D>();
    auto* OtherPortrait = NewObject<UTexture2D>();
    Runner->RunnerPortrait = OriginalPortrait;
    TestTrue(TEXT("Parent getter returns the assigned portrait"), Runner->GetRunnerPortrait() == OriginalPortrait);
    Runner->Stats.Name = TEXT("Original runner");
    Runner->Stats.Age = 87;
    Runner->Stats.Odds = 2.f;
    Manager->Phase = ERacePhase::Betting;
    if (!TestTrue(TEXT("Ticket purchase succeeds"), Manager->ServerBuyTicket(Player, 0, 100, 1))) return false;
    TestEqual(TEXT("Purchase stores runner age"), Manager->Tickets[0].RunnerAge, 87);
    TestTrue(TEXT("Purchase stores native runner portrait"), Manager->Tickets[0].RunnerPortrait == OriginalPortrait);

    // Changing the live runner must not change the already purchased ticket.
    Runner->Stats.Age = 99;
    Runner->RunnerPortrait = OtherPortrait;
    TestEqual(TEXT("Age remains the purchase-time value"), Manager->Tickets[0].RunnerAge, 87);
    TestTrue(TEXT("Portrait remains the purchase-time asset"), Manager->Tickets[0].RunnerPortrait == OriginalPortrait);

    // Keep a winning ticket while retiring its runner and creating the next round.
    Manager->Tickets[0].bWon = true;
    Manager->Phase = ERacePhase::Finished;
    Manager->ResetRace();
    Manager->StartNewRound();
    TestEqual(TEXT("Next round starts"), Manager->CurrentRoundNumber, 2);
    const TArray<FBetTicket> SavedTickets = Manager->GetTicketsForPlayer(Buyer);
    if (!TestEqual(TEXT("Unclaimed ticket survives the next round"), SavedTickets.Num(), 1)) return false;
    TestEqual(TEXT("Original round survives"), SavedTickets[0].RoundNumber, 1);
    TestEqual(TEXT("Original age survives runner destruction"), SavedTickets[0].RunnerAge, 87);
    TestTrue(TEXT("Original portrait survives runner destruction"), SavedTickets[0].RunnerPortrait == OriginalPortrait);
    TestEqual(TEXT("Unclaimed ticket can still be redeemed"), Manager->ServerClaimWinnings(Player), 200);
    TestEqual(TEXT("Redeemed ticket is removed"), Manager->GetTicketsForPlayer(Buyer).Num(), 0);
    return true;
}

#endif
