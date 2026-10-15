// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorGameMode.generated.h"



class APawn;
class Acasino_simulatorCharacter;
class UPoliceEncounterComponent;
class ARaceManager;
class Acasino_simulatorPlayerController;
/**
 *  Simple GameMode for a first person game
 */
UCLASS(abstract)
class Acasino_simulatorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	Acasino_simulatorGameMode();
    virtual void PostLoad() override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Race")
	void StartRaceRound(ARaceManager* RaceManager);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Race")
    bool bEnableDailyRaceEvent = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Race",
        meta=(ClampMin="-1.0", Units="s", ToolTip="Seconds after active gameplay begins to start runner entry. Negative values keep the halfway-through-the-day schedule. Zero starts immediately. Values at or beyond the day duration skip the event."))
    float RaceEventStartDelaySeconds = -1.f;
    // Set explicitly before day activation if the level contains multiple managers.
    UPROPERTY(Transient, BlueprintReadWrite, Category="Casino|Race")
    TObjectPtr<ARaceManager> RaceEventManager;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loading", meta=(ClampMin="30.0", Units="s"))
    float OnlineLoadingTimeoutSeconds = 180.f;

    // Called by the owning controller's reliable server RPCs.
    void NotifyClientWorldReady(Acasino_simulatorPlayerController* Player, const FGuid& Generation);
    void NotifyClientSpawnReady(Acasino_simulatorPlayerController* Player, const FGuid& Generation);

	UFUNCTION(BlueprintCallable, Category = "Police")
	void ArrestPlayer(APawn* TargetPlayer, AActor* PoliceActor, AActor* JailPoint);

	UFUNCTION(BlueprintCallable, Category = "Police|Jail")
	bool PayBail(APawn* Player, float BailAmount);

	UFUNCTION(BlueprintCallable, Category = "Thief")
	bool StealMoney(APawn* TargetPlayer, AActor* ThiefActor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thief")
	int St_Money_Max = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thief")
	int St_Money_Min = 100;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1"))
    int32 StartDay = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1"))
    int32 DaysToPlay = 5;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1.0"))
    float DayDurationSeconds = 600.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="0.1"))
    float DayIntroDurationSeconds = 3.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1.0"))
    float PaymentDurationSeconds = 30.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="0.1"))
    float ResultDurationSeconds = 5.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop")
    bool bAutoStartDayLoop = true;
    // Preserve saved settings from the old per-day table; PostLoad keeps its first value.
    UPROPERTY(BlueprintReadOnly, Category="Casino|Loop", meta=(DeprecatedProperty, DeprecationMessage="Use First Day Payment and daily payment multipliers."))
    TArray<int32> DailyPayments;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1", DisplayName="First Day Payment"))
    int32 DefaultDailyPayment = 100;
    // Multiply the larger of the previous day's bill and the post-payment team balance.
    // Day 2 starts here; later days add the increase once per day.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="0.0"))
    double DailyPaymentBaseMultiplier = 1.5;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="0.0"))
    double DailyPaymentMultiplierIncreasePerDay = 0.0;
    // Place one tagged TargetPoint per player. Locations must be clear of collision.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop")
    FName PaymentSpawnTag = TEXT("CasinoPaymentSpawn");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1.0"))
    float PaymentRadius = 400.0f;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Casino|Loop")
    void StartDayLoop();
    UFUNCTION(BlueprintPure, Category="Casino|Loop")
    bool CanPlayCasino() const;
    bool RestartCasinoRun(APlayerController* Requester);
    // Server only; client UI calls the PlayerController RPC instead.
    bool SubmitDailyPayment(Acasino_simulatorCharacter* Player, int32 Amount);

    // Implement in existing GameMode BP: cancel all casino timers/payouts,
    // forfeit current wagers without charging twice, and release seats.
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|Loop")
    void ForceEndCasinoGamesForDay();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Casino | Police")
    TObjectPtr<UPoliceEncounterComponent> PoliceEncounter;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCasinoPaymentCompletionTest;
    friend class FCasinoMiningDayEndTest;
    friend class FCasinoAdaptivePaymentTest;
    friend class FCasinoPaymentGrowthFloorTest;
    friend class FRaceWorldEventTest;
    friend class FCasinoMatchLoadingBarrierTest;
    friend class FCasinoRunResultsTest;
#endif
    FTimerHandle RaceEventTimer;
    FTimerHandle RaceAnnouncementTimer;
    TWeakObjectPtr<ARaceManager> ScheduledRaceManager;
    double RaceEventStartServerTime = 0.0;
    void ScheduleRaceEvent(float DayDuration);
    void AnnounceRaceEvent();
    void StartScheduledRaceEvent();
    void CancelRaceEventTimers();
    FTimerHandle OnlineArrivalTimer;
    double OnlineArrivalDeadline = 0.0;
    int32 OnlineExpectedPlayers = 0;
    struct FOnlineLoadingParticipant
    {
        TWeakObjectPtr<Acasino_simulatorPlayerController> Player;
        TWeakObjectPtr<APawn> ReadyPawn;
        FTransform SpawnTransform;
        bool bWorldReady = false;
        bool bSpawnReady = false;
    };
    TArray<FOnlineLoadingParticipant> OnlineLoadingParticipants;
    TSet<TWeakObjectPtr<Acasino_simulatorPlayerController>> DepartedLoadingPlayers;
    bool bPreparingInitialSpawns = false;
    bool bInitialSpawnsPrepared = false;
    bool bCompletingInitialLoading = false;
    void BeginOnlineLoading(int32 ExpectedPlayers);
    void RegisterOnlineLoadingPlayer(Acasino_simulatorPlayerController* Player);
    void PublishOnlineLoadingCounts();
    void PrepareOnlineInitialSpawns();
    void FinishOnlineLoading();
    void FailOnlineLoading(const FText& Reason);
    bool GatherCentralSpawns();
    void WaitForOnlinePlayers();
    FTimerHandle IntroTimer;
    bool bRestartTravelPending = false;
    FTimerHandle DayLoopTimer;
    FTimerHandle PaymentTimer;
    FTimerHandle NextDayTimer;
    TArray<TWeakObjectPtr<AActor>> PaymentSpawns;
    TArray<TWeakObjectPtr<class Acasino_simulatorPlayerState>> PaymentParticipants;
    bool bCollectingPayment = false;
    int32 PreviousPaymentDay = 0;
    int32 PreviousDayRequiredPayment = 0;
    double PreviousDayRemainingCurrency = 0.0;
    // GameMode exists only on the server. Never copy these live records to GameState.
    struct FRunPlayerHistory
    {
        TWeakObjectPtr<class Acasino_simulatorPlayerState> PlayerState;
        TWeakObjectPtr<Acasino_simulatorCharacter> Pawn;
        FCasinoRunPlayerResult Result;
        int64 LastKnownBalance = 0;
        int32 PendingPaymentDay = 0;
        bool bHasKnownBalance = false;
    };
    TArray<FRunPlayerHistory> RunPlayerHistory;
    TArray<int32> RunRecordedDays;
    int32 NextRunPlayerId = 1;
    bool bRunHistoryStarted = false;
    FRunPlayerHistory& RegisterRunPlayer(class Acasino_simulatorPlayerState* PlayerState,
        Acasino_simulatorCharacter* Pawn = nullptr);
    void RefreshRunPlayerBalance(FRunPlayerHistory& Entry);
    void RegisterCurrentRunPlayers();
    void ResetRunResults();
    void RecordRunPaymentDay(int32 Day);
    void FinalizeRunResults();
    UFUNCTION()
    void HandleRunPawnDestroyed(AActor* DestroyedPawn);
    double CalculateTeamCurrency() const;
    int32 CalculateRequiredPayment(int32 Day) const;
    void BeginCasinoDay(int32 Day);
    void ActivateCasinoDay();
    void BeginPaymentPhase();
    void FinishPaymentPhase();
    void AdvanceCasinoDay();
    bool MovePlayersToCentralSpawns(bool bForPayment);
};



