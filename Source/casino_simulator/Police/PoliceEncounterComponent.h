#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "PoliceEncounterComponent.generated.h"

class APoliceCharacter;
class Acasino_simulatorCharacter;
class APawn;

// 경찰 이벤트의 진행 상태
UENUM()
enum class EPoliceEncounterState : uint8
{
    Inactive,   // 하루 시작 전 또는 정산으로 종료됨
    Waiting,    // 등장 시간 대기
    Arrival,    // 등장 컷신 진행
    Chasing,    // 추격 중
    Arresting,  // 체포 연출 진행
    Finished    // 오늘 경찰 활동 완료


};

UCLASS()
class CASINO_SIMULATOR_API UPoliceEncounterComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UPoliceEncounterComponent();
    
    void BeginPoliceDay(float RemainingDaySeconds);

    void EndPoliceDay();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

    void FinishPoliceIntro();
    void StartPoliceEncounter();
    Acasino_simulatorCharacter* SelectRandomPoliceTarget();

    UPROPERTY(EditDefaultsOnly, Category = "Casino|Police")
    bool bEnableDailyPoliceEncounter = true;

    UPROPERTY(EditDefaultsOnly, Category = "Casino|Police",
        meta = (ClampMin = "0.0", Units = "s"))
    float PoliceArrivalDelayMinSeconds = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Casino|Police",
        meta = (ClampMin = "0.0", Units = "s"))
    float PoliceArrivalDelayMaxSeconds = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Casino|Police",
        meta = (ClampMin = "0.1", Units = "s"))
    float PoliceIntroDurationSeconds = 4.0f;

    // 체포 후 감옥으로 이동하기 전 연출 시간
    UPROPERTY(EditDefaultsOnly, Category = "Casino | Police",
        meta = (ClampMin = "0.1", Units = "s"))
    float PoliceArrestDurationSeconds = 1.0f;

    UPROPERTY(VisibleInstanceOnly, Category = "Casino|Police")
    EPoliceEncounterState EncounterState =
        EPoliceEncounterState::Inactive;


    UFUNCTION()
    void HandlePoliceChaseReachedTarget(APawn* Target);

    void FinishPoliceArrest();

    // 맵에 배치된 경찰
    UPROPERTY(Transient)
    TObjectPtr<APoliceCharacter> EncounterPoliceActor;

    // 서버만 보관하는 추격 대상
    TWeakObjectPtr<Acasino_simulatorCharacter> PoliceChaseTarget;

    // 경찰의 최초 배치 위치와 회전
    FTransform PoliceInitialTransform = FTransform::Identity;

    // 등장 예약
    FTimerHandle PoliceArrivalTimerHandle;

    // 등장 컷신 종료
    FTimerHandle PoliceIntroTimerHandle;

    // 체포 연출 종료
    FTimerHandle PoliceArrestTimerHandle;
};