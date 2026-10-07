#pragma once

#include "CoreMinimal.h"
#include "Interaction/WorldInteractableBase.h"
#include "Interaction/CasinoDayParticipant.h"
#include "BailATM.generated.h"

class Acasino_simulatorCharacter;

UCLASS(Blueprintable)
class CASINO_SIMULATOR_API ABailATM
    : public AWorldInteractableBase
    , public ICasinoDayParticipant
{
    GENERATED_BODY()

public :
    ABailATM();

    // 기존 상호작용 시스템이 호출합니다.
    // 플레이어가 범위 안에 있고 보석금 결제가 가능한지 판단합니다.
    virtual bool CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const override;


    // E 상호작용 요청을 처리합니다.
    // 서버에서 상호작용한 플레이어의 돈을 검사하고 차감합니다.
    virtual void Interact(Acasino_simulatorCharacter* InteractingCharacter) override;

    // 체포 완료 시 경찰 BP가 호출합니다.
    // ATM 결제를 활성화하고 감옥문 닫기 이벤트를 실행합니다.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Police|Bail")
    void ActivateBailPayment();

    // 기존 게임모드가 하루 시작 시 호출합니다.
    // 결제를 비활성화하고 감옥문을 기본 열림 상태로 돌립니다.
    virtual void BeginCasinoDay_Implementation() override;

    // 기존 게임모드가 하루 종료 시 호출합니다.
    // 미결제 상태라도 ATM과 감옥문을 초기화합니다.
    virtual void EndCasinoDay_Implementation() override;

protected:
    // bBailPaymentAvilable을 복제할 변수로 엔진에 등록합니다.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // BP_ATM의 Class Defaults에서 설정할 보석금입니다.
    // 100은 임시 기본값 입니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Police|Bail", meta = (ClampMin = "1.0"))
    float BailAmount = 100.0f;

    // 맵에 배치된 실제 BP_Prison_Door를 지정합니다.
    // BP 이벤트에서 이 문을 대상으로 함수를 호출합니다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Police|Bail")
    TObjectPtr<AActor> PrisonDoor;

    // 기본 false, 체포 후 true, 결제 성공이나 초기화 시 false입니다.
    // 클라이언트도 이 값을 받아 상호작용 가능 여부를 판단합니다.
    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bBailPaymentAvailable = false;

    // 체포 시 감옥문의 CloseDoor를 호출합니다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Police|Bail")
    void OnPrisonLocked();

    // 결제 성공 시 감옥문의 OpenDoor를 호출합니다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Police|Bail")
    void OnBailPaid();

    // 초기화 시 감옥문의 ResetDoor를 호출합니다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Police|Bail")
    void OnPrisonReset();

private:
    // 하루 시작과 종료가 함께 사용하는 내부 초기화 함수입니다.
    // 결제 가능 상태를 끄고 OnPrisonReset을 실행합니다.
    void ResetPrison();
};
