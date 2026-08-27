#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlotMachineTypes.h"
#include "SlotMachineBlueprintLibrary.generated.h"

UCLASS()
class CASINO_SIMULATOR_API USlotMachineBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 라인 번호에 해당하는 보드 칸 인덱스 배열을 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static TArray<int32> GetPaylineIndexes(int32 LineIndex);

	// 라인 번호에 해당하는 라인 종류를 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static ESlotLineType GetPaylineType(int32 LineIndex);

	// 현재 슬롯머신이 검사할 전체 페이라인 개수를 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static int32 GetPaylineCount();

	// 15칸 전체가 같은 심볼인지 검사 전체화면 잭팟은 일반 라인 당첨보다 먼저 검사
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static bool IsFullScreenMatch(const TArray<ESlotSymbol>& ResultSymbols, ESlotSymbol& OutSymbol);

	// 일치한 심볼 개수를 족보 타입으로 변환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static ESlotResultType GetResultTypeByMatchCount(int32 MatchCount);

	// 해당 라인 종류에서 현재 매치 개수가 보상 가능한지 검사
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static bool IsLineEligibleForMatchCount(ESlotLineType LineType, int32 MatchCount);

	// 라인 하나를 왼쪽부터 검사
	// 유효한 당첨이면 OutLineWin에 결과를 채우고 true를 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static bool EvaluateSingleLine(
		const TArray<ESlotSymbol>& ResultSymbols,
		int32 LineIndex,
		FSlotLineWin& OutLineWin
	);

	// 5x3 결과 배열 전체를 판정 전체화면 잭팟을 먼저 검사하고, 아니면 모든 페이라인을 검사
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static FSlotSpinResult EvaluateSlotResult(const TArray<ESlotSymbol>& ResultSymbols);

	// 5x3 결과 배열을 판정하고, DataTable을 이용해 보상 금액까지 계산합니다.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static FSlotSpinResult EvaluateSlotResultWithPayout(
		const TArray<ESlotSymbol>& ResultSymbols,
		UDataTable* SymbolDataTable,
		UDataTable* PayoutDataTable,
		int32 BetAmount
	);

	// 슬롯 심볼 enum을 DataTable Row Name으로 변환합니다.
	// C++ enum 문자열은 Row Name과 다를 수 있으므로 BP에서 DataTable을 찾을 때 이 함수를 사용합니다.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static FName GetSlotSymbolRowName(ESlotSymbol Symbol);

	// Outcome Weight DataTable을 읽어서 이번 스핀의 목표 결과를 가중치 기반으로 뽑습니다.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Slot Machine")
	static ESlotSpinOutcome RollSpinOutcomeByWeight(UDataTable* OutcomeWeightDataTable);
};