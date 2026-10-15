#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "casino_loop_gamestate.generated.h"

UENUM(BlueprintType)
enum class ECasinoLoopPhase : uint8
{
    Waiting     UMETA(DisplayName = "시작 대기"),
    Playing     UMETA(DisplayName = "하루 진행"),
    Settling    UMETA(DisplayName = "납부 처리 중"),
    DayPassed   UMETA(DisplayName = "오늘 납부 성공"),
    GameOver    UMETA(DisplayName = "납부 실패"),
    Cleared     UMETA(DisplayName = "게임 클리어"),
    DayIntro    UMETA(DisplayName = "날짜 안내")
};

USTRUCT(BlueprintType)
struct FCasinoLoopStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ECasinoLoopPhase Phase = ECasinoLoopPhase::Waiting;

    UPROPERTY(BlueprintReadOnly)
    int32 CurrentDay = 1;

    UPROPERTY(BlueprintReadOnly)
    int32 FinalDay = 1;

    UPROPERTY(BlueprintReadOnly)
    int32 RequiredPayment = 0;

    UPROPERTY(BlueprintReadOnly)
    double DayEndServerTime = 0.0;

    UPROPERTY(BlueprintReadOnly)
    double IntroEndServerTime = 0.0;

    UPROPERTY(BlueprintReadOnly)
    int32 CollectedPayment = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 PaymentParticipantCount = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 PaymentSubmittedCount = 0;

    UPROPERTY(BlueprintReadOnly)
    double PaymentEndServerTime = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCasinoLoopChanged);

UENUM(BlueprintType)
enum class ECasinoMatchLoadingStage : uint8
{
    None,
    WaitingForPlayers,
    PreparingSpawns,
    Complete,
    Failed
};

USTRUCT(BlueprintType)
struct FCasinoMatchLoadingStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid Generation;
    UPROPERTY(BlueprintReadOnly)
    ECasinoMatchLoadingStage Stage = ECasinoMatchLoadingStage::None;
    UPROPERTY(BlueprintReadOnly)
    int32 ExpectedPlayers = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 ArrivedPlayers = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 ReadyPlayers = 0;
    UPROPERTY(BlueprintReadOnly)
    FText FailureReason;

    bool IsPending() const
    {
        return Stage == ECasinoMatchLoadingStage::WaitingForPlayers
            || Stage == ECasinoMatchLoadingStage::PreparingSpawns;
    }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCasinoMatchLoadingChanged);

USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FCasinoRunBalanceSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    int32 Day = 0;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    int64 Balance = 0;
};

USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FCasinoRunPlayerResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    int32 RunPlayerId = 0;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    FString PlayerName;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    TArray<FCasinoRunBalanceSnapshot> DailyBalances;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    int64 FinalBalance = 0;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    int64 TotalContribution = 0;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    int32 Rank = 0;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    bool bDisconnected = false;
};

USTRUCT(BlueprintType)
struct CASINO_SIMULATOR_API FCasinoRunResults
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    bool bFinalized = false;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    TArray<int32> Days;
    UPROPERTY(BlueprintReadOnly, Category="Casino|Results")
    TArray<FCasinoRunPlayerResult> Players;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCasinoRunResultsChanged);

UCLASS()
class CASINO_SIMULATOR_API ACasinoLoopGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    UPROPERTY(
        BlueprintReadOnly,
        ReplicatedUsing = OnRep_LoopStatus,
        Category = "Casino|Loop"
    )
    FCasinoLoopStatus LoopStatus;

    UPROPERTY(BlueprintAssignable, Category = "Casino|Loop")
    FOnCasinoLoopChanged OnLoopChanged;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MatchLoadingStatus, Category="Casino|Loading")
    FCasinoMatchLoadingStatus MatchLoadingStatus;

    UPROPERTY(BlueprintAssignable, Category="Casino|Loading")
    FOnCasinoMatchLoadingChanged OnMatchLoadingChanged;

    void SetMatchLoadingStatus(const FCasinoMatchLoadingStatus& NewStatus);

    // The server keeps daily records privately until the run has ended.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_RunResults, Category="Casino|Results")
    FCasinoRunResults RunResults;

    UPROPERTY(BlueprintAssignable, Category="Casino|Results")
    FOnCasinoRunResultsChanged OnRunResultsChanged;

    void SetRunResults(const FCasinoRunResults& NewResults);

    UFUNCTION(BlueprintPure, Category = "Casino|Loop")
    float GetRemainingDaySeconds() const;

    UFUNCTION(BlueprintPure, Category="Casino|Loop")
    float GetRemainingIntroSeconds() const;

    UFUNCTION(BlueprintPure, Category = "Casino|Loop")
    float GetRemainingPaymentSeconds() const;

    void SetLoopStatus(const FCasinoLoopStatus& NewStatus);

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

private:
    UFUNCTION()
    void OnRep_LoopStatus();
    UFUNCTION()
    void OnRep_MatchLoadingStatus();
    UFUNCTION()
    void OnRep_RunResults();
};
