// RaceManager.h  ─ 레이스 진행 관리(서버 권위). 레벨에 배치.
// [결정론 재생] 시작 때 각 러너 레시피 롤 → 복제. 승자는 서버가 시뮬로 확정 → WinnerIndex 복제.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceTypes.h"
#include "NPC/NPC_InteractionCameraBase.h"
#include "Components/SceneComponent.h"
#include "RaceManager.generated.h"

class ARaceRunner;
class Acasino_simulatorCharacter;
class UWidgetComponent;
class URaceBillboardWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRacePhaseChanged, ERacePhase, NewPhase);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRaceLineupReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRaceStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRaceLineupExit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRaceFinished, ARaceRunner*, Winner, int32, WinnerIndex);


UCLASS()
class CASINO_SIMULATOR_API ARaceManager : public AActor
{
	GENERATED_BODY()

public:
	ARaceManager();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race") TSubclassOf<ARaceRunner> RunnerClass;

	UPROPERTY(VisibleAnywhere, Category = "Race") USceneComponent* SceneRoot;
	UPROPERTY(EditAnywhere, Category = "Race|Track") UStaticMeshComponent* Track;
	// 레벨에 배치한 스폰 지점들. 순서대로 러너 스폰 (0번 지점 = 0번 러너).
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Race|Track") TArray<AActor*> RunnerSpawnPoints;
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track") float   TrackLength = 2700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track", meta = (ClampMin = "0.0"))
	float EnterDistance = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track", meta = (ClampMin = "0.0"))
	float ExitDistance = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track", meta = (ClampMin = "1.0"))
	float TransitSpeed = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_BillboardData, Category = "Race", meta = (ClampMin = "0.0"))
	float BettingDuration = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Odds")  float   HouseMargin = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC") TSubclassOf<ANPC_InteractionCameraBase> NPCClass;
	UPROPERTY(EditDefaultsOnly, Category = "Race|NPC")  ANPC_InteractionCameraBase* RaceNPC;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Race|NPC") USceneComponent* NPCSpawnPoints;

	

	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "Race") ERacePhase Phase = ERacePhase::Idle;
	UPROPERTY(ReplicatedUsing=OnRep_BillboardData, BlueprintReadOnly, Category = "Race") int32 WinnerIndex = -1;
	// 완주 순위: FinishOrder[0]=1등, [1]=2등 ... (러너 인덱스). 서버가 확정, 복제.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Race") TArray<int32> FinishOrder;
	// 마권 원장 (서버 권위, 모두에게 복제)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Race|Bet") TArray<FBetTicket> Tickets;
	//현재 경마 라운드
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Race") int32 CurrentRoundNumber = 0;

	UPROPERTY(BlueprintAssignable, Category = "Race") FOnRaceLineupReady OnLineupReady;
	UPROPERTY(BlueprintAssignable, Category = "Race") FOnRaceStarted OnRaceStarted;

	UPROPERTY(BlueprintAssignable, Category = "Race") FOnRaceLineupExit OnLineupExit;
	UPROPERTY(BlueprintAssignable, Category = "Race") FOnRaceFinished OnRaceFinished;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Race|Billboard")
    TObjectPtr<UWidgetComponent> BillboardWidget;

    UPROPERTY(BlueprintAssignable, Category="Race|Billboard")
    FOnRacePhaseChanged OnPhaseChanged;
    // Local presentation hook on server/listen host and replicated clients.
    UFUNCTION(BlueprintImplementableEvent, Category="Race|Billboard")
    void OnBillboardPhaseChanged(ERacePhase NewPhase);
    UFUNCTION(BlueprintPure, Category="Race|Billboard")
    URaceBillboardWidget* GetBillboardWidget() const;
    UFUNCTION(BlueprintPure, Category="Race|Billboard")
    ARaceRunner* GetLeadingRunner() const;
    UFUNCTION(BlueprintPure, Category="Race|Billboard")
    float GetRemainingBettingSeconds() const;

    // 새 라운드: 러너 스폰 + 스탯 롤 + 배당 공개 (서버)
	UFUNCTION(BlueprintCallable, Category = "Race") void StartNewRound();
	UFUNCTION(BlueprintCallable, Category = "Race") void Entering();
	UFUNCTION(BlueprintCallable, Category = "Race") void Exiting();
	// 배팅 마감 → 레이스 시작: 레시피 롤 + 승자 확정 + 출발 (서버)
	UFUNCTION(BlueprintCallable, Category = "Race") void StartRace();
	UFUNCTION(BlueprintCallable, Category = "Race") void ResetRace();

	UFUNCTION(BlueprintCallable, Category = "Race") const TArray<ARaceRunner*>& GetRunners() const { return Runners; }
	UFUNCTION(BlueprintCallable, Category = "Race") ARaceRunner* GetWinner() const;

	// ── 마권 (서버 권위) ──
	UFUNCTION(BlueprintCallable, Category = "Race|Bet") bool ServerBuyTicket(Acasino_simulatorCharacter* Player, int32 RunnerIndex, int32 Amount, int32 Count);
	UFUNCTION(BlueprintCallable, Category = "Race|Bet") int32 ServerClaimWinnings(Acasino_simulatorCharacter* Player);
	UFUNCTION(BlueprintCallable, Category = "Race|Bet") TArray<FBetTicket> GetTicketsForPlayer(APlayerState* PS) const;
	UFUNCTION(BlueprintCallable, Category = "Race|Bet") float GetOdds(int32 RunnerIndex) const;

protected:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRaceEntranceFlowTest;
    friend class FRaceBillboardFlowTest;
#endif
	virtual void BeginPlay() override;
    UFUNCTION() void OnRep_Phase();
    UFUNCTION() void OnRep_BillboardData();
    void RefreshBillboard();
    bool bBillboardPhaseInitialized = false;
    ERacePhase LastBillboardPhase = ERacePhase::Idle;

	// 서버가 스폰한 러너의 순서를 클라이언트 UI에도 전달한다.
	UPROPERTY(ReplicatedUsing=OnRep_BillboardData, BlueprintReadOnly, Category = "Race") TArray<ARaceRunner*> Runners;

	int32 NextTicketId = 1;
	void SettleTickets();   // 단승 정산: 진 마권 삭제, 당첨 마권 유지

	FRaceRunnerStats  RollStats(int32 LaneIndex) const;
	FRunnerRaceScript RollScript(const FRaceRunnerStats& S, const FVector& StartLoc, const FVector& Dir) const;

	float RaceElapsed = 0.f;
	float RaceDuration = 0.f;
	bool  bResultBroadcast = false;
	double EnterStartServerTime = 0.0;
	float EnterDuration = 0.f;
	double RaceStartServerTime = 0.0;
    UPROPERTY(ReplicatedUsing=OnRep_BillboardData)
    double BettingStartServerTime = 0.0;
	double ExitStartServerTime = 0.0;
	float ExitDuration = 0.f;
	double GetServerTime() const;
	void BeginBetting();
	void FinishExiting();
};
