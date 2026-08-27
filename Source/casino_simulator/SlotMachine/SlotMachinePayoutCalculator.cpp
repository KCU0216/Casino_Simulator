#include "SlotMachine/SlotMachinePayoutCalculator.h"

bool FSlotMachinePayoutCalculator::FindSymbolData(
	UDataTable* SymbolDataTable,
	ESlotSymbol Symbol,
	FSlotSymbolData& OutSymbolData
)
{
	OutSymbolData = FSlotSymbolData();

	if (!SymbolDataTable || Symbol == ESlotSymbol::None)
	{
		return false;
	}

	TArray<FSlotSymbolData*> Rows;
	SymbolDataTable->GetAllRows<FSlotSymbolData>(TEXT("FindSymbolData"), Rows);

	for (const FSlotSymbolData* Row : Rows)
	{
		if (Row && Row->Symbol == Symbol)
		{
			OutSymbolData = *Row;
			return true;
		}
	}

	return false;
}

bool FSlotMachinePayoutCalculator::FindPayoutData(
	UDataTable* PayoutDataTable,
	ESlotResultType ResultType,
	ESlotLineType LineType,
	int32 MatchCount,
	FSlotPayoutData& OutPayoutData
)
{
	OutPayoutData = FSlotPayoutData();

	if (!PayoutDataTable)
	{
		return false;
	}

	TArray<FSlotPayoutData*> Rows;
	PayoutDataTable->GetAllRows<FSlotPayoutData>(TEXT("FindPayoutData"), Rows);

	for (const FSlotPayoutData* Row : Rows)
	{
		if (!Row)
		{
			continue;
		}

		const bool bIsMatch =
			Row->ResultType == ResultType &&
			Row->LineType == LineType &&
			Row->MatchCount == MatchCount;

		if (bIsMatch)
		{
			OutPayoutData = *Row;
			return true;
		}
	}

	return false;
}

bool FSlotMachinePayoutCalculator::ApplyPayoutToLineWin(
	UDataTable* SymbolDataTable,
	UDataTable* PayoutDataTable,
	int32 BetAmount,
	FSlotLineWin& LineWin
)
{
	FSlotSymbolData SymbolData;
	if (!FindSymbolData(SymbolDataTable, LineWin.WinningSymbol, SymbolData))
	{
		return false;
	}

	FSlotPayoutData PayoutData;
	if (!FindPayoutData(
		PayoutDataTable,
		LineWin.ResultType,
		LineWin.LineType,
		LineWin.MatchCount,
		PayoutData
	))
	{
		return false;
	}

	LineWin.RewardType = PayoutData.RewardType;
	LineWin.SymbolMultiplier = SymbolData.SymbolMultiplier;
	LineWin.ResultMultiplier = PayoutData.ResultMultiplier;
	LineWin.DisplayText = PayoutData.DisplayText;
	LineWin.RewardAmount = FMath::RoundToInt(BetAmount * LineWin.SymbolMultiplier * LineWin.ResultMultiplier);

	return true;
}

FSlotSpinResult FSlotMachinePayoutCalculator::ApplyPayoutToSpinResult(
	const FSlotSpinResult& SpinResult,
	UDataTable* SymbolDataTable,
	UDataTable* PayoutDataTable,
	int32 BetAmount
)
{
	FSlotSpinResult ResultWithPayout = SpinResult;
	ResultWithPayout.TotalRewardAmount = 0;

	if (!ResultWithPayout.bIsWin)
	{
		return ResultWithPayout;
	}

	for (FSlotLineWin& LineWin : ResultWithPayout.LineWins)
	{
		if (ApplyPayoutToLineWin(SymbolDataTable, PayoutDataTable, BetAmount, LineWin))
		{
			ResultWithPayout.TotalRewardAmount += LineWin.RewardAmount;
		}
	}

	return ResultWithPayout;
}