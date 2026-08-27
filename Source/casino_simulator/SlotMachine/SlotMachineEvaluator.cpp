#include "SlotMachine/SlotMachineEvaluator.h"
#include "SlotMachine/SlotMachinePaylines.h"

// 15칸 전체가 같은 심볼인지 검사합니다.
bool FSlotMachineEvaluator::IsFullScreenMatch(const TArray<ESlotSymbol>& ResultSymbols, ESlotSymbol& OutSymbol)
{
	OutSymbol = ESlotSymbol::None;

	if (ResultSymbols.Num() != 15)
	{
		return false;
	}

	const ESlotSymbol FirstSymbol = ResultSymbols[0];

	if (FirstSymbol == ESlotSymbol::None)
	{
		return false;
	}

	for (int32 Index = 1; Index < ResultSymbols.Num(); ++Index)
	{
		if (ResultSymbols[Index] != FirstSymbol)
		{
			return false;
		}
	}

	OutSymbol = FirstSymbol;
	return true;
}

// 매치 개수를 족보 타입으로 변환합니다.
ESlotResultType FSlotMachineEvaluator::GetResultTypeByMatchCount(int32 MatchCount)
{
	switch (MatchCount)
	{
	case 3:
		return ESlotResultType::ThreeOfAKind;

	case 4:
		return ESlotResultType::FourOfAKind;

	case 5:
		return ESlotResultType::FiveOfAKind;

	default:
		return ESlotResultType::Lose;
	}
}

// 라인 종류에 따라 해당 매치 개수가 보상 가능한지 검사합니다.
bool FSlotMachineEvaluator::IsLineEligibleForMatchCount(ESlotLineType LineType, int32 MatchCount)
{
	switch (LineType)
	{
	case ESlotLineType::Horizontal:
		return MatchCount >= 3 && MatchCount <= 5;

	case ESlotLineType::VShape:
	case ESlotLineType::InvertedVShape:
		return MatchCount == 5;

	case ESlotLineType::Vertical:
	case ESlotLineType::Diagonal:
		return MatchCount == 3;

	default:
		return false;
	}
}

// 라인 하나를 검사하고 당첨이면 결과 구조체를 채웁니다.
bool FSlotMachineEvaluator::EvaluateSingleLine(
	const TArray<ESlotSymbol>& ResultSymbols,
	int32 LineIndex,
	FSlotLineWin& OutLineWin
)
{
	OutLineWin = FSlotLineWin();

	if (ResultSymbols.Num() != 15)
	{
		return false;
	}

	const TArray<int32> LineIndexes = FSlotMachinePaylines::GetPaylineIndexes(LineIndex);

	if (LineIndexes.Num() == 0)
	{
		return false;
	}

	for (const int32 BoardIndex : LineIndexes)
	{
		if (!ResultSymbols.IsValidIndex(BoardIndex))
		{
			return false;
		}
	}

	const ESlotLineType LineType = FSlotMachinePaylines::GetPaylineType(LineIndex);

	if (LineType == ESlotLineType::Horizontal)
	{
		int32 BestStartPosition = INDEX_NONE;
		int32 BestMatchCount = 0;
		ESlotSymbol BestSymbol = ESlotSymbol::None;

		for (int32 LinePosition = 0; LinePosition < LineIndexes.Num();)
		{
			const ESlotSymbol CurrentSymbol = ResultSymbols[LineIndexes[LinePosition]];

			if (CurrentSymbol == ESlotSymbol::None)
			{
				++LinePosition;
				continue;
			}

			int32 CurrentMatchCount = 1;

			for (int32 NextPosition = LinePosition + 1; NextPosition < LineIndexes.Num(); ++NextPosition)
			{
				if (ResultSymbols[LineIndexes[NextPosition]] != CurrentSymbol)
				{
					break;
				}

				++CurrentMatchCount;
			}

			if (CurrentMatchCount > BestMatchCount)
			{
				BestStartPosition = LinePosition;
				BestMatchCount = CurrentMatchCount;
				BestSymbol = CurrentSymbol;
			}

			LinePosition += CurrentMatchCount;
		}

		if (!IsLineEligibleForMatchCount(LineType, BestMatchCount))
		{
			return false;
		}

		OutLineWin.LineIndex = LineIndex;
		OutLineWin.LineType = LineType;
		OutLineWin.WinningSymbol = BestSymbol;
		OutLineWin.MatchCount = BestMatchCount;
		OutLineWin.ResultType = GetResultTypeByMatchCount(BestMatchCount);

		for (int32 Offset = 0; Offset < BestMatchCount; ++Offset)
		{
			OutLineWin.LineIndexes.Add(LineIndexes[BestStartPosition + Offset]);
		}

		return true;
	}

	const int32 FirstBoardIndex = LineIndexes[0];
	const ESlotSymbol FirstSymbol = ResultSymbols[FirstBoardIndex];

	if (FirstSymbol == ESlotSymbol::None)
	{
		return false;
	}

	int32 MatchCount = 1;

	for (int32 LinePosition = 1; LinePosition < LineIndexes.Num(); ++LinePosition)
	{
		const int32 BoardIndex = LineIndexes[LinePosition];

		if (ResultSymbols[BoardIndex] != FirstSymbol)
		{
			break;
		}

		++MatchCount;
	}

	if (!IsLineEligibleForMatchCount(LineType, MatchCount))
	{
		return false;
	}

	OutLineWin.LineIndex = LineIndex;
	OutLineWin.LineType = LineType;
	OutLineWin.LineIndexes = LineIndexes;
	OutLineWin.WinningSymbol = FirstSymbol;
	OutLineWin.MatchCount = MatchCount;
	OutLineWin.ResultType = GetResultTypeByMatchCount(MatchCount);

	return true;
}
// 한 판 전체 결과를 판정합니다.
// 전체화면이면 즉시 반환하고, 아니면 모든 페이라인을 검사합니다.
FSlotSpinResult FSlotMachineEvaluator::EvaluateSlotResult(const TArray<ESlotSymbol>& ResultSymbols)
{
	FSlotSpinResult SpinResult;
	SpinResult.ResultSymbols = ResultSymbols;

	if (ResultSymbols.Num() != 15)
	{
		SpinResult.bIsWin = false;
		SpinResult.bIsFullScreen = false;
		SpinResult.ResultType = ESlotResultType::Lose;
		SpinResult.WinningSymbol = ESlotSymbol::None;
		SpinResult.BestMatchCount = 0;
		return SpinResult;
	}

	ESlotSymbol FullScreenSymbol = ESlotSymbol::None;

	if (IsFullScreenMatch(ResultSymbols, FullScreenSymbol))
	{
		FSlotLineWin FullScreenWin;
		FullScreenWin.LineIndex = -1;
		FullScreenWin.LineType = ESlotLineType::FullScreen;
		FullScreenWin.WinningSymbol = FullScreenSymbol;
		FullScreenWin.ResultType = ESlotResultType::FullScreenMatch;
		FullScreenWin.MatchCount = 15;
		FullScreenWin.LineIndexes = {
			0, 1, 2, 3, 4,
			5, 6, 7, 8, 9,
			10, 11, 12, 13, 14
		};

		SpinResult.bIsWin = true;
		SpinResult.bIsFullScreen = true;
		SpinResult.ResultType = ESlotResultType::FullScreenMatch;
		SpinResult.WinningSymbol = FullScreenSymbol;
		SpinResult.BestMatchCount = 15;
		SpinResult.LineWins.Add(FullScreenWin);

		return SpinResult;
	}

	for (int32 LineIndex = 0; LineIndex < FSlotMachinePaylines::GetPaylineCount(); ++LineIndex)
	{
		FSlotLineWin LineWin;

		if (EvaluateSingleLine(ResultSymbols, LineIndex, LineWin))
		{
			SpinResult.LineWins.Add(LineWin);
		}
	}

	const bool bHasVShapeWin = SpinResult.LineWins.ContainsByPredicate(
		[](const FSlotLineWin& LineWin)
		{
			return LineWin.LineIndex == 3;
		}
	);

	const bool bHasInvertedVShapeWin = SpinResult.LineWins.ContainsByPredicate(
		[](const FSlotLineWin& LineWin)
		{
			return LineWin.LineIndex == 4;
		}
	);

	if (bHasVShapeWin || bHasInvertedVShapeWin)
	{
		SpinResult.LineWins.RemoveAll(
			[bHasVShapeWin, bHasInvertedVShapeWin](const FSlotLineWin& LineWin)
			{
				if (bHasVShapeWin && (LineWin.LineIndex == 12 || LineWin.LineIndex == 13))
				{
					return true;
				}

				if (bHasInvertedVShapeWin && (LineWin.LineIndex == 10 || LineWin.LineIndex == 15))
				{
					return true;
				}

				return false;
			}
		);
	}

	SpinResult.bIsWin = SpinResult.LineWins.Num() > 0;
	SpinResult.bIsFullScreen = false;
	SpinResult.ResultType = ESlotResultType::Lose;
	SpinResult.WinningSymbol = ESlotSymbol::None;
	SpinResult.BestMatchCount = 0;

	for (const FSlotLineWin& LineWin : SpinResult.LineWins)
	{
		if (LineWin.MatchCount > SpinResult.BestMatchCount)
		{
			SpinResult.BestMatchCount = LineWin.MatchCount;
			SpinResult.ResultType = LineWin.ResultType;
			SpinResult.WinningSymbol = LineWin.WinningSymbol;
		}
	}

	if (!SpinResult.bIsWin)
	{
		SpinResult.ResultType = ESlotResultType::Lose;
		SpinResult.WinningSymbol = ESlotSymbol::None;
		SpinResult.BestMatchCount = 0;
	}

	return SpinResult;
}