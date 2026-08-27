// LadderMachine.cpp  (서버 권위 버전)

#include "LadderMachine.h"
#include "casino_simulatorCharacter.h"   // GetCurrency / TrySpendCurrency / AddCurrency
#include "Net/UnrealNetwork.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "Engine/World.h"

ALadderMachine::ALadderMachine()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;   // 설정값 리플리케이션 + RPC 사용

	// 기본 배당 테이블 (합계 = 줄 개수 → 공정 EV).
	auto Add = [this](int32 R, const TArray<int32>& M)
	{
		FPayoutOption O; O.Rails = R; O.Multipliers = M; PayoutTable.Add(O);
	};
	Add(3, { 3, 3, 3 });
	Add(4, { 0, 0, 2, 2 });
	Add(4, { 0, 0, 0, 4 });
	Add(4, { 0, 0, 1, 3 });
	Add(5, { 0, 0, 0, 1, 4 });
	Add(5, { 0, 0, 0, 0, 5 });
	Add(5, { 0, 0, 0, 2, 3 });
}

void ALadderMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALadderMachine, Rails);
	DOREPLIFETIME(ALadderMachine, Rows);
	DOREPLIFETIME(ALadderMachine, Payouts);
	DOREPLIFETIME(ALadderMachine, BetAmount);
	DOREPLIFETIME(ALadderMachine, Mode);
	DOREPLIFETIME(ALadderMachine, RailCursor);
	DOREPLIFETIME(ALadderMachine, DisplayBalance);
}

void ALadderMachine::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SetRailCount(Rails);   // 서버가 초기 판 세팅 → 클라로 리플리케이트
	}
}

// 점유 시작: 서버에서 머신 Owner 를 앉은 클라 컨트롤러로 → 그 클라의 Server RPC 허용.
void ALadderMachine::OnMachineReady_Implementation(Acasino_simulatorCharacter* RequestingCharacter)
{
	Super::OnMachineReady_Implementation(RequestingCharacter);
	if (HasAuthority() && RequestingCharacter)
	{
		SetOwner(RequestingCharacter->GetController());
		// 서버가 확정 잔액을 DisplayBalance 로 → 복제되어 전 클라(앉은 클라 포함)가 동일 표시.
		RefreshDisplayBalance();
	}
}

void ALadderMachine::OnMachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter)
{
	Super::OnMachineReleased_Implementation(ReleasingCharacter);
	if (HasAuthority())
	{
		SetOwner(nullptr);
		DisplayBalance = 0;   // 자리 비면 0 → 복제
		OnRep_DisplayBalance();
	}
}

void ALadderMachine::BroadcastInitialState()
{
	OnRailCountChanged.Broadcast(Rails);
	OnResultsChanged.Broadcast();
	OnModeChanged.Broadcast(Mode);
	OnCursorChanged.Broadcast(RailCursor);
	OnBetChanged.Broadcast(BetAmount);
	OnBalanceChanged.Broadcast(DisplayBalance);   // 로컬 GetBalance() 대신 복제값 사용
	OnBoardCleared.Broadcast();
}

// ─────────── Config ops (authority) ───────────

void ALadderMachine::SetRailCount(int32 NewRails)
{
	if (!HasAuthority()) return;

	Rails = FMath::Clamp(NewRails, MinRails, MaxRails);
	Rows  = FMath::Max(Rails * RowsPerRail, 2);
	RailCursor = FMath::Clamp(RailCursor, 0, Rails - 1);

	ClearBoard();       // 판 비움 (Multicast_BoardCleared)
	BuildPayouts();     // 배당 재구성

	OnRep_RailConfig(); // 호스트 로컬 UI (클라는 리플리케이션으로 각자 OnRep)
	OnRep_Payouts();
	OnRep_Cursor();
}

void ALadderMachine::BuildPayouts()
{
	Payouts = RollPayouts();
}

void ALadderMachine::ShuffleResults()
{
	if (!HasAuthority()) return;
	Payouts = RollPayouts();
	OnRep_Payouts();
}

TArray<int32> ALadderMachine::RollPayouts()
{
	TArray<int32> MatchIdx;
	for (int32 i = 0; i < PayoutTable.Num(); ++i)
	{
		if (PayoutTable[i].Rails == Rails) { MatchIdx.Add(i); }
	}

	TArray<int32> Result;
	if (MatchIdx.Num() > 0)
	{
		const int32 Pick = MatchIdx[FMath::RandRange(0, MatchIdx.Num() - 1)];
		Result = PayoutTable[Pick].Multipliers;
	}
	else
	{
		for (int32 i = 0; i < Rails; ++i)
		{
			Result.Add(BasePayouts.IsValidIndex(i) ? BasePayouts[i] : 0);
		}
	}

	Result.SetNum(Rails);

	for (int32 i = Result.Num() - 1; i > 0; --i)
	{
		Result.Swap(i, FMath::RandRange(0, i));
	}
	return Result;
}

int32 ALadderMachine::GetPayout(int32 Rail) const
{
	return Payouts.IsValidIndex(Rail) ? Payouts[Rail] : 0;
}

// ─────────── Money ───────────

int32 ALadderMachine::GetBalance() const
{
	if (const Acasino_simulatorCharacter* User = GetCurrentUser())
	{
		return FMath::RoundToInt(User->GetCurrency());
	}
	return 0;
}

void ALadderMachine::ClampBet()
{
	if (!HasAuthority()) return;
	const int32 MaxBet = FMath::Max(GetBalance(), BetStep);
	BetAmount = FMath::Clamp(BetAmount, BetStep, MaxBet);
	OnRep_Bet();
}

void ALadderMachine::ChangeBet(int32 Steps)
{
	if (!HasAuthority()) return;
	BetAmount += Steps * BetStep;
	ClampBet();
}

// ─────────── Nav (입력 진입점 — 서버 라우팅) ───────────

void ALadderMachine::SetRailCursor(int32 NewCursor)
{
	if (!HasAuthority()) { Server_SetRailCursor(NewCursor); return; }
	RailCursor = FMath::Clamp(NewCursor, 0, Rails - 1);
	OnRep_Cursor();
}
void ALadderMachine::Server_SetRailCursor_Implementation(int32 NewCursor) { SetRailCursor(NewCursor); }

void ALadderMachine::NavCycle()
{
	if (!HasAuthority()) { Server_NavCycle(); return; }
	if (bResultOpen) return;
	const uint8 Next = (static_cast<uint8>(Mode) + 1) % 4;
	Mode = static_cast<ELadderMode>(Next);
	OnRep_Mode();
}
void ALadderMachine::Server_NavCycle_Implementation() { NavCycle(); }

void ALadderMachine::NavLeft()
{
	if (!HasAuthority()) { Server_NavLeft(); return; }
	if (bResultOpen) return;
	switch (Mode)
	{
	case ELadderMode::RailSelect: SetRailCursor(RailCursor - 1); break;
	case ELadderMode::RailCount:  RemoveRail();                  break;
	case ELadderMode::Bet:        ChangeBet(-1);                 break;
	default: break;
	}
}
void ALadderMachine::Server_NavLeft_Implementation() { NavLeft(); }

void ALadderMachine::NavRight()
{
	if (!HasAuthority()) { Server_NavRight(); return; }
	if (bResultOpen) return;
	switch (Mode)
	{
	case ELadderMode::RailSelect: SetRailCursor(RailCursor + 1); break;
	case ELadderMode::RailCount:  AddRail();                     break;
	case ELadderMode::Bet:        ChangeBet(1);                  break;
	default: break;
	}
}
void ALadderMachine::Server_NavRight_Implementation() { NavRight(); }

void ALadderMachine::NavSelect()
{
	if (!HasAuthority())
	{
		// 클라 즉시 피드백(잔액부족). 권위 검증은 서버가 다시 함.
		// 클라는 로컬 Currency 를 못 읽으므로 복제된 DisplayBalance 로 예측 판단.
		if (!bIsPlaying && Mode == ELadderMode::RailSelect && DisplayBalance < BetAmount)
		{
			OnInsufficientFunds.Broadcast();
		}
		Server_NavSelect();
		return;
	}

	if (bResultOpen) return;

	switch (Mode)
	{
	case ELadderMode::RailSelect:
		if (!bIsPlaying)
		{
			if (GetBalance() >= BetAmount) { ServerStartPlay(RailCursor); }
			else                           { OnInsufficientFunds.Broadcast(); }
		}
		break;

	case ELadderMode::Refresh:
		ClearBoard();
		ShuffleResults();
		break;

	default: break;
	}
}
void ALadderMachine::Server_NavSelect_Implementation() { NavSelect(); }

void ALadderMachine::ResetRound()
{
	if (!HasAuthority()) { Server_ResetRound(); return; }

	const bool bWasResultOpen = bResultOpen;
	bResultOpen = false;

	ClearBoard();
	Mode = ELadderMode::RailSelect;
	OnRep_Mode();
	SetRailCursor(0);
	ClampBet();

	if (bWasResultOpen) { Multicast_ResultClosed(); }
}
void ALadderMachine::Server_ResetRound_Implementation() { ResetRound(); }

// ─────────── Play / Settle (authority) ───────────

void ALadderMachine::ServerStartPlay(int32 StartRail)
{
	if (!HasAuthority()) return;
	if (Rails < 2 || Rows < 2) return;
	if (bIsPlaying) return;

	// (1) 스테이크 차감 (서버 권위)
	StakedThisRound = FMath::Min(BetAmount, GetBalance());
	if (Acasino_simulatorCharacter* User = GetCurrentUser())
	{
		User->TrySpendCurrency(static_cast<float>(StakedThisRound));
	}
	RefreshDisplayBalance();   // 스테이크 차감 반영

	// (2) 결과 확정 (서버 RNG)
	CurrentPlan = ULadderGenerator::Generate(Rails, Rows, StartRail, Payouts, FakeRungChance);
	PendingWinnings = StakedThisRound * CurrentPlan.Payout;

	bIsPlaying   = true;
	bResultOpen  = false;

	// (3) 스크립트 배포 — 전 클라가 이 Plan 으로 트레이스 시작. 스테이크 반영된 잔액도 전달.
	Multicast_PlayStarted(CurrentPlan, GetBalance());

	// (4) 서버 타이머: 트레이스 길이 후 정산.
	GetWorldTimerManager().SetTimer(
		RevealTimerHandle, this, &ALadderMachine::ServerReveal,
		FMath::Max(TraceDuration, 0.01f), false);
}

void ALadderMachine::ServerReveal()
{
	if (!HasAuthority()) return;
	if (!bIsPlaying) return;

	// 구슬 도착 시점 = 당첨금 지급 (서버 권위)
	if (PendingWinnings > 0)
	{
		if (Acasino_simulatorCharacter* User = GetCurrentUser())
		{
			User->AddCurrency(static_cast<float>(PendingWinnings));
		}
	}
	RefreshDisplayBalance();   // 당첨금 반영

	bIsPlaying  = false;
	bResultOpen = true;

	// 결과 배포 — 당첨금 반영된 최종 잔액도 같이.
	Multicast_Result(CurrentPlan.bWin, CurrentPlan.DestRail, CurrentPlan.Payout, PendingWinnings, GetBalance());
}

// ─────────── Multicast 구현 (서버 + 전 클라에서 실행) ───────────

void ALadderMachine::Multicast_BoardCleared_Implementation()
{
	CurrentPlan = FLadderPlan();
	bIsPlaying  = false;
	OnBoardCleared.Broadcast();
}

void ALadderMachine::Multicast_PlayStarted_Implementation(FLadderPlan Plan, int32 BalanceAfterStake)
{
	CurrentPlan = Plan;
	bIsPlaying  = true;
	bResultOpen = false;
	OnBalanceChanged.Broadcast(BalanceAfterStake);   // 스테이크 반영 잔액 표시
	OnPlayStarted.Broadcast(Plan);                   // 위젯 트레이스 시작
}

void ALadderMachine::Multicast_Result_Implementation(bool bWin, int32 DestRail, int32 Multiplier, int32 Winnings, int32 NewBalance)
{
	bIsPlaying  = false;
	bResultOpen = true;
	OnBalanceChanged.Broadcast(NewBalance);          // 당첨금 반영 최종 잔액
	OnResult.Broadcast(bWin, DestRail, Multiplier, Winnings);
}

void ALadderMachine::Multicast_ResultClosed_Implementation()
{
	bResultOpen = false;
	OnResultClosed.Broadcast();
}

// ─────────── OnRep (config 동기화) ───────────

void ALadderMachine::OnRep_RailConfig()
{
	OnRailCountChanged.Broadcast(Rails);
	OnCursorChanged.Broadcast(RailCursor);
}

void ALadderMachine::OnRep_Payouts()
{
	OnResultsChanged.Broadcast();
}

void ALadderMachine::OnRep_Bet()
{
	OnBetChanged.Broadcast(BetAmount);
}

void ALadderMachine::OnRep_Mode()
{
	OnModeChanged.Broadcast(Mode);
}

void ALadderMachine::OnRep_Cursor()
{
	OnCursorChanged.Broadcast(RailCursor);
}

void ALadderMachine::OnRep_DisplayBalance()
{
	OnBalanceChanged.Broadcast(DisplayBalance);
}

// authority 전용: 서버의 실제 잔액을 복제 변수에 기록.
// 호스트는 자기 변경에 OnRep 이 안 오므로 수동 호출로 로컬 UI 갱신.
void ALadderMachine::RefreshDisplayBalance()
{
	if (!HasAuthority()) return;
	DisplayBalance = GetBalance();
	OnRep_DisplayBalance();
}

// ─────────── helpers ───────────

void ALadderMachine::ClearBoard()
{
	if (!HasAuthority()) return;
	CurrentPlan = FLadderPlan();
	bIsPlaying  = false;
	Multicast_BoardCleared();   // 서버 + 전 클라 판 비움 + OnBoardCleared
}
