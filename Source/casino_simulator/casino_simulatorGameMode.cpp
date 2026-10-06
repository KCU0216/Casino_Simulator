// Copyright Epic Games, Inc. All Rights Reserved.
#include "casino_simulatorGameMode.h"
#include "Police/PoliceEncounterComponent.h"
#include "RaceGame/RaceManager.h"
#include "Enemy/ThiefCharacter.h"
#include "casino_simulatorPlayerState.h"
#include "GameFramework/Pawn.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/CasinoDayParticipant.h"
#include "NPC/NPC_Base.h"
#include "Machine/SeatedMachineBase.h"
#include "Interaction/WorldInteractableBase.h"
Acasino_simulatorGameMode::Acasino_simulatorGameMode()
{
	PlayerStateClass = Acasino_simulatorPlayerState::StaticClass();
    GameStateClass = ACasinoLoopGameState::StaticClass();
    bUseSeamlessTravel = true;

    PoliceEncounter = CreateDefaultSubobject<UPoliceEncounterComponent>(TEXT("PoliceEncounter"));
}

void Acasino_simulatorGameMode::PostLoad()
{
    Super::PostLoad();
    if (!DailyPayments.IsEmpty())
    {
        DefaultDailyPayment = FMath::Max(1, DailyPayments[0]);
        DailyPayments.Reset();
    }
}

void Acasino_simulatorGameMode::StartRaceRound(ARaceManager* RaceManager)
{
    if (HasAuthority() && IsValid(RaceManager) && RaceManager->GetWorld() == GetWorld())
    {
        if (RaceManager->Phase == ERacePhase::Finished) RaceManager->ResetRace();
        RaceManager->StartNewRound();
    }
}

void Acasino_simulatorGameMode::ScheduleRaceEvent(float DayDuration)
{
    CancelRaceEventTimers();
    if (!HasAuthority() || !CanPlayCasino() || !bEnableDailyRaceEvent) return;
    ARaceManager* Manager = RaceEventManager;
    if (!IsValid(Manager))
    {
        TArray<AActor*> Managers;
        UGameplayStatics::GetAllActorsOfClass(this, ARaceManager::StaticClass(), Managers);
        if (Managers.IsEmpty()) return;
        if (Managers.Num() != 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("Race event: assign RaceEventManager when multiple managers exist."));
            return;
        }
        Manager = Cast<ARaceManager>(Managers[0]);
    }
    if (Manager->GetWorld() != GetWorld()) return;
    ScheduledRaceManager = Manager;
    const float StartDelay = FMath::Max(1.f, DayDuration) * 0.5f;
    const auto* GS = GetGameState<ACasinoLoopGameState>();
    RaceEventStartServerTime = GS->GetServerWorldTimeSeconds() + StartDelay;
    GetWorldTimerManager().SetTimer(RaceEventTimer, this,
        &ThisClass::StartScheduledRaceEvent, StartDelay, false);
    const float AnnouncementDelay = FMath::Max(0.f, StartDelay - 10.f);
    if (AnnouncementDelay > 0.f)
        GetWorldTimerManager().SetTimer(RaceAnnouncementTimer, this,
            &ThisClass::AnnounceRaceEvent, AnnouncementDelay, false);
    else
        AnnounceRaceEvent();
}

void Acasino_simulatorGameMode::AnnounceRaceEvent()
{
    const ARaceManager* Manager = ScheduledRaceManager.Get();
    if (!HasAuthority() || !CanPlayCasino() || !IsValid(Manager)
        || (Manager->Phase != ERacePhase::Idle && Manager->Phase != ERacePhase::Finished)) return;
    const auto* GS = GetGameState<ACasinoLoopGameState>();
    const float Remaining = FMath::Max(0.f, static_cast<float>(RaceEventStartServerTime - GS->GetServerWorldTimeSeconds()));
    if (Remaining <= 0.f) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()))
            PC->ClientShowWorldEventAnnouncement();
}

void Acasino_simulatorGameMode::StartScheduledRaceEvent()
{
    ARaceManager* Manager = ScheduledRaceManager.Get();
    CancelRaceEventTimers();
    if (!HasAuthority() || !CanPlayCasino() || !IsValid(Manager)) return;
    StartRaceRound(Manager);
}

void Acasino_simulatorGameMode::CancelRaceEventTimers()
{
    GetWorldTimerManager().ClearTimer(RaceEventTimer);
    GetWorldTimerManager().ClearTimer(RaceAnnouncementTimer);
    ScheduledRaceManager.Reset();
    RaceEventStartServerTime = 0.0;
}

void Acasino_simulatorGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    if (UGameplayStatics::HasOption(OptionsString, TEXT("CasinoRestart")))
        if (auto* PS = NewPlayer->GetPlayerState<Acasino_simulatorPlayerState>()) PS->ResetForNewCasinoRun();
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    // Called for both new players and players retained through seamless travel.
    if (UGameplayStatics::HasOption(OptionsString, TEXT("CasinoOnlineMatch")))
    {
        if (auto* PC = Cast<Acasino_simulatorPlayerController>(NewPlayer))
            PC->ClientEnterCasinoMatch();
        UE_LOG(LogTemp, Log, TEXT("CasinoOnline: Match player %s, pawn %s, mode %s"),
            *GetNameSafe(NewPlayer), *GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr), *GetClass()->GetName());
    }
}

void Acasino_simulatorGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (UGameplayStatics::HasOption(OptionsString, TEXT("CasinoRestart"))) StartDay = 1;
    if (UGameplayStatics::HasOption(OptionsString, TEXT("CasinoOnlineMatch")))
    {
        OnlineExpectedPlayers = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("ExpectedPlayers"), 1), 1, 16);
        OnlineArrivalDeadline = FPlatformTime::Seconds() + 90.0;
        GetWorldTimerManager().SetTimer(OnlineArrivalTimer, this,
            &Acasino_simulatorGameMode::WaitForOnlinePlayers, 0.5f, true);
    }
    else if (bAutoStartDayLoop) StartDayLoop();
}

void Acasino_simulatorGameMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (UGameplayStatics::HasOption(OptionsString, TEXT("CasinoOnlineMatch")))
        ErrorMessage = TEXT("Match already started. Join the next lobby.");
}

void Acasino_simulatorGameMode::WaitForOnlinePlayers()
{
    int32 Arrived = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* PC = It->Get())
            if (PC->GetPawn() && PC->HasClientLoadedCurrentWorld()) ++Arrived;
    if (Arrived >= OnlineExpectedPlayers)
    {
        GetWorldTimerManager().ClearTimer(OnlineArrivalTimer);
        StartDayLoop();
    }
    else if (FPlatformTime::Seconds() >= OnlineArrivalDeadline)
    {
        GetWorldTimerManager().ClearTimer(OnlineArrivalTimer);
        UE_LOG(LogTemp, Error, TEXT("Online match: players did not finish loading within 90 seconds."));
        if (auto* GS = GetGameState<ACasinoLoopGameState>())
        {
            auto Status = GS->LoopStatus;
            Status.Phase = ECasinoLoopPhase::GameOver;
            GS->SetLoopStatus(Status);
        }
    }
}

void Acasino_simulatorGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelRaceEventTimers();
    GetWorldTimerManager().ClearTimer(IntroTimer);
    GetWorldTimerManager().ClearTimer(DayLoopTimer);
    GetWorldTimerManager().ClearTimer(PaymentTimer);
    GetWorldTimerManager().ClearTimer(NextDayTimer);
    GetWorldTimerManager().ClearTimer(OnlineArrivalTimer);
    Super::EndPlay(Reason);
}

bool Acasino_simulatorGameMode::CanPlayCasino() const
{
    const auto* GS = GetGameState<ACasinoLoopGameState>();
    return GS && GS->LoopStatus.Phase == ECasinoLoopPhase::Playing;
}

void Acasino_simulatorGameMode::StartDayLoop()
{
    auto* GS = GetGameState<ACasinoLoopGameState>();
    if (!HasAuthority() || !GS || GS->LoopStatus.Phase != ECasinoLoopPhase::Waiting) return;
    TArray<AActor*> Points;
    UGameplayStatics::GetAllActorsWithTag(this, PaymentSpawnTag, Points);
    Points.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
    PaymentSpawns.Reset();
    for (AActor* Point : Points) PaymentSpawns.Add(Point);
    if (PaymentSpawns.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Day loop: no actors tagged %s. Start cancelled."), *PaymentSpawnTag.ToString());
        return;
    }
    BeginCasinoDay(FMath::Max(1, StartDay));
}

int32 Acasino_simulatorGameMode::CalculateRequiredPayment(int32 Day) const
{
    if (Day <= 1) return FMath::Max(1, DefaultDailyPayment);

    double TeamCurrency = 0.0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get());
        const auto* PS = PC ? PC->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
        const auto* Player = PC ? Cast<Acasino_simulatorCharacter>(PC->GetPawn()) : nullptr;
        if (!IsValid(PS) || PS->IsInactive() || PS->IsOnlyASpectator() || !IsValid(Player)) continue;
        const float Currency = Player->GetCurrency();
        if (FMath::IsFinite(Currency) && Currency > 0.0f) TeamCurrency += Currency;
    }
    if (TeamCurrency <= 0.0) return 1;
    const double Base = FMath::IsFinite(DailyPaymentBaseMultiplier)
        ? FMath::Max(0.0, DailyPaymentBaseMultiplier) : 1.5;
    const double Increase = FMath::IsFinite(DailyPaymentMultiplierIncreasePerDay)
        ? FMath::Max(0.0, DailyPaymentMultiplierIncreasePerDay) : 0.0;
    const double Multiplier = Base + FMath::Max(0, Day - 2) * Increase;
    // Snapshot once at day start. Integer payments round up without overflowing the UI total.
    double Target = FMath::Clamp(TeamCurrency * Multiplier, 1.0, static_cast<double>(MAX_int32));
    const double Rounded = FMath::RoundToDouble(Target);
    // Decimal multipliers must not add a won solely due to floating-point roundoff.
    if (FMath::IsNearlyEqual(Target, Rounded, 1.e-6)) Target = Rounded;
    return static_cast<int32>(FMath::CeilToDouble(Target));
}

void Acasino_simulatorGameMode::BeginCasinoDay(int32 Day)
{
    auto* GS = GetGameState<ACasinoLoopGameState>();
    if (!GS) return;
    FCasinoLoopStatus Status;
    PaymentParticipants.Reset();
    for (APlayerState* BasePS : GS->PlayerArray)
        if (auto* PS = Cast<Acasino_simulatorPlayerState>(BasePS))
        {
            PS->bDailyPaymentSubmitted = false;
            PS->DailyPaymentAmount = 0;
            PS->ForceNetUpdate();
        }
    Status.Phase = ECasinoLoopPhase::DayIntro;
    Status.CurrentDay = Day;
    Status.FinalDay = FMath::Max(1, StartDay) + FMath::Max(1, DaysToPlay) - 1;
    Status.RequiredPayment = CalculateRequiredPayment(Day);
    Status.IntroEndServerTime = GS->GetServerWorldTimeSeconds() + FMath::Max(0.1f, DayIntroDurationSeconds);
    GS->SetLoopStatus(Status);
    if (!MovePlayersToCentralSpawns(false))
    {
        Status.Phase = ECasinoLoopPhase::GameOver;
        GS->SetLoopStatus(Status);
        return;
    }
    GetWorldTimerManager().SetTimer(IntroTimer, this,
        &Acasino_simulatorGameMode::ActivateCasinoDay, FMath::Max(0.1f, DayIntroDurationSeconds), false);
}

void Acasino_simulatorGameMode::ActivateCasinoDay()
{
    auto* GS = GetGameState<ACasinoLoopGameState>();
    if (!GS || GS->LoopStatus.Phase != ECasinoLoopPhase::DayIntro) return;
    FCasinoLoopStatus Status = GS->LoopStatus;
    Status.Phase = ECasinoLoopPhase::Playing;
    Status.IntroEndServerTime = 0.0;
    const float Duration = FMath::Max(1.0f, DayDurationSeconds);
    Status.DayEndServerTime = GS->GetServerWorldTimeSeconds() + Duration;
    GS->SetLoopStatus(Status);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()))
        {
            if (auto* Player = Cast<ACharacter>(PC->GetPawn())) Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            PC->ClientPrepareCasinoDay(PC->GetControlRotation());
        }
    GetWorldTimerManager().SetTimer(DayLoopTimer, this,
        &Acasino_simulatorGameMode::BeginPaymentPhase, Duration, false);
    TArray<AActor*> Games;
    UGameplayStatics::GetAllActorsWithInterface(this, UCasinoDayParticipant::StaticClass(), Games);
    for (AActor* Game : Games)
        if (IsValid(Game)) ICasinoDayParticipant::Execute_BeginCasinoDay(Game);


    if (IsValid(PoliceEncounter.Get()))
    {
        PoliceEncounter->BeginPoliceDay(GS->GetRemainingDaySeconds());
    }
    ScheduleRaceEvent(Duration);
}

bool Acasino_simulatorGameMode::MovePlayersToCentralSpawns(bool bForPayment)
{
    if (!HasAuthority()) return false;
    PaymentSpawns.RemoveAll([](const TWeakObjectPtr<AActor>& Point) { return !Point.IsValid(); });
    int32 Index = 0;
    bool bAllMoved = !PaymentSpawns.IsEmpty();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        auto* Player = PC ? Cast<Acasino_simulatorCharacter>(PC->GetPawn()) : nullptr;
        if (!Player) continue;
        if (!PaymentSpawns.IsValidIndex(Index)) { bAllMoved = false; continue; }
        AActor* Point = PaymentSpawns[Index++].Get();
        Player->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        const bool bMoved = Player->TeleportTo(Point->GetActorLocation(), Point->GetActorRotation());
        if (bForPayment || (GetGameState<ACasinoLoopGameState>() && GetGameState<ACasinoLoopGameState>()->LoopStatus.Phase == ECasinoLoopPhase::DayIntro))
            Player->GetCharacterMovement()->DisableMovement();
        bAllMoved &= bMoved;
        PC->SetControlRotation(Point->GetActorRotation());
        if (auto* CasinoPC = Cast<Acasino_simulatorPlayerController>(PC))
        {
            if (bForPayment) CasinoPC->ClientPrepareDailyPayment(Point->GetActorRotation());
            else if (GetGameState<ACasinoLoopGameState>()->LoopStatus.Phase == ECasinoLoopPhase::DayIntro)
                CasinoPC->ClientPrepareDayIntro(Point->GetActorRotation());
            else CasinoPC->ClientPrepareCasinoDay(Point->GetActorRotation());
        }
    }
    return bAllMoved;
}

void Acasino_simulatorGameMode::BeginPaymentPhase()
{
    auto* GS = GetGameState<ACasinoLoopGameState>();
    if (!GS || GS->LoopStatus.Phase != ECasinoLoopPhase::Playing) return;
    CancelRaceEventTimers();
    FCasinoLoopStatus Status = GS->LoopStatus;
    Status.Phase = ECasinoLoopPhase::Settling;
    Status.DayEndServerTime = 0.0;
    GS->SetLoopStatus(Status);


    if (IsValid(PoliceEncounter.Get()))
    {
        PoliceEncounter->EndPoliceDay();
    }


    // This event must finish synchronously before teleporting players.
    TArray<AActor*> Games;
    UGameplayStatics::GetAllActorsWithInterface(this, UCasinoDayParticipant::StaticClass(), Games);
    for (AActor* Game : Games)
        if (IsValid(Game)) ICasinoDayParticipant::Execute_EndCasinoDay(Game);
    ForceEndCasinoGamesForDay();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        auto* User = Cast<Acasino_simulatorCharacter>(It->Get()->GetPawn());
        if (!User) continue;
        UObject* Target = User->GetCurrentInteractionTarget().GetObject();
        if (auto* NPC = Cast<ANPC_Base>(Target)) NPC->ReleaseInteraction(User);
        else if (auto* Machine = Cast<ASeatedMachineBase>(Target))
        {
            Machine->SetCanExitMachine(true);
            Machine->RequestReleaseMachine(User);
        }
        else if (auto* Interactable = Cast<AWorldInteractableBase>(Target)) Interactable->RequestReleaseMachine(User);
    }
    PaymentParticipants.Reset();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()))
            if (auto* PS = PC->GetPlayerState<Acasino_simulatorPlayerState>())
                PaymentParticipants.Add(PS);
    Status.PaymentParticipantCount = PaymentParticipants.Num();
    Status.PaymentSubmittedCount = 0;
    const bool bAllMoved = MovePlayersToCentralSpawns(true);
    if (!bAllMoved)
    {
        UE_LOG(LogTemp, Error, TEXT("Day loop: payment teleport failed. Provide a clear tagged point per player."));
        Status.Phase = ECasinoLoopPhase::GameOver;
        GS->SetLoopStatus(Status);
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()))
                PC->ClientFinishDailyPayment(Status.Phase);
        return;
    }
    const float Duration = FMath::Max(1.0f, PaymentDurationSeconds);
    Status.PaymentEndServerTime = GS->GetServerWorldTimeSeconds() + Duration;
    GetWorldTimerManager().SetTimer(PaymentTimer, this,
        &Acasino_simulatorGameMode::FinishPaymentPhase, Duration, false);
    GS->SetLoopStatus(Status);
}

bool Acasino_simulatorGameMode::SubmitDailyPayment(Acasino_simulatorCharacter* Player, int32 Amount)
{
    auto* GS = GetGameState<ACasinoLoopGameState>();
    if (bCollectingPayment || !HasAuthority() || !GS || !IsValid(Player) || Amount < 0 ||
        GS->LoopStatus.Phase != ECasinoLoopPhase::Settling ||
        GS->GetRemainingPaymentSeconds() <= 0.0f) return false;
    auto* PS = Player->GetPlayerState<Acasino_simulatorPlayerState>();
    if (!PS || PS->bDailyPaymentSubmitted || !PaymentParticipants.Contains(PS)) return false;
    bool bNear = false;
    for (const auto& Point : PaymentSpawns)
        if (Point.IsValid() && FVector::DistSquared(Player->GetActorLocation(), Point->GetActorLocation())
            <= FMath::Square(FMath::Max(1.0f, PaymentRadius))) { bNear = true; break; }
    if (!bNear) return false;
    FCasinoLoopStatus Status = GS->LoopStatus;
    // Cap each contribution at the full daily target, not the remaining shared balance.
    const int32 Actual = FMath::Min(Amount, Status.RequiredPayment);
    if (Actual > MAX_int32 - Status.CollectedPayment) return false;
    TGuardValue<bool> PaymentGuard(bCollectingPayment, true);
    if (Actual > 0 && !Player->TrySpendCurrency(static_cast<float>(Actual))) return false;
    PS->bDailyPaymentSubmitted = true;
    PS->DailyPaymentAmount = Actual;
    PS->ForceNetUpdate();
    Status.CollectedPayment += Actual;
    ++Status.PaymentSubmittedCount;
    GS->SetLoopStatus(Status);
    // Send acknowledgement before the outcome so the UI cannot reopen waiting after closing.
    if (auto* PC = Cast<Acasino_simulatorPlayerController>(Player->GetController()))
        PC->ClientDailyPaymentResult(true);
    // Reaching the shared target does not remove the other players' chance to contribute.
    // Finish only after everyone submits (including explicit zero), or the deadline timer fires.
    if (Status.PaymentSubmittedCount >= Status.PaymentParticipantCount) FinishPaymentPhase();
    return true;
}

void Acasino_simulatorGameMode::FinishPaymentPhase()
{
    auto* GS = GetGameState<ACasinoLoopGameState>();
    if (!GS || GS->LoopStatus.Phase != ECasinoLoopPhase::Settling) return;
    GetWorldTimerManager().ClearTimer(PaymentTimer);
    FCasinoLoopStatus Status = GS->LoopStatus;
    for (const auto& Entry : PaymentParticipants)
        if (auto* PS = Entry.Get())
            if (!PS->bDailyPaymentSubmitted)
            {
                PS->bDailyPaymentSubmitted = true;
                PS->DailyPaymentAmount = 0;
                PS->ForceNetUpdate();
            }
    // Disconnected participants also count as zero when the deadline expires.
    Status.PaymentSubmittedCount = Status.PaymentParticipantCount;
    Status.PaymentEndServerTime = 0.0;
    Status.Phase = Status.CollectedPayment < Status.RequiredPayment ? ECasinoLoopPhase::GameOver
        : (Status.CurrentDay >= Status.FinalDay ? ECasinoLoopPhase::Cleared : ECasinoLoopPhase::DayPassed);
    GS->SetLoopStatus(Status);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (auto* PC = Cast<Acasino_simulatorPlayerController>(It->Get()))
            PC->ClientFinishDailyPayment(Status.Phase);
    if (Status.Phase == ECasinoLoopPhase::DayPassed)
        GetWorldTimerManager().SetTimer(NextDayTimer, this,
            &Acasino_simulatorGameMode::AdvanceCasinoDay, FMath::Max(0.1f, ResultDurationSeconds), false);
}

void Acasino_simulatorGameMode::AdvanceCasinoDay()
{
    auto* GS = GetGameState<ACasinoLoopGameState>();
    if (!HasAuthority() || !GS || GS->LoopStatus.Phase != ECasinoLoopPhase::DayPassed) return;
    BeginCasinoDay(GS->LoopStatus.CurrentDay + 1);
}

void Acasino_simulatorGameMode::ArrestPlayer(
    APawn* TargetPlayer,
    AActor* PoliceActor,
    AActor* JailPoint)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!IsValid(TargetPlayer) || !IsValid(JailPoint))
    {
        return;
    }

    TargetPlayer->SetActorLocation(
        JailPoint->GetActorLocation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    if (IsValid(PoliceActor))
    {
        PoliceActor->Destroy();
    }
}

bool Acasino_simulatorGameMode::PayBail(
    APawn* Player,
    float BailAmount)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!IsValid(Player) || BailAmount < 0.0f)
    {
        return false;
    }

    Acasino_simulatorCharacter* CasinoPlayer =
        Cast<Acasino_simulatorCharacter>(Player);

    if (!CasinoPlayer)
    {
        return false;
    }

    return CasinoPlayer->TrySpendCurrency(BailAmount);
}

bool Acasino_simulatorGameMode::StealMoney(
    APawn* TargetPlayer,
    AActor* ThiefActor)
{
    if (!HasAuthority())
    {
        return false;
    }

    Acasino_simulatorCharacter* Player =
        Cast<Acasino_simulatorCharacter>(TargetPlayer);

    AThiefCharacter* Thief =
        Cast<AThiefCharacter>(ThiefActor);

    if (!IsValid(Player) || !IsValid(Thief) || !Thief->CanSteal())
    {
        return false;
    }

    const int32 StealAmount =
        FMath::RandRange(St_Money_Min / 50, St_Money_Max / 50) * 50;

    return Thief->TryStealFrom(
        Player, static_cast<float>(StealAmount));
}
bool Acasino_simulatorGameMode::RestartCasinoRun(APlayerController* Requester)
{
    const auto* GS = GetGameState<ACasinoLoopGameState>();
    if (!HasAuthority() || bRestartTravelPending || !Requester || !Requester->IsLocalController() ||
        Requester->GetWorld() != GetWorld() || !GS ||
        (GS->LoopStatus.Phase != ECasinoLoopPhase::GameOver && GS->LoopStatus.Phase != ECasinoLoopPhase::Cleared)) return false;
    // Keep the EOS session and connected players. The map (including pawns/ASCs/games) is recreated.
    // AGameModeBase replaces controllers/PlayerStates and copies identity; reset run flags on the new states.
    const FString Map = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
    const FString URL = Map + TEXT("?game=") + GetClass()->GetPathName()
        + FString::Printf(TEXT("?SeamlessTravel?CasinoOnlineMatch=1?CasinoRestart=1?ExpectedPlayers=%d"), GetNumPlayers());
#if WITH_EDITOR
    if (GetWorld()->WorldType == EWorldType::PIE)
        if (IConsoleVariable* AllowTravel = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel")))
            AllowTravel->Set(1, ECVF_SetByCode);
#endif
    bUseSeamlessTravel = true;
    bRestartTravelPending = GetWorld()->ServerTravel(URL, true);
    return bRestartTravelPending;
}

