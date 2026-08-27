// RaceManager.cpp
#include "RaceManager.h"
#include "RaceRunner.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "casino_simulatorCharacter.h"
#include "GameFramework/PlayerState.h"

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

}

void ARaceManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaceManager, Phase);
	DOREPLIFETIME(ARaceManager, WinnerIndex);
	DOREPLIFETIME(ARaceManager, FinishOrder);
	DOREPLIFETIME(ARaceManager, Tickets);
}

void ARaceManager::BeginPlay()
{
	Super::BeginPlay();
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

		StartNewRound();
	}
}

FRaceRunnerStats ARaceManager::RollStats(int32 LaneIndex) const
{
	static const int32 Lo[4] = { 65, 75, 85, 95 };
	static const int32 Hi[4] = { 74, 84, 94, 104 };
	const int32 Bucket = (LaneIndex < 4) ? LaneIndex : FMath::RandRange(0, 3);

	FRaceRunnerStats S;
	S.Age           = FMath::RandRange(Lo[Bucket], Hi[Bucket]);
	S.Name          = KRNames[FMath::RandRange(0, UE_ARRAY_COUNT(KRNames) - 1)];
	S.BaseSpeed     = 225.f - (S.Age - 60) * 2.6f;
	S.AwakenChance  = FMath::Max(0.f, (S.Age - 68) / 27.f) * 0.32f;
	S.StumbleChance = FMath::Max(0.f, (S.Age - 68) / 27.f) * 0.30f;

	const float Raw = 1.8f + FMath::Pow((S.Age - 60) / 35.f, 1.4f) * 7.f;
	S.Odds = FMath::RoundToFloat(Raw * 10.f) / 10.f;
	return S;
}

FRunnerRaceScript ARaceManager::RollScript(const FRaceRunnerStats& S, const FVector& StartLoc, const FVector& Dir) const
{
	FRunnerRaceScript R;
	R.StartLoc     = StartLoc;
	R.Dir          = Dir.GetSafeNormal();
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
	if (!HasAuthority()) return;

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

	const bool bUseSpawnPoints = RunnerSpawnPoints.Num() > 0;
	const int32 Count = bUseSpawnPoints ? RunnerSpawnPoints.Num() : NumRunners;

	const FVector DirN = RaceDirection.GetSafeNormal();
	const FVector Side = FVector::CrossProduct(DirN, FVector::UpVector).GetSafeNormal();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < Count; ++i)
	{
		FVector  Loc;
		FRotator Rot;
		if (bUseSpawnPoints)
		{
			if (!RunnerSpawnPoints[i]) continue;                    // 순서대로: i번 지점 = i번 러너
			Loc = RunnerSpawnPoints[i]->GetActorLocation();
			Rot = RunnerSpawnPoints[i]->GetActorRotation();
		}
		else
		{
			Loc = StartLocation + Side * (LaneSpacing * i);   // 폴백: 하드코딩 계산
			Rot = DirN.Rotation();
		}

		ARaceRunner* R = GetWorld()->SpawnActor<ARaceRunner>(RunnerClass, Loc, Rot, SP);
		if (!R) continue;

		R->InitStats(RollStats(i));
		R->ServerSetRacing(false);
		Runners.Add(R);
	}

	Phase = ERacePhase::Betting;
	OnRep_Phase();
	OnLineupReady.Broadcast();
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

		// 이번 판 레시피 롤 (랜덤은 여기서 한 번에 다 굴림). 방향 = 러너가 바라보는 쪽(스폰지점 방향).
		const FRunnerRaceScript Script = RollScript(Rn->Stats, Rn->GetActorLocation(), Rn->GetActorForwardVector());
		Rn->ServerSetupScript(Script);

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

	for (ARaceRunner* Rn : Runners) { if (Rn) Rn->ServerSetRacing(true); }   // 출발 신호(복제)

	Phase = ERacePhase::Racing;
	OnRep_Phase();
	OnRaceStarted.Broadcast();
}

void ARaceManager::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!HasAuthority() || Phase != ERacePhase::Racing) return;

	RaceElapsed += Dt;
	if (!bResultBroadcast && RaceElapsed >= RaceDuration)
	{
		bResultBroadcast = true;
		Phase = ERacePhase::Finished;
		OnRep_Phase();
		OnRaceFinished.Broadcast(GetWinner(), WinnerIndex);

		SettleTickets();   // 단승: 진 마권 자동삭제, 당첨 마권 유지(환전 대기)
	}
}

void ARaceManager::ResetRace()
{
	if (!HasAuthority()) return;
	Phase = ERacePhase::Idle;
	WinnerIndex = -1;
	FinishOrder.Reset();
	bResultBroadcast = false;
	for (ARaceRunner* R : Runners) { if (R) R->ServerSetRacing(false); }
	OnRep_Phase();
}

ARaceRunner* ARaceManager::GetWinner() const
{
	return Runners.IsValidIndex(WinnerIndex) ? Runners[WinnerIndex] : nullptr;
}

void ARaceManager::OnRep_Phase()
{
	// 클라: 페이즈 바뀔 때 UI 전환 등 (BP에서 OnRep 후크로 처리)
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
	T.RunnerIndex = RunnerIndex;
	T.RunnerName  = Runners[RunnerIndex]->Stats.Name;
	T.Amount      = Amount;
	T.Count       = Count;
	T.Odds        = Runners[RunnerIndex]->Stats.Odds;               // 구매 시점 배당 고정
	Tickets.Add(T);
	return true;
}

void ARaceManager::SettleTickets()
{
	if (!HasAuthority()) return;
	// 단승: WinnerIndex 맞춘 마권만 당첨 → 유지, 나머진 삭제.
	// 이미 bWon인 건 지난 라운드 당첨분(환전 대기) → 건드리지 않음.
	for (int32 i = Tickets.Num() - 1; i >= 0; --i)
	{
		if (Tickets[i].bWon) continue;
		if (Tickets[i].RunnerIndex == WinnerIndex)
			Tickets[i].bWon = true;
		else
			Tickets.RemoveAt(i);
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
	return TotalPaid;
}

TArray<FBetTicket> ARaceManager::GetTicketsForPlayer(APlayerState* PS) const
{
	TArray<FBetTicket> Out;
	for (const FBetTicket& T : Tickets)
		if (T.Buyer == PS) Out.Add(T);
	return Out;
}

float ARaceManager::GetOdds(int32 RunnerIndex) const
{
	return (Runners.IsValidIndex(RunnerIndex) && Runners[RunnerIndex]) ? Runners[RunnerIndex]->Stats.Odds : 0.f;
}
