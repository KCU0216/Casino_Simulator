// RaceManager.cpp
#include "RaceManager.h"
#include "RaceRunner.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "RaceBillboardWidget.h"

static const TCHAR* KRNames[] = {
	TEXT("김춘수"), TEXT("박막례"), TEXT("이순자"), TEXT("최봉팔"),
	TEXT("정갑수"), TEXT("오만득"), TEXT("한상철"), TEXT("서말자"),
	TEXT("류판석"), TEXT("강달수"), TEXT("문영자"), TEXT("고만수")
};

// 레시피로 완주 시각을 결정론적으로 계산 (고정 스텝). 승자 판정용.
static float SimulateFinishTime(const FRunnerRaceScript& S)
{
	const float Dt = 1.f / 120.f;
	float T = 0.f, Pos = 0.f, StumbleUntil = 0.f;
	bool bAwake = false, bStumbled = false;

	for (int32 i = 0; i < 200000 && Pos < S.TrackLength; ++i)
	{
		if (S.bWillAwaken && !bAwake && Pos >= S.AwakenAtPos) { bAwake = true; }
		if (!bAwake && S.bWillStumble && !bStumbled && Pos >= S.StumbleAtPos) { bStumbled = true; StumbleUntil = T + 0.7f; }

		float Mult = bAwake ? 2.3f : 1.f;
		if (T < StumbleUntil) Mult *= 0.3f;

		Pos += S.Speed * Mult * Dt;
		T += Dt;
	}
	return T;
}

ARaceManager::ARaceManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 빈 씬을 루트로
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Track을 루트에 부착
	Track = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Track"));
	Track->SetupAttachment(SceneRoot);

	// NPC 스폰 포인트는 루트에 부착 → Track 스케일 영향 안 받음
	NPCSpawnPoints = CreateDefaultSubobject<USceneComponent>(TEXT("NPCSpawnPoints"));
    NPCSpawnPoints->SetupAttachment(SceneRoot);

    BillboardWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("BillboardWidget"));
    BillboardWidget->SetupAttachment(SceneRoot);
    BillboardWidget->SetWidgetSpace(EWidgetSpace::World);
    BillboardWidget->SetDrawSize(FVector2D(1920.f, 1080.f));
    BillboardWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

void ARaceManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaceManager, Phase);
	DOREPLIFETIME(ARaceManager, WinnerIndex);
	DOREPLIFETIME(ARaceManager, FinishOrder);
	DOREPLIFETIME(ARaceManager, Tickets);
	DOREPLIFETIME(ARaceManager, CurrentRoundNumber);
	DOREPLIFETIME(ARaceManager, Runners);
    DOREPLIFETIME(ARaceManager, BettingStartServerTime);
    DOREPLIFETIME(ARaceManager, BettingDuration);
}

void ARaceManager::BeginPlay()
{
	Super::BeginPlay();
    OnRep_Phase();
	// NPC는 서버에서만 스폰 (복제 액터 → 클라엔 자동으로 복제됨). 클라가 또 스폰하면 2개가 됨.
	if (HasAuthority())
	{
		if (NPCClass && NPCSpawnPoints)
		{
			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			FTransform SpawnTM = NPCSpawnPoints->GetComponentTransform();
			SpawnTM.SetScale3D(FVector::OneVector);   // 스케일 항상 1

			RaceNPC = GetWorld()->SpawnActor<ANPC_InteractionCameraBase>(NPCClass, SpawnTM, Params);
			if (!RaceNPC)
				UE_LOG(LogTemp, Warning, TEXT("[RaceManager] NPC 스폰 실패 - 클래스/위치 확인"));
		}

		// 첫 경주는 외부 입력으로 시작한다. BeginPlay에서는 Idle 상태를 유지한다.
	}
}


FRaceRunnerStats ARaceManager::RollStats(int32 LaneIndex) const
{
	static const int32 Lo[] = { 55, 65, 75, 85, 95, 105 };
	static const int32 Hi[] = { 64, 74, 84, 94, 104, 114 };
	const int32 BucketCount = UE_ARRAY_COUNT(Lo);
	const int32 Bucket = (LaneIndex < BucketCount) ? LaneIndex : FMath::RandRange(0, BucketCount - 1);

	FRaceRunnerStats S;
	S.Age           = FMath::RandRange(Lo[Bucket], Hi[Bucket]);
	S.BaseSpeed     = 225.f - (S.Age - 60) * 2.6f;
	S.AwakenChance  = FMath::Max(0.f, (S.Age - 68) / 27.f) * 0.32f;
	S.StumbleChance = FMath::Max(0.f, (S.Age - 68) / 27.f) * 0.30f;

	// Ages below 60 use the minimum odds instead of a fractional power of a negative number.
	const float Raw = 1.8f + FMath::Pow(FMath::Max(0.f, (S.Age - 60) / 35.f), 1.4f) * 7.f;
	S.Odds = FMath::RoundToFloat(Raw * 10.f) / 10.f;
	return S;
}

FRunnerRaceScript ARaceManager::RollScript(const FRaceRunnerStats& S, const FVector& StartLoc, const FVector& Dir) const
{
	FRunnerRaceScript R;
	R.SpawnLoc     = StartLoc;
	R.Dir          = Dir.GetSafeNormal();
	R.StartLoc     = R.SpawnLoc + R.Dir * EnterDistance;
	R.FinishLoc    = R.StartLoc + R.Dir * TrackLength;
	R.ExitLoc      = R.FinishLoc + R.Dir * ExitDistance;
	R.EnterStartServerTime = EnterStartServerTime;
	R.EnterDuration = EnterDuration;
	R.ExitDuration = ExitDuration;
	R.TrackLength  = TrackLength;
	R.Speed        = S.BaseSpeed * FMath::FRandRange(0.85f, 1.15f);   // 운 반영, 레이스 내내 고정
	R.bWillAwaken  = FMath::FRand() < S.AwakenChance;
	R.AwakenAtPos  = TrackLength * FMath::FRandRange(0.35f, 0.55f);
	R.bWillStumble = FMath::FRand() < S.StumbleChance;
	R.StumbleAtPos = TrackLength * FMath::FRandRange(0.25f, 0.70f);
	return R;
}

void ARaceManager::StartNewRound()
{
	if (!HasAuthority() || Phase != ERacePhase::Idle) return;
	if (RunnerSpawnPoints.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[RaceManager] RunnerSpawnPoints must be assigned."));
		return;
	}
	if (RunnerSpawnPoints.Num() > UE_ARRAY_COUNT(KRNames))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RaceManager] Not enough unique names for RunnerSpawnPoints."));
		return;
	}
	
	for (ARaceRunner* R : Runners) { if (R) R->Destroy(); }
	Runners.Reset();
	WinnerIndex = -1;
	FinishOrder.Reset();
	RaceElapsed = 0.f;
	RaceDuration = 0.f;
	bResultBroadcast = false;

	if (!RunnerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RaceManager] RunnerClass 미지정 - 러너 BP를 지정해줘"));
		return;
	}
	CurrentRoundNumber++;
	EnterStartServerTime = GetServerTime();
	EnterDuration = FMath::Max(0.f, EnterDistance) / FMath::Max(1.f, TransitSpeed);
	ExitDuration = FMath::Max(0.f, ExitDistance) / FMath::Max(1.f, TransitSpeed);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 매 라운드 이름 목록을 섞고, 스폰된 러너에게 순서대로 배정한다.
	TArray<const TCHAR*> ShuffledNames;
	for (const TCHAR* Name : KRNames)
	{
		ShuffledNames.Add(Name);
	}
	for (int32 i = ShuffledNames.Num() - 1; i > 0; --i)
	{
		ShuffledNames.Swap(i, FMath::RandRange(0, i));
	}

	// 나이대별 스탯과 이름을 먼저 정한 뒤, 묶음 전체를 섞어 레일에 배정한다.
	TArray<FRaceRunnerStats> ShuffledStats;
	for (int32 i = 0; i < RunnerSpawnPoints.Num(); ++i)
	{
		FRaceRunnerStats Stats = RollStats(i);
		Stats.Name = ShuffledNames[i];
		ShuffledStats.Add(Stats);
	}
	for (int32 i = ShuffledStats.Num() - 1; i > 0; --i)
	{
		ShuffledStats.Swap(i, FMath::RandRange(0, i));
	}

	for (int32 i = 0; i < RunnerSpawnPoints.Num(); ++i)
	{
		if (!IsValid(RunnerSpawnPoints[i])) continue;
		const FVector Loc = RunnerSpawnPoints[i]->GetActorLocation();
		const FRotator Rot = RunnerSpawnPoints[i]->GetActorRotation();

		ARaceRunner* R = GetWorld()->SpawnActor<ARaceRunner>(RunnerClass, Loc, Rot, SP);
		if (!R) continue;

		R->InitStats(ShuffledStats[i]);
		R->ServerSetupScript(RollScript(R->Stats, Loc, Rot.Vector()));
		Runners.Add(R);
	}

	if (Runners.IsEmpty()) return;
	Entering();
}

void ARaceManager::Entering()
{
	if (!HasAuthority() || Phase != ERacePhase::Idle || Runners.IsEmpty()) return;
	for (ARaceRunner* Runner : Runners)
	{
		if (IsValid(Runner)) Runner->ServerSetEntering(true);
	}
	Phase = ERacePhase::Entering;
	OnRep_Phase();
	ForceNetUpdate();
}

void ARaceManager::StartRace()
{
	if (!HasAuthority() || Phase != ERacePhase::Betting) return;

	float MaxTime = 0.f;
	TArray<TPair<int32, float>> Times;   // {러너 인덱스, 완주 시각}

	for (int32 i = 0; i < Runners.Num(); ++i)
	{
		ARaceRunner* Rn = Runners[i];
		if (!Rn) continue;

		const FRunnerRaceScript& Script = Rn->RaceScript;

		// 완주 시각 결정론 계산 → 순위 판정용
		const float FinishT = SimulateFinishTime(Script);
		Times.Add({ i, FinishT });
		MaxTime = FMath::Max(MaxTime, FinishT);
	}

	// 완주 시각 오름차순 = 완주 순위
	Times.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) { return A.Value < B.Value; });
	FinishOrder.Reset();
	for (const TPair<int32, float>& T : Times) FinishOrder.Add(T.Key);

	WinnerIndex = FinishOrder.Num() > 0 ? FinishOrder[0] : -1;   // 1등 (복제됨) — 결승 전에 이미 정해짐
	RaceDuration = MaxTime + 0.3f;      // 전원 완주 시간 + 버퍼
	RaceElapsed = 0.f;
	bResultBroadcast = false;

	RaceStartServerTime = GetServerTime();
	for (ARaceRunner* Rn : Runners)
	{
		if (IsValid(Rn)) Rn->ServerSetRacing(true, RaceStartServerTime);
	}

	Phase = ERacePhase::Racing;
	OnRep_Phase();
	OnRaceStarted.Broadcast();
}

void ARaceManager::Tick(float Dt)
{
	Super::Tick(Dt);
    RefreshBillboard(); // Also initializes a widget created or replaced after BeginPlay.
	if (!HasAuthority()) return;
	if (Phase == ERacePhase::Entering)
	{
		if (GetServerTime() - EnterStartServerTime >= EnterDuration) BeginBetting();
		return;
	}
	if (Phase == ERacePhase::Betting)
	{
		if (GetServerTime() - BettingStartServerTime >= BettingDuration) StartRace();
		return;
	}
	if (Phase == ERacePhase::Exiting)
	{
		if (GetServerTime() - ExitStartServerTime >= ExitDuration) FinishExiting();
		return;
	}
	if (Phase != ERacePhase::Racing) return;

	RaceElapsed = static_cast<float>(GetServerTime() - RaceStartServerTime);
	if (!bResultBroadcast && RaceElapsed >= RaceDuration)
	{
		bResultBroadcast = true;
		SettleTickets();   // 단승: 진 마권 자동삭제, 당첨 마권 유지(환전 대기)
		Exiting();
	}
}

double ARaceManager::GetServerTime() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void ARaceManager::BeginBetting()
{
	for (ARaceRunner* Runner : Runners)
	{
		if (IsValid(Runner)) Runner->ServerSetEntering(false);
	}
	BettingStartServerTime = GetServerTime();
	Phase = ERacePhase::Betting;
	OnRep_Phase();
	ForceNetUpdate();
	OnLineupReady.Broadcast();
}

void ARaceManager::Exiting()
{
	if (!HasAuthority() || Phase != ERacePhase::Racing || !bResultBroadcast) return;
	ExitStartServerTime = GetServerTime();
	for (ARaceRunner* Runner : Runners)
	{
		if (!IsValid(Runner)) continue;
		Runner->ServerSetRacing(false);
		Runner->ServerSetExiting(true, ExitStartServerTime);
	}
	Phase = ERacePhase::Exiting;
	OnRep_Phase();
	ForceNetUpdate();
	OnRaceFinished.Broadcast(GetWinner(), WinnerIndex);
	OnLineupExit.Broadcast();
}

void ARaceManager::FinishExiting()
{
	for (ARaceRunner* Runner : Runners)
	{
		if (!IsValid(Runner)) continue;
		Runner->ServerSetExiting(false);
		Runner->Destroy();
	}
	Runners.Reset();
	Phase = ERacePhase::Finished;
	OnRep_Phase();
	ForceNetUpdate();
}

void ARaceManager::ResetRace()
{
	if (!HasAuthority() || Phase != ERacePhase::Finished) return;

	for (ARaceRunner* R : Runners)
	{
		if (R) R->Destroy();
	}
	Runners.Reset();

	Phase = ERacePhase::Idle;
	WinnerIndex = -1;
	FinishOrder.Reset();
	RaceElapsed = 0.f;
	RaceDuration = 0.f;
	bResultBroadcast = false;
	OnRep_Phase();
}

ARaceRunner* ARaceManager::GetWinner() const
{
	return Runners.IsValidIndex(WinnerIndex) ? Runners[WinnerIndex] : nullptr;
}

void ARaceManager::OnRep_Phase()
{
    RefreshBillboard();
    if (GetNetMode() == NM_DedicatedServer) return;
    if (!bBillboardPhaseInitialized || LastBillboardPhase != Phase)
    {
        bBillboardPhaseInitialized = true;
        LastBillboardPhase = Phase;
        OnPhaseChanged.Broadcast(Phase);
        OnBillboardPhaseChanged(Phase);
    }
}

void ARaceManager::RefreshBillboard()
{
    if (GetNetMode() == NM_DedicatedServer || !BillboardWidget) return;
    BillboardWidget->InitWidget();
    if (auto* Widget = GetBillboardWidget()) Widget->RefreshFromManager(this);
}

void ARaceManager::OnRep_BillboardData()
{
    RefreshBillboard();
    if (auto* Widget = GetBillboardWidget()) Widget->OnRaceDataUpdated();
}

URaceBillboardWidget* ARaceManager::GetBillboardWidget() const
{
    return BillboardWidget ? Cast<URaceBillboardWidget>(BillboardWidget->GetUserWidgetObject()) : nullptr;
}

ARaceRunner* ARaceManager::GetLeadingRunner() const
{
    if (Phase != ERacePhase::Racing) return nullptr;
    ARaceRunner* Leader = nullptr;
    float BestDistance = -1.f;
    for (ARaceRunner* Runner : Runners)
    {
        if (!IsValid(Runner)) continue;
        const float Distance = Runner->GetPosUnits();
        if (Distance > BestDistance)
        {
            BestDistance = Distance;
            Leader = Runner;
        }
    }
    return Leader;
}

float ARaceManager::GetRemainingBettingSeconds() const
{
    return Phase == ERacePhase::Betting
        ? static_cast<float>(FMath::Max(0.0, BettingStartServerTime + BettingDuration - GetServerTime())) : 0.f;
}

// ───────────────────────── 마권 ─────────────────────────

bool ARaceManager::ServerBuyTicket(Acasino_simulatorCharacter* Player, int32 RunnerIndex, int32 Amount, int32 Count)
{
	if (!HasAuthority() || !Player) return false;
	if (Phase != ERacePhase::Betting) return false;                 // 배팅 페이즈에만 구매
	if (!Runners.IsValidIndex(RunnerIndex) || !Runners[RunnerIndex]) return false;
	if (Amount <= 0 || Count <= 0) return false;

	const int32 Total = Amount * Count;
	if (!Player->TrySpendCurrency(static_cast<float>(Total))) return false;   // 잔액 부족 → 실패

	APlayerState* PS = Player->GetPlayerState();

	FBetTicket T;
	T.TicketId    = NextTicketId++;
	T.Buyer       = PS;
	T.BuyerName   = PS ? PS->GetPlayerName() : TEXT("?");
	T.RoundNumber = CurrentRoundNumber;
	T.RunnerIndex = RunnerIndex;
	T.RunnerName  = Runners[RunnerIndex]->Stats.Name;
	T.RunnerAge   = Runners[RunnerIndex]->Stats.Age;
	T.RunnerPortrait = Runners[RunnerIndex]->GetRunnerPortrait();
	T.Amount      = Amount;
	T.Count       = Count;
	T.Odds        = Runners[RunnerIndex]->Stats.Odds;               // 구매 시점 배당 고정
	Tickets.Add(T);
	OnRep_Tickets();
	ForceNetUpdate();
	return true;
}

void ARaceManager::SettleTickets()
{
	if (!HasAuthority()) return;
	bool bTicketsChanged = false;
	// 단승: WinnerIndex 맞춘 마권만 당첨 → 유지, 나머진 삭제.
	// 이미 bWon인 건 지난 라운드 당첨분(환전 대기) → 건드리지 않음.
	for (int32 i = Tickets.Num() - 1; i >= 0; --i)
	{
		if (Tickets[i].bWon) continue;
		bTicketsChanged = true;
		if (Tickets[i].RunnerIndex == WinnerIndex)
			Tickets[i].bWon = true;
		else
			Tickets.RemoveAt(i);
	}
	if (bTicketsChanged)
	{
		OnRep_Tickets();
		ForceNetUpdate();
	}
}

int32 ARaceManager::ServerClaimWinnings(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority() || !Player) return 0;
	APlayerState* PS = Player->GetPlayerState();
	if (!PS) return 0;

	int32 TotalPaid = 0;
	for (int32 i = Tickets.Num() - 1; i >= 0; --i)
	{
		if (Tickets[i].bWon && Tickets[i].Buyer == PS)
		{
			const int32 Pay = Tickets[i].Payout();
			Player->AddCurrency(static_cast<float>(Pay));
			TotalPaid += Pay;
			Tickets.RemoveAt(i);                    // 환전 완료 → 제거
		}
	}
	if (TotalPaid > 0)
	{
		OnRep_Tickets();
		ForceNetUpdate();
	}
	return TotalPaid;
}

void ARaceManager::OnRep_Tickets()
{
	OnTicketsChanged.Broadcast();
}

TArray<FBetTicket> ARaceManager::GetTicketsForPlayer(APlayerState* PS) const
{
	TArray<FBetTicket> Out;
	if (!PS) return Out;
	for (const FBetTicket& T : Tickets)
		if (T.Buyer == PS) Out.Add(T);
	return Out;
}

float ARaceManager::GetOdds(int32 RunnerIndex) const
{
	return (Runners.IsValidIndex(RunnerIndex) && Runners[RunnerIndex]) ? Runners[RunnerIndex]->Stats.Odds : 0.f;
}
