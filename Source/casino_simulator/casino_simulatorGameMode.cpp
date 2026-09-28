// Copyright Epic Games, Inc. All Rights Reserved.
#include "casino_simulatorGameMode.h"

#include "Enemy/ThiefCharacter.h"
#include "casino_simulatorPlayerState.h"
#include "GameFramework/Pawn.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
Acasino_simulatorGameMode::Acasino_simulatorGameMode()
{
	PlayerStateClass = Acasino_simulatorPlayerState::StaticClass();
    GameStateClass = ACasinoLoopGameState::StaticClass();
    bUseSeamlessTravel = true;
}

void Acasino_simulatorGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
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
    Status.Phase = ECasinoLoopPhase::Playing;
    Status.CurrentDay = Day;
    Status.FinalDay = FMath::Max(1, StartDay) + FMath::Max(1, DaysToPlay) - 1;
    Status.RequiredPayment = FMath::Max(1, DailyPayments.IsValidIndex(Day - 1)
        ? DailyPayments[Day - 1] : DefaultDailyPayment);
    const float Duration = FMath::Max(1.0f, DayDurationSeconds);
    Status.DayEndServerTime = GS->GetServerWorldTimeSeconds() + Duration;
    GetWorldTimerManager().SetTimer(DayLoopTimer, this,
        &Acasino_simulatorGameMode::BeginPaymentPhase, Duration, false);
    GS->SetLoopStatus(Status);
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
        if (bForPayment) Player->GetCharacterMovement()->DisableMovement();
        bAllMoved &= bMoved;
        PC->SetControlRotation(Point->GetActorRotation());
        if (auto* CasinoPC = Cast<Acasino_simulatorPlayerController>(PC))
        {
            if (bForPayment) CasinoPC->ClientPrepareDailyPayment(Point->GetActorRotation());
            else CasinoPC->ClientPrepareCasinoDay(Point->GetActorRotation());
        }
    }
    return bAllMoved;
}

void Acasino_simulatorGameMode::BeginPaymentPhase()
{
    auto* GS = GetGameState<ACasinoLoopGameState>();
    if (!GS || GS->LoopStatus.Phase != ECasinoLoopPhase::Playing) return;
    FCasinoLoopStatus Status = GS->LoopStatus;
    Status.Phase = ECasinoLoopPhase::Settling;
    Status.DayEndServerTime = 0.0;
    GS->SetLoopStatus(Status);
    // This event must finish synchronously before teleporting players.
    ForceEndCasinoGamesForDay();
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
    if (Status.CollectedPayment >= Status.RequiredPayment ||
        Status.PaymentSubmittedCount >= Status.PaymentParticipantCount) FinishPaymentPhase();
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
    if (!MovePlayersToCentralSpawns(false))
    {
        UE_LOG(LogTemp, Error, TEXT("Day loop: next-day teleport failed. Check central spawn points."));
        FCasinoLoopStatus Status = GS->LoopStatus;
        Status.Phase = ECasinoLoopPhase::GameOver;
        GS->SetLoopStatus(Status);
        return;
    }
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