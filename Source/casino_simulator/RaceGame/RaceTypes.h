// RaceTypes.h  ─ 공유 타입 (Runner·Manager·GameState·UI 전부 이거 include)
#pragma once

#include "CoreMinimal.h"
#include "RaceTypes.generated.h"

class APlayerState;

UENUM(BlueprintType)
enum class ERacePhase : uint8
{
	Idle     UMETA(DisplayName = "대기"),
	Betting  UMETA(DisplayName = "배팅"),
	Racing   UMETA(DisplayName = "레이스"),
	Finished UMETA(DisplayName = "종료")
};

// 러너 스탯 (UI/배당 표시용). 매니저가 매 라운드 롤 → 러너에 주입 → 리플리케이트.
USTRUCT(BlueprintType)
struct FRaceRunnerStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Race") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Race") int32  Age = 60;
	UPROPERTY(BlueprintReadOnly, Category = "Race") float  BaseSpeed = 200.f;
	UPROPERTY(BlueprintReadOnly, Category = "Race") float  Odds = 2.f;
	UPROPERTY(BlueprintReadOnly, Category = "Race") float  AwakenChance = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Race") float  StumbleChance = 0.f;
};

// 이번 판 "주행 레시피". 서버가 레이스 시작 때 1번 롤 → 러너에 리플리케이트.
// 클라는 이 레시피만 받아서 매 프레임 위치를 스스로 계산 (위치 스트리밍 X).
USTRUCT(BlueprintType)
struct FRunnerRaceScript
{
	GENERATED_BODY()

	UPROPERTY() FVector StartLoc = FVector::ZeroVector;
	UPROPERTY() FVector Dir = FVector(1, 0, 0);
	UPROPERTY() float   TrackLength = 3000.f;
	UPROPERTY() float   Speed = 200.f;          // 운 반영된 실제 속도 (레이스 내내 고정)
	UPROPERTY() bool    bWillAwaken = false;
	UPROPERTY() float   AwakenAtPos = 0.f;      // 이 위치 지나면 각성
	UPROPERTY() bool    bWillStumble = false;
	UPROPERTY() float   StumbleAtPos = 0.f;     // 이 위치 지나면 삐끗
};

// 마권 1장(묶음). 매니저가 원장으로 보유. 구매 시점 배당(Odds) 고정.
USTRUCT(BlueprintType)
struct FBetTicket
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Bet") int32 TicketId = 0;
	// 누가 (폰 리스폰돼도 유지되게 PlayerState 기준)
	UPROPERTY(BlueprintReadOnly, Category = "Bet") TObjectPtr<APlayerState> Buyer = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "Bet") FString BuyerName;
	// 어느 러너에
	UPROPERTY(BlueprintReadOnly, Category = "Bet") int32 RunnerIndex = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Bet") FString RunnerName;
	// 장당 배팅액 / 개수 / 구매시점 배당
	UPROPERTY(BlueprintReadOnly, Category = "Bet") int32 Amount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bet") int32 Count = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Bet") float Odds = 1.f;
	// 정산 결과 (true=당첨, 환전 대기)
	UPROPERTY(BlueprintReadOnly, Category = "Bet") bool bWon = false;

	int32 Payout() const { return FMath::FloorToInt(Amount * Count * Odds); }
};
