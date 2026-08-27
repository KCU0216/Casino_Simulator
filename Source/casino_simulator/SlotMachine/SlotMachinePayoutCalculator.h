#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SlotMachineTypes.h"

struct FSlotMachinePayoutCalculator
{
	// 심볼 DataTable에서 특정 심볼의 기본 정보를 찾습니다.
	static bool FindSymbolData(
		UDataTable* SymbolDataTable,
		ESlotSymbol Symbol,
		FSlotSymbolData& OutSymbolData
	);

	// 보상 DataTable에서 심볼/족보/라인/매치 개수에 맞는 보상 규칙을 찾습니다.
	static bool FindPayoutData(
		UDataTable* PayoutDataTable,
		ESlotResultType ResultType,
		ESlotLineType LineType,
		int32 MatchCount,
		FSlotPayoutData& OutPayoutData
	);

	// 한 줄 당첨 결과에 배율과 보상 금액을 채웁니다.
	static bool ApplyPayoutToLineWin(
		UDataTable* SymbolDataTable,
		UDataTable* PayoutDataTable,
		int32 BetAmount,
		FSlotLineWin& LineWin
	);

	// 전체 SpinResult에 모든 라인 보상과 총 보상 금액을 채웁니다.
	static FSlotSpinResult ApplyPayoutToSpinResult(
		const FSlotSpinResult& SpinResult,
		UDataTable* SymbolDataTable,
		UDataTable* PayoutDataTable,
		int32 BetAmount
	);
};