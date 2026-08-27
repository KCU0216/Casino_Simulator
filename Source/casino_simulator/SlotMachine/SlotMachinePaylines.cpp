#include "SlotMachine/SlotMachinePaylines.h"

// 라인 번호를 실제 보드 칸 인덱스 배열로 변환
TArray<int32> FSlotMachinePaylines::GetPaylineIndexes(int32 LineIndex)
{
	switch (LineIndex)
	{
	case 0:
		return { 5, 6, 7, 8, 9 };

	case 1:
		return { 0, 1, 2, 3, 4 };

	case 2:
		return { 10, 11, 12, 13, 14 };

	case 3:
		return { 10, 6, 2, 8, 14 };

	case 4:
		return { 0, 6, 12, 8, 4 };

	case 5:
		return { 0, 5, 10 };

	case 6:
		return { 1, 6, 11 };

	case 7:
		return { 2, 7, 12 };

	case 8:
		return { 3, 8, 13 };

	case 9:
		return { 4, 9, 14 };

	case 10:
		return { 0, 6, 12 };

	case 11:
		return { 1, 7, 13 };

	case 12:
		return { 2, 8, 14 };

	case 13:
		return { 10, 6, 2 };

	case 14:
		return { 11, 7, 3 };

	case 15:
		return { 12, 8, 4 };

	default:
		return {};
	}
}

// 라인 번호를 라인 종류로 변환
ESlotLineType FSlotMachinePaylines::GetPaylineType(int32 LineIndex)
{
	if (LineIndex >= 0 && LineIndex <= 2)
	{
		return ESlotLineType::Horizontal;
	}

	if (LineIndex == 3)
	{
		return ESlotLineType::VShape;
	}

	if (LineIndex == 4)
	{
		return ESlotLineType::InvertedVShape;
	}

	if (LineIndex >= 5 && LineIndex <= 9)
	{
		return ESlotLineType::Vertical;
	}

	if (LineIndex >= 10 && LineIndex <= 15)
	{
		return ESlotLineType::Diagonal;
	}

	return ESlotLineType::Horizontal;
}

// 현재 사용하는 페이라인 총 개수를 반환
int32 FSlotMachinePaylines::GetPaylineCount()
{
	return 16;
}