#pragma once

#include "CoreMinimal.h"
#include "SlotMachineTypes.h"

struct FSlotMachineEvaluator
{
	// 15칸 전체가 같은 심볼인지 검사합니다.
	static bool IsFullScreenMatch(const TArray<ESlotSymbol>& ResultSymbols, ESlotSymbol& OutSymbol);

	// 매치 개수를 족보 타입으로 변환합니다.
	static ESlotResultType GetResultTypeByMatchCount(int32 MatchCount);

	// 라인 종류에 따라 해당 매치 개수가 보상 가능한지 검사합니다.
	static bool IsLineEligibleForMatchCount(ESlotLineType LineType, int32 MatchCount);

	// 라인 하나를 왼쪽부터 검사합니다.
	static bool EvaluateSingleLine(
		const TArray<ESlotSymbol>& ResultSymbols,
		int32 LineIndex,
		FSlotLineWin& OutLineWin
	);

	// 5x3 결과 배열 전체를 판정합니다.
	static FSlotSpinResult EvaluateSlotResult(const TArray<ESlotSymbol>& ResultSymbols);
};