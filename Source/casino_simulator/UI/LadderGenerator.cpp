// LadderGenerator.cpp

#include "LadderGenerator.h"

void ULadderGenerator::WalkStep(int32 Rails, int32& Cur, int32 Dest, int32& Moves, int32& Slack)
{
	if (Slack == 0)
	{
		// 여유분 0 → 최단루트(homing): 도착 쪽으로 한 칸
		if (Cur > Dest)      Cur -= 1;
		else if (Cur < Dest) Cur += 1;
	}
	else if (Cur == 0)          Cur += 1;   // 왼쪽 벽
	else if (Cur == Rails - 1)  Cur -= 1;   // 오른쪽 벽
	else                        Cur += (FMath::RandRange(0, 1) == 0) ? 1 : -1;

	Moves -= 1;
	Slack = (Moves - FMath::Abs(Dest - Cur)) / 2;
}

TArray<int32> ULadderGenerator::PickSortedRows(int32 Rows, int32 Count)
{
	TArray<int32> Pool;
	for (int32 i = 1; i < Rows; ++i)
	{
		Pool.Add(i);
	}

	// Fisher-Yates
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Pool.Swap(i, j);
	}

	Count = FMath::Clamp(Count, 0, Pool.Num());
	Pool.SetNum(Count);
	Pool.Sort();
	return Pool;
}

FLadderPlan ULadderGenerator::Generate(int32 Rails, int32 Rows, int32 StartRail,
	const TArray<int32>& Payouts, float FakeRungChance)
{
	FLadderPlan Plan;

	if (Rails < 2 || Rows < 2)
	{
		return Plan; // 빈 결과 (호출측에서 무시)
	}

	StartRail = FMath::Clamp(StartRail, 0, Rails - 1);
	Plan.StartRail = StartRail;

	// ── 도착 줄(결과) 먼저 결정 ──
	const int32 Dest = FMath::RandRange(0, Rails - 1);
	Plan.DestRail = Dest;
	Plan.Payout   = Payouts.IsValidIndex(Dest) ? Payouts[Dest] : 0; // 도착줄 배당
	Plan.bWin     = (Plan.Payout > 0);                              // 0이면 꽝

	const int32 MinMove = FMath::Abs(Dest - StartRail);

	// 크로싱 횟수: 행 수와 최소 이동횟수로 결정 (패리티 맞춤)
	int32 Crossings = Rows / 2;
	if ((Crossings % 2) != (MinMove % 2)) Crossings -= 1;

	int32 Slack = (Crossings - MinMove) / 2;

	// ── walk: 줄 시퀀스 ──
	TArray<int32> Path;
	Path.Add(StartRail);
	int32 Cur = StartRail;
	int32 MovesRemaining = Crossings;
	while (MovesRemaining > 0)
	{
		WalkStep(Rails, Cur, Dest, MovesRemaining, Slack);
		Path.Add(Cur);
	}

	// ── 크로싱 행(랜덤행) ──
	const TArray<int32> CrossRows = PickSortedRows(Rows, Crossings);

	// ── 구슬저장소 [행][줄] ──
	TArray<TArray<int32>> Ball;
	Ball.SetNum(Rows);
	for (TArray<int32>& Row : Ball) { Row.Init(0, Rails); }

	int32 idx = 0, ci = 0;
	for (int32 r = 0; r < Rows; ++r)
	{
		Ball[r][Path[idx]] = 1;                              // 들어온 줄
		if (ci < CrossRows.Num() && r == CrossRows[ci])      // 이 행이 크로싱?
		{
			idx++;
			Ball[r][Path[idx]] = 1;                          // 건너간 줄도 같은 행에
			ci++;
		}
	}

	// ── DrawRung 판별 ──
	const int32 Gaps = Rails - 1;
	Plan.DrawRung.SetNum(Rows);
	for (FLadderRungRow& Row : Plan.DrawRung) { Row.Cells.Init(false, Gaps); }

	// 1) 실제 가로줄: 칸 g 양옆 줄(g, g+1)이 둘 다 구슬
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 g = 0; g < Gaps; ++g)
		{
			const bool L = Ball[r][g] != 0;
			const bool R = Ball[r][g + 1] != 0;
			if (L && R)
			{
				Plan.DrawRung[r].Cells[g] = true; // 실제 경로 가로줄
			}
		}
	}

	// 2) 위장: "양옆 둘 다 구슬 아님" 후보 중, 같은 행 인접 칸에 이미 줄이 없으면 배치
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 g = 0; g < Gaps; ++g)
		{
			if (Plan.DrawRung[r].Cells[g]) continue;             // 이미 실제 줄
			if (Ball[r][g] != 0 || Ball[r][g + 1] != 0) continue; // 구슬 옆 금지
			const bool LeftAdj  = (g > 0)        && Plan.DrawRung[r].Cells[g - 1];
			const bool RightAdj = (g < Gaps - 1) && Plan.DrawRung[r].Cells[g + 1];
			if (LeftAdj || RightAdj) continue;                   // 같은 행 인접 금지
			if (FMath::FRand() >= FakeRungChance) continue;
			Plan.DrawRung[r].Cells[g] = true;                    // 위장 배치
		}
	}

	Plan.PathRails = Path;
	Plan.CrossRows = CrossRows;
	return Plan;
}
