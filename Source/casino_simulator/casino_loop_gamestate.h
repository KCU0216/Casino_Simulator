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
    Cleared     UMETA(DisplayName = "게임 클리어")
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
    int32 CollectedPayment = 0;

    UPROPERTY(BlueprintReadOnly)
    double PaymentEndServerTime = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCasinoLoopChanged);

UCLASS()
class ACasinoLoopGameState : public AGameStateBase
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

    UFUNCTION(BlueprintPure, Category = "Casino|Loop")
    float GetRemainingDaySeconds() const;

    UFUNCTION(BlueprintPure, Category = "Casino|Loop")
    float GetRemainingPaymentSeconds() const;

    void SetLoopStatus(const FCasinoLoopStatus& NewStatus);

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

private:
    UFUNCTION()
    void OnRep_LoopStatus();
};
