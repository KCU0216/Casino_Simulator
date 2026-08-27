// LadderMachine.h  (서버 권위 버전)
//
// 결정론적 재생 모델 (경마와 동일):
//   - 서버가 결과(CurrentPlan)를 확정하고 돈을 정산한다.
//   - 입력(Nav*)은 Server RPC로 서버에 올린다.  설정값은 Replicated + OnRep로 클라 동기화.
//   - 판시작/결과/보드클리어 같은 일회성 이벤트는 Multicast RPC로 전 클라에 뿌린다(잔액도 같이 전달).
//   - 당첨금은 서버 타이머(TraceDuration)가 끝날 때(=구슬 도착 즈음) 지급 → 스포일러 방지.
//   - BP_LadderMachine 을 이 클래스로 재부모화해서 사용. 머신 Owner는 앉은 클라 컨트롤러로 SetOwner됨.

#pragma once

#include "CoreMinimal.h"
#include "LadderGenerator.h"   // FLadderPlan / FLadderRungRow / ULadderGenerator
#include "Machine/SeatedMachineBase.h"
#include "LadderMachine.generated.h"

/** 전환 버튼으로 순환하는 조작 모드. */
UENUM(BlueprintType)
enum class ELadderMode : uint8
{
	RailSelect  UMETA(DisplayName = "레일선택"),
	RailCount   UMETA(DisplayName = "줄개수"),
	Bet         UMETA(DisplayName = "배팅"),
	Refresh     UMETA(DisplayName = "새로고침"),
};

/** 배당 테이블 한 항목. 같은 Rails 값 여러 개 → 리롤 시 랜덤. Multipliers 길이는 Rails. 0 = 꽝. */
USTRUCT(BlueprintType)
struct FPayoutOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 Rails = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	TArray<int32> Multipliers;
};

// ── Dispatcher 선언 ──
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLadderIntEvent, int32, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLadderSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLadderModeEvent, ELadderMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLadderPlayEvent, FLadderPlan, Plan);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FLadderResultEvent, bool, bWin, int32, DestRail, int32, Multiplier, int32, Winnings);

UCLASS()
class CASINO_SIMULATOR_API ALadderMachine : public ASeatedMachineBase
{
	GENERATED_BODY()

public:
	ALadderMachine();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ─────────── Config (디자이너 편집) ───────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_RailConfig, Category = "Ladder")
	int32 Rails = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 RowsPerRail = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 MinRails = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	int32 MaxRails = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Result")
	TArray<FPayoutOption> PayoutTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Result")
	TArray<int32> BasePayouts = { 0, 0, 2, 3, 5 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float FakeRungChance = 0.4f;

	/** 트레이스(구슬) 연출 길이. 서버 정산 타이머가 이 값을 쓴다. 위젯 TraceDuration 과 같게 유지. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	float TraceDuration = 2.5f;

	// ─────────── Money ───────────
	// 잔액은 이 머신이 소유 안 함: CurrentUser(캐릭터)의 GAS Currency 가 진짜 잔액. 읽기는 GetBalance().
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Bet, Category = "Ladder|Money")
	int32 BetAmount = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Money")
	int32 BetStep = 100;

	// ─────────── Runtime state ───────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Ladder")
	int32 Rows = 12;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Payouts, Category = "Ladder|Result")
	TArray<int32> Payouts;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mode, Category = "Ladder|Nav")
	ELadderMode Mode = ELadderMode::RailSelect;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Cursor, Category = "Ladder|Nav")
	int32 RailCursor = 0;

	// 표시용 잔액. 서버가 CurrentUser 의 실제 잔액을 여기 써서 전 클라로 복제한다.
	// (GAS Currency 는 소유자 클라에만 복제되므로, 다른 클라가 잔액을 직접 못 읽는다 → 이 값이 필요.)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DisplayBalance, Category = "Ladder|Money")
	int32 DisplayBalance = 0;

	// 아래 3개는 리플리케이트 안 함 — 일회성 이벤트(Multicast)로 각 클라 로컬에 세팅된다.
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	bool bIsPlaying = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	bool bResultOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	FLadderPlan CurrentPlan;

	// ─────────── Dispatchers (뷰가 구독) ───────────
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderIntEvent    OnBalanceChanged;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderIntEvent    OnBetChanged;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderIntEvent    OnRailCountChanged;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderModeEvent   OnModeChanged;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderIntEvent    OnCursorChanged;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderSimpleEvent OnResultsChanged;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderSimpleEvent OnInsufficientFunds;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderSimpleEvent OnBoardCleared;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderPlayEvent   OnPlayStarted;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderResultEvent OnResult;
	UPROPERTY(BlueprintAssignable, Category = "Ladder|Events") FLadderSimpleEvent OnResultClosed;

	// ─────────── Config ops (authority에서만 실제 실행) ───────────
	UFUNCTION(BlueprintCallable, Category = "Ladder") void SetRailCount(int32 NewRails);
	UFUNCTION(BlueprintCallable, Category = "Ladder") void AddRail()    { SetRailCount(Rails + 1); }
	UFUNCTION(BlueprintCallable, Category = "Ladder") void RemoveRail() { SetRailCount(Rails - 1); }
	UFUNCTION(BlueprintCallable, Category = "Ladder|Result") void ShuffleResults();
	UFUNCTION(BlueprintPure,     Category = "Ladder|Result") int32 GetPayout(int32 Rail) const;

	// ─────────── Money ops ───────────
	UFUNCTION(BlueprintCallable, Category = "Ladder|Money") void ChangeBet(int32 Steps);
	UFUNCTION(BlueprintCallable, Category = "Ladder|Money") void ClampBet();

	/** 현재 잔액 = CurrentUser 의 Currency (정수 반올림). 사용자 없으면 0. */
	UFUNCTION(BlueprintPure, Category = "Ladder|Money") int32 GetBalance() const;

	// ─────────── Nav (입력 진입점 — 내부에서 서버로 라우팅) ───────────
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav") void SetRailCursor(int32 NewCursor);
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav") void NavCycle();
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav") void NavLeft();
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav") void NavRight();
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav") void NavSelect();
	UFUNCTION(BlueprintCallable, Category = "Ladder|Nav") void ResetRound();

	UFUNCTION(BlueprintCallable, Category = "Ladder") void BroadcastInitialState();

	UFUNCTION(BlueprintPure, Category = "Ladder") int32 GetRails() const { return Rails; }
	UFUNCTION(BlueprintPure, Category = "Ladder") int32 GetRows()  const { return Rows; }

protected:
	virtual void BeginPlay() override;

	// 점유 시작/해제 훅 오버라이드 — 서버에서 머신 Owner 세팅(클라 Server RPC 허용용).
	virtual void OnMachineReady_Implementation(Acasino_simulatorCharacter* RequestingCharacter) override;
	virtual void OnMachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter) override;

	// ── Server RPC (입력을 서버로) ──
	UFUNCTION(Server, Reliable) void Server_NavCycle();
	UFUNCTION(Server, Reliable) void Server_NavLeft();
	UFUNCTION(Server, Reliable) void Server_NavRight();
	UFUNCTION(Server, Reliable) void Server_NavSelect();
	UFUNCTION(Server, Reliable) void Server_SetRailCursor(int32 NewCursor);
	UFUNCTION(Server, Reliable) void Server_ResetRound();

	// ── Multicast RPC (일회성 이벤트를 전 클라로) ──
	UFUNCTION(NetMulticast, Reliable) void Multicast_BoardCleared();
	UFUNCTION(NetMulticast, Reliable) void Multicast_PlayStarted(FLadderPlan Plan, int32 BalanceAfterStake);
	UFUNCTION(NetMulticast, Reliable) void Multicast_Result(bool bWin, int32 DestRail, int32 Multiplier, int32 Winnings, int32 NewBalance);
	UFUNCTION(NetMulticast, Reliable) void Multicast_ResultClosed();

	// ── OnRep (config 동기화) ──
	UFUNCTION() void OnRep_RailConfig();
	UFUNCTION() void OnRep_Payouts();
	UFUNCTION() void OnRep_Bet();
	UFUNCTION() void OnRep_Mode();
	UFUNCTION() void OnRep_Cursor();
	UFUNCTION() void OnRep_DisplayBalance();

private:
	void BuildPayouts();
	TArray<int32> RollPayouts();
	void ClearBoard();               // authority
	void ServerStartPlay(int32 StartRail);   // authority
	void ServerReveal();                     // authority, 타이머 콜백

	void RefreshDisplayBalance();    // authority 전용: DisplayBalance = 서버 잔액 → 복제

	int32 StakedThisRound = 0;
	int32 PendingWinnings = 0;
	FTimerHandle RevealTimerHandle;
};
