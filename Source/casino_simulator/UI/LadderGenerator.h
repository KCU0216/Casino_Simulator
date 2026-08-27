// LadderGenerator.h
//
// 사다리 "생성 알고리즘"만 떼어낸 순수 로직.
//  - UI/그리기/돈/BP이벤트 전혀 없음. (Rails, Rows, StartRail, Payouts) → 결과 구조체.
//  - LadderWidget::GenerateLadder 의 계산 부분을 그대로 옮긴 것.
//  - 위젯은 이걸 호출해서 나온 FLadderPlan 을 멤버에 저장하고 NativePaint 로 그리기만 하면 됨.
//  - 순수 함수라 자동화 테스트(도착 줄 균등성, 위장 줄이 실제 경로를 안 건드리는지 등)에 쓰기 좋음.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LadderGenerator.generated.h"

// 한 행의 가로줄들. Cells[g]==true 면 세로줄 g 와 g+1 사이에 가로줄을 그린다.
// (UPROPERTY 가 TArray<TArray<bool>> 를 못 담아서 행을 구조체로 감쌈. 기존 DrawRung[r][g] 와 1:1.)
USTRUCT(BlueprintType)
struct FLadderRungRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	TArray<bool> Cells;
};

// 사다리 한 판의 확정된 결과 + 그리기용 데이터. GenerateLadder 가 만들던 값들을 한 곳에 모음.
USTRUCT(BlueprintType)
struct FLadderPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 StartRail = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 DestRail = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	bool bWin = false;

	// 도착 줄의 배당(배율). 0 = 꽝.
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	int32 Payout = 0;

	// 실제 경로: 방문한 줄 시퀀스. (트레이스 재구성용)
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	TArray<int32> PathRails;

	// 크로싱이 일어난 행들(오름차순).
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	TArray<int32> CrossRows;

	// 그릴 가로줄(실제 + 위장 통합). 길이 = Rows, 각 행 Cells 길이 = Rails-1.
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	TArray<FLadderRungRow> DrawRung;
};

UCLASS()
class CASINO_SIMULATOR_API ULadderGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 사다리 한 판 생성. 도착 줄을 먼저 뽑고 → 거기 도달하는 경로를 만들고 → 위장 줄을 덧붙인다.
	// Payouts: 각 도착 줄의 배당(길이 = Rails 권장). FakeRungChance: 위장 줄 배치 확률(기존 0.4).
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	static FLadderPlan Generate(int32 Rails, int32 Rows, int32 StartRail,
		const TArray<int32>& Payouts, float FakeRungChance = 0.4f);

private:
	// walk 한 스텝(기존 WalkStep 과 동일 규칙).
	static void WalkStep(int32 Rails, int32& Cur, int32 Dest, int32& Moves, int32& Slack);

	// [1, Rows-1] 에서 Count 개를 중복 없이 뽑아 오름차순 반환 (Fisher-Yates, 기존 PickSortedRows 동일).
	static TArray<int32> PickSortedRows(int32 Rows, int32 Count);
};
