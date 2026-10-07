#include "PoliceEncounterComponent.h"
#include "Blackjack/BlackjackPlayerComponent.h"
#include "Blackjack/BlackjackTableActor.h"
#include "NPC/NPC_Base.h"
#include "Machine/SeatedMachineBase.h"
#include "Interaction/WorldInteractableBase.h"
#include "casino_loop_gamestate.h"
#include "Police/PoliceAiController.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "Engine/World.h"
#include "Police/PoliceCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"


UPoliceEncounterComponent::UPoliceEncounterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPoliceEncounterComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    TArray<AActor*> PoliceActors;
    UGameplayStatics::GetAllActorsOfClass(
        this,
        APoliceCharacter::StaticClass(),
        PoliceActors
    );

    if (PoliceActors.Num() != 1)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Police encounter requires exactly one police actor. Found: %d"),
            PoliceActors.Num()
        );
        return;
    }

    EncounterPoliceActor = Cast<APoliceCharacter>(PoliceActors[0]);
    PoliceInitialTransform = EncounterPoliceActor->GetActorTransform();

    EncounterPoliceActor->GetCharacterMovement()->StopMovementImmediately();
    EncounterPoliceActor->GetCharacterMovement()->DisableMovement();

    EncounterPoliceActor->SetActorEnableCollision(false);
    EncounterPoliceActor->SetActorHiddenInGame(true);
}


void UPoliceEncounterComponent::BeginPoliceDay(float RemainingDaySeconds)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    if (!bEnableDailyPoliceEncounter ||
        !IsValid(EncounterPoliceActor.Get()) ||
        EncounterState != EPoliceEncounterState::Inactive)
    {
        return;
    }

    const float IntroSeconds = FMath::Max(
        0.1f,
        PoliceIntroDurationSeconds
    );

    const float LatestDelay =
        RemainingDaySeconds - IntroSeconds - 0.1f;

    if (LatestDelay <= 0.01f)
    {
        return;
    }

    const float MinDelay = FMath::Clamp(
        FMath::Min(
            PoliceArrivalDelayMinSeconds,
            PoliceArrivalDelayMaxSeconds
        ),
        0.01f,
        LatestDelay
    );

    const float MaxDelay = FMath::Clamp(
        FMath::Max(
            PoliceArrivalDelayMinSeconds,
            PoliceArrivalDelayMaxSeconds
        ),
        MinDelay,
        LatestDelay
    );

    const float ArrivalDelay = FMath::FRandRange(
        MinDelay,
        MaxDelay
    );

    EncounterState = EPoliceEncounterState::Waiting;

    GetWorld()->GetTimerManager().SetTimer(
        PoliceArrivalTimerHandle,
        this,
        &UPoliceEncounterComponent::StartPoliceEncounter,
        ArrivalDelay,
        false
    );
}


Acasino_simulatorCharacter*
UPoliceEncounterComponent::SelectRandomPoliceTarget()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return nullptr;
    }

    TArray<Acasino_simulatorCharacter*> Candidates;

    for (FConstPlayerControllerIterator It =
        GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get());
        if (!IsValid(PC))
        {
            continue;
        }

        auto* Player = Cast<Acasino_simulatorCharacter>(PC->GetPawn());
        if (!IsValid(Player))
        {
            continue;
        }

        Candidates.Add(Player);
    }

    if (Candidates.IsEmpty())
    {
        return nullptr;
    }

    const int32 RandomIndex = FMath::RandRange(
        0,
        Candidates.Num() - 1
    );

    return Candidates[RandomIndex];
}

void UPoliceEncounterComponent::StartPoliceEncounter()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || EncounterState != EPoliceEncounterState::Waiting)
    {
        return;
    }

    const ACasinoLoopGameState* GS =
        GetWorld()->GetGameState<ACasinoLoopGameState>();

    if (!GS || GS->LoopStatus.Phase != ECasinoLoopPhase::Playing)
    {
        return;
    }

    if (!IsValid(EncounterPoliceActor.Get()))
    {
        EncounterState = EPoliceEncounterState::Finished;
        return;
    }

    APoliceAIController* PoliceAI = Cast<APoliceAIController>(
        EncounterPoliceActor->GetController());

    Acasino_simulatorCharacter* Target = SelectRandomPoliceTarget();

    if (!IsValid(PoliceAI) || !IsValid(Target))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Police AI NULL"));

        EncounterState = EPoliceEncounterState::Finished;
        return;
    }
        
    PoliceChaseTarget = Target;
    EncounterState = EPoliceEncounterState::Arrival;

    PoliceAI->StopChase();
    EncounterPoliceActor->SetActorEnableCollision(false);
    EncounterPoliceActor->SetActorHiddenInGame(false);

    for (FConstPlayerControllerIterator It =
        GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        Acasino_simulatorPlayerController* PC =
            Cast<Acasino_simulatorPlayerController>(It->Get());

        if (IsValid(PC))
        {
            PC->BeginPoliceArrival();
        }
    }

    EncounterPoliceActor->OnPoliceArrivalStarted();

    GetWorld()->GetTimerManager().SetTimer(
        PoliceIntroTimerHandle,
        this,
        &UPoliceEncounterComponent::FinishPoliceIntro,
        FMath::Max(0.1f, PoliceIntroDurationSeconds),
        false);
}

void UPoliceEncounterComponent::FinishPoliceIntro()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || EncounterState != EPoliceEncounterState::Arrival)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        Acasino_simulatorPlayerController* PC =
            Cast<Acasino_simulatorPlayerController>(It->Get());

        if (IsValid(PC))
        {
            PC->EndPoliceCinematic();
        }
    }
    APoliceCharacter* Police = EncounterPoliceActor.Get();
    Acasino_simulatorCharacter* Target = PoliceChaseTarget.Get();

    const ACasinoLoopGameState* GS =
        GetWorld()->GetGameState<ACasinoLoopGameState>();

    APoliceAIController* PoliceAI = IsValid(Police)
        ? Cast<APoliceAIController>(Police->GetController())
        : nullptr;

    if (IsValid(Police))
    {
        Police->OnPoliceEncounterStopped();
    }

    if (!GS || GS->LoopStatus.Phase != ECasinoLoopPhase::Playing ||
        !IsValid(Police) ||
        !IsValid(PoliceAI) ||
        !IsValid(Target) ||
        !Target->IsPlayerControlled())
    {
        PoliceChaseTarget.Reset();
        EncounterState = EPoliceEncounterState::Finished;

        if (IsValid(Police))
        {
            Police->GetCharacterMovement()->StopMovementImmediately();
            Police->GetCharacterMovement()->DisableMovement();
            Police->SetActorEnableCollision(false);
            Police->SetActorHiddenInGame(true);
        }
        
        return;
    }

    Police->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Police->SetActorEnableCollision(true);

    PoliceAI->OnChaseReachedTarget.AddUniqueDynamic(
        this,
        &UPoliceEncounterComponent::HandlePoliceChaseReachedTarget);

    EncounterState = EPoliceEncounterState::Chasing;
    PoliceAI->StartChase(Target);
}


void UPoliceEncounterComponent::EndPoliceDay()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    EncounterState = EPoliceEncounterState::Inactive;

    FTimerManager& Timer = GetWorld()->GetTimerManager();
    Timer.ClearTimer(PoliceArrivalTimerHandle);
    Timer.ClearTimer(PoliceIntroTimerHandle);
    Timer.ClearTimer(PoliceArrestTimerHandle);

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        Acasino_simulatorPlayerController* PC =
            Cast<Acasino_simulatorPlayerController>(It->Get());

        if (IsValid(PC))
        {
            PC->EndPoliceCinematic();
        }
    }

    APoliceCharacter* Police = EncounterPoliceActor.Get();

    if (IsValid(Police))
    {
        APoliceAIController* PoliceAI =
            Cast<APoliceAIController>(Police->GetController());

        if (IsValid(PoliceAI))
        {
            PoliceAI->StopChase();
        }

        Police->OnPoliceEncounterStopped();

        Police->GetCharacterMovement()->StopMovementImmediately();
        Police->GetCharacterMovement()->DisableMovement();

        Police->SetActorEnableCollision(false);
        Police->SetActorHiddenInGame(true);

        Police->SetActorTransform(
            PoliceInitialTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);

        Police->ForceNetUpdate();

    }

    PoliceChaseTarget.Reset();
}


void UPoliceEncounterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    EncounterState = EPoliceEncounterState::Inactive;

    if (UWorld* World = GetWorld())
    {
        FTimerManager& Timers = World->GetTimerManager();
        Timers.ClearTimer(PoliceArrivalTimerHandle);
        Timers.ClearTimer(PoliceIntroTimerHandle);
        Timers.ClearTimer(PoliceArrestTimerHandle);
    }

    if (IsValid(EncounterPoliceActor.Get()))
    {
        APoliceAIController* PoliceAI =
            Cast<APoliceAIController>(
                EncounterPoliceActor->GetController());

        if (IsValid(PoliceAI))
        {
            PoliceAI->StopChase();
        }
    }

    PoliceChaseTarget.Reset();

    Super::EndPlay(EndPlayReason);
}

void UPoliceEncounterComponent::HandlePoliceChaseReachedTarget(APawn* Target)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || EncounterState != EPoliceEncounterState::Chasing)
    {
        return;
    }

    const ACasinoLoopGameState* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();

    APoliceCharacter* Police = EncounterPoliceActor.Get();

    if (!GS || GS->LoopStatus.Phase != ECasinoLoopPhase::Playing
        || !IsValid(Police)
        || !IsValid(Target)
        || Target != PoliceChaseTarget.Get()
        || !Target->IsPlayerControlled())
    {
        return;
    }

    EncounterState = EPoliceEncounterState::Arresting;

    APoliceAIController* PoliceAI =
        Cast<APoliceAIController>(Police->GetController());

    if (IsValid(PoliceAI))
    {
        PoliceAI->StopChase();
    }

    Police->GetCharacterMovement()->StopMovementImmediately();
    Police->GetCharacterMovement()->DisableMovement();
    Police->SetActorEnableCollision(false);

    if (Acasino_simulatorCharacter* Player =
        Cast<Acasino_simulatorCharacter>(Target))
    {
        if (UBlackjackPlayerComponent* Blackjack =
            Player->GetBlackjackPlayerComponent())
        {
            if (ABlackjackTableActor* Table =
                Blackjack->GetCurrentBlackjackTable();
                IsValid(Table))
            {
                Table->ForceLeaveSeat(Player);
            }
        }
    }

    Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(Target->GetController());

    if (IsValid(PC))
    {
        PC->BeginPoliceArrest(Police);
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Police arrest started. Target: %s"),
        *GetNameSafe(Target));

    GetWorld()->GetTimerManager().SetTimer(
        PoliceArrestTimerHandle,
        this,
        &UPoliceEncounterComponent::FinishPoliceArrest,
        FMath::Max(0.1f, PoliceArrestDurationSeconds),
        false);
}

void UPoliceEncounterComponent::FinishPoliceArrest()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || EncounterState != EPoliceEncounterState::Arresting)
    {
        return;
    }

    const ACasinoLoopGameState* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();

    if (!GS || GS->LoopStatus.Phase != ECasinoLoopPhase::Playing)
    {
        return;
    }

    APoliceCharacter* Police = EncounterPoliceActor.Get();
    Acasino_simulatorCharacter* Player = PoliceChaseTarget.Get();

    if (IsValid(Player))
    {
        Acasino_simulatorPlayerController* PC = Cast<Acasino_simulatorPlayerController>(Player->GetController());

        if (IsValid(PC))
        {
            PC->EndPoliceCinematic();
        }
    }

    AActor* JailPoint = IsValid(Police)
        ? Police->GetPoliceJailDestination()
        : nullptr;

    EncounterState = EPoliceEncounterState::Finished;
    PoliceChaseTarget.Reset();

    if (IsValid(Police))
    {
        Police->OnPoliceEncounterStopped();
        Police->SetActorEnableCollision(false);
        Police->SetActorHiddenInGame(true);
        Police->ForceNetUpdate();
    }

    if (!IsValid(Player) || !Player->IsPlayerControlled() || !IsValid(JailPoint))
    {
        UE_LOG(LogTemp, Error, TEXT("Police Arrest Failed: invalid player or jail destinaion"));
        return;
    }

    UObject* InteractionTarget = Player->GetCurrentInteractionTarget().GetObject();

    if (ANPC_Base* NPC = Cast<ANPC_Base>(InteractionTarget))
    {
        NPC->ReleaseInteraction(Player);
    }
    else if(ASeatedMachineBase* Machine = Cast<ASeatedMachineBase>(InteractionTarget))
    {
        //  // 게임 진행 중의 퇴장 제한을 체포 시에는 해제
        Machine->SetCanExitMachine(true);
        Machine->RequestReleaseMachine(Player);
    }
    else if (AWorldInteractableBase* Interactable = Cast<AWorldInteractableBase>(InteractionTarget))
    {
        Interactable->RequestReleaseMachine(Player);

    }


    Player->GetCharacterMovement()->StopMovementImmediately();

    const bool bMoved = Player->SetActorLocation(JailPoint->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);

    if (!bMoved)
    {
        UE_LOG(LogTemp, Error, TEXT("Police Arrest Failed: jail teleport failed."));
        return;
    }

    if (IsValid(Police))
    {
        Police->OnPolicePlayerJailed();
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Police arrest completed. Jailed player: %s"),
        *GetNameSafe(Player));
}