#pragma once

#include "CoreMinimal.h"
#include "Interaction/WorldInteractableBase.h"
#include "Interaction/CasinoDayParticipant.h"
#include "BailATM.generated.h"

class Acasino_simulatorCharacter;

// 납부 결과를 입력 UI에 전달할 때 사용합니다.
UENUM(BlueprintType)
enum class EBailPaymentResult : uint8
{
    Success,
    InsufficientFunds,
    InvalidAmount,
    Unavailable
};

// 금액이나 결제 상태가 바뀌면 열린 UI에 알려줍니다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBailStateChanged);

UCLASS(Blueprintable)
class CASINO_SIMULATOR_API ABailATM
    : public AWorldInteractableBase
    , public ICasinoDayParticipant
{
    GENERATED_BODY()

public:
    ABailATM();

    virtual bool CanInteract(
        Acasino_simulatorCharacter* InteractingCharacter) const override;

    // E키로 금액 입력창을 여는 역할입니다.
    virtual void Interact(
        Acasino_simulatorCharacter* InteractingCharacter) override;

    // 내부·외부 ATM이 함께 사용할 결제 담당 ATM을 반환합니다.
    UFUNCTION(BlueprintPure, Category = "Police|Bail")
    ABailATM* GetBailPaymentOwner() const;

    UFUNCTION(BlueprintPure, Category = "Police|Bail")
    float GetRemainingBailAmount() const;

    // 서버에서 지정 금액을 납부합니다.
    // OutPaidAmount에는 실제로 차감한 금액을 반환합니다.
    EBailPaymentResult SubmitBailPayment(
        Acasino_simulatorCharacter* InteractingCharacter,
        float RequestedAmount,
        float& OutPaidAmount);

    UPROPERTY(BlueprintAssignable, Category = "Police|Bail")
    FOnBailStateChanged OnBailStateChanged;

    // 기존 경찰 BP가 체포 완료 시 호출합니다.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Police|Bail")
    void ActivateBailPayment();

    virtual void BeginCasinoDay_Implementation() override;
    virtual void EndCasinoDay_Implementation() override;

protected:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 기존 BP 설정값을 첫날 보석금으로 사용합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Police|Bail", meta = (ClampMin = "1.0"))
    float BailAmount = 100.0f;

    // 외부 ATM에 내부 ATM을 지정합니다.
    // 내부 ATM은 비워둡니다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Police|Bail")
    TObjectPtr<ABailATM> PrimaryBailATM;

    // 둘째 날부터 적용할 전날 채무 비율입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Police|Bail",
        meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float BailDebtRatio = 0.5f;

    // 오늘의 총 보석금입니다.
    UPROPERTY(ReplicatedUsing = OnRep_BailState, BlueprintReadOnly,
        Category = "Police|Bail")
    float CurrentBailAmount = 100.0f;

    // 모든 플레이어가 납부한 금액의 합계입니다.
    UPROPERTY(ReplicatedUsing = OnRep_BailState, BlueprintReadOnly,
        Category = "Police|Bail")
    float PaidBailAmount = 0.0f;

    // 감옥문 참조는 내부 ATM에 지정합니다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Police|Bail")
    TObjectPtr<AActor> PrisonDoor;

    UPROPERTY(ReplicatedUsing = OnRep_BailState, BlueprintReadOnly,
        Category = "Police|Bail")
    bool bBailPaymentAvailable = false;

    UFUNCTION(BlueprintImplementableEvent, Category = "Police|Bail")
    void OnPrisonLocked();

    // 누적 납부액이 총 보석금을 채웠을 때만 호출합니다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Police|Bail")
    void OnBailPaid();

    UFUNCTION(BlueprintImplementableEvent, Category = "Police|Bail")
    void OnPrisonReset();

private:
    // 결제 상태와 누적 납부액을 초기화합니다. 환급은 하지 않습니다.
    void ResetPrison();

    UFUNCTION()
    void OnRep_BailState();

    // 서버의 UI 갱신과 클라이언트 복제를 요청합니다.
    void NotifyBailStateChanged();

    int32 PreviousDayRequiredPayment = 0;

    // 결제 도중 다시 결제가 실행되는 것을 막습니다.
    bool bProcessingBailPayment = false;
};