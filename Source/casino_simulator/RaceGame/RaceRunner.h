// RaceRunner.h  ─ 경주 러너 (AActor).
// [결정론 재생] 서버가 레시피(RaceScript)만 복제 → 클라가 매 프레임 스스로 위치 계산.
//              위치 스트리밍(SetReplicateMovement) 안 씀.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceTypes.h"
#include "RaceRunner.generated.h"

class USkeletalMeshComponent;
class UAnimMontage;

UCLASS()
class CASINO_SIMULATOR_API ARaceRunner : public AActor
{
	GENERATED_BODY()

public:
	ARaceRunner();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 스탯 (이름/나이/배당 — UI용). 서버가 세팅, 리플리케이트.
	UPROPERTY(ReplicatedUsing = OnRep_Stats, BlueprintReadOnly, Category = "Race")
	FRaceRunnerStats Stats;

	// 이번 판 주행 레시피. 서버가 시작 때 1번 세팅, 리플리케이트.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Race")
	FRunnerRaceScript RaceScript;

	// 주행 중? true 되는 순간이 각 클라의 "출발 신호".
	UPROPERTY(ReplicatedUsing = OnRep_Racing, BlueprintReadOnly, Category = "Race")
	bool bRacing = false;
	// 각 러너의 달리기 상태
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool  bIsRunning = false;

	// ── 서버 전용 세팅 함수 ──
	void InitStats(const FRaceRunnerStats& In);
	void ServerSetupScript(const FRunnerRaceScript& S);   // 레시피 주입 + 리셋 + 출발선 배치
	void ServerSetRacing(bool bNew);                       // 출발/정지

	// 매니저(서버)가 승자 판정에 참고
	float GetPosUnits() const { return PosUnits; }

	// BP 연출 훅
	UFUNCTION(BlueprintImplementableEvent, Category = "Race") void OnStatsUpdated();
	UFUNCTION(BlueprintImplementableEvent, Category = "Race") void OnAwakenFX();
	UFUNCTION(BlueprintImplementableEvent, Category = "Race") void OnStumbleFX();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Runner|Character") USceneComponent* SceneRoot;
	UPROPERTY(EditAnywhere,   BlueprintReadOnly, Category = "Runner|Character") USkeletalMeshComponent* Skin;
	UPROPERTY(EditAnywhere,   BlueprintReadOnly, Category = "Runner|Character") UAnimMontage* RunAnim;

	UFUNCTION() void OnRep_Stats();
	UFUNCTION() void OnRep_Racing();

	void ResetVisual();

	// 로컬 시뮬 상태 (복제 X — 서버·클라 각자 레시피로 계산)
	float LocalTime = 0.f;
	float PosUnits = 0.f;
	bool  bAwakenedLocal = false;
	bool  bStumbledLocal = false;
	float StumbleUntil = 0.f;
	
};
