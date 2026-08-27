#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SlotMachineTypes.generated.h"

UENUM(BlueprintType)
enum class ESlotSymbol : uint8
{
	None UMETA(DisplayName = "None"),

	Cherry UMETA(DisplayName = "Cherry"),
	Bell UMETA(DisplayName = "Bell"),
	Game UMETA(DisplayName = "Game"),
	Diamond UMETA(DisplayName = "Diamond"),
	Watermelon UMETA(DisplayName = "Watermelon"),
	Star UMETA(DisplayName = "Star"),
	Grape UMETA(DisplayName = "Grape")
};

UENUM(BlueprintType)
enum class ESlotResultType : uint8
{
	Lose UMETA(DisplayName = "Lose"),
	ThreeOfAKind UMETA(DisplayName = "Three Of A Kind"),
	FourOfAKind UMETA(DisplayName = "Four Of A Kind"),
	FiveOfAKind UMETA(DisplayName = "Five Of A Kind"),
	FullScreenMatch UMETA(DisplayName = "Full Screen Match")
};

UENUM(BlueprintType)
enum class ESlotRewardType : uint8
{
	None UMETA(DisplayName = "None"),
	Money UMETA(DisplayName = "Money"),
	FreeSpin UMETA(DisplayName = "Free Spin")
};

UENUM(BlueprintType)
enum class ESlotLineType : uint8
{
	Horizontal UMETA(DisplayName = "Horizontal"),
	Vertical UMETA(DisplayName = "Vertical"),
	Diagonal UMETA(DisplayName = "Diagonal"),
	VShape UMETA(DisplayName = "V Shape"),
	InvertedVShape UMETA(DisplayName = "Inverted V Shape"),
	FullScreen UMETA(DisplayName = "Full Screen")
};

UENUM(BlueprintType)
enum class ESlotSpinOutcome : uint8
{
	Lose UMETA(DisplayName = "Lose"),
	Horizontal3 UMETA(DisplayName = "Horizontal 3"),
	Horizontal4 UMETA(DisplayName = "Horizontal 4"),
	Horizontal5 UMETA(DisplayName = "Horizontal 5"),
	Vertical3 UMETA(DisplayName = "Vertical 3"),
	Diagonal3 UMETA(DisplayName = "Diagonal 3"),
	VShape5 UMETA(DisplayName = "V Shape 5"),
	InvertedVShape5 UMETA(DisplayName = "Inverted V Shape 5"),
	FullScreen UMETA(DisplayName = "Full Screen")
};

USTRUCT(BlueprintType)
struct FSlotOutcomeWeightData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotSpinOutcome Outcome = ESlotSpinOutcome::Lose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Weight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ExpectedChance = 0.0f;
};

USTRUCT(BlueprintType)
struct FSlotSymbolData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotSymbol Symbol = ESlotSymbol::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TargetAngleX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SymbolMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct FSlotPayoutData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotResultType ResultType = ESlotResultType::Lose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotLineType LineType = ESlotLineType::Horizontal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MatchCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotRewardType RewardType = ESlotRewardType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ResultMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayText;
};

USTRUCT(BlueprintType)
struct FSlotLineWin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 LineIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotLineType LineType = ESlotLineType::Horizontal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<int32> LineIndexes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotSymbol WinningSymbol = ESlotSymbol::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotResultType ResultType = ESlotResultType::Lose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotRewardType RewardType = ESlotRewardType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MatchCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SymbolMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ResultMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RewardAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayText;
};

USTRUCT(BlueprintType)
struct FSlotSpinResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsWin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsFullScreen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotResultType ResultType = ESlotResultType::Lose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESlotSymbol WinningSymbol = ESlotSymbol::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BestMatchCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TotalRewardAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ESlotSymbol> ResultSymbols;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSlotLineWin> LineWins;
};