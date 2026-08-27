// Copyright Epic Games, Inc. All Rights Reserved.
//
// LadderWidget = View.
//  - 그리기 + 트레이스 연출 + 좌표 헬퍼만 담당. 게임 로직/돈/규칙은 전혀 없음.
//  - ALadderMachine 을 참조하고 그 dispatcher 를 구독해서 상태를 반영한다.
//  - Play 시 머신이 넘긴 FLadderPlan 을 받아 그리고, 트레이스가 끝나면 머신에 정산을 요청한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LadderMachine.h"   // ALadderMachine, ELadderMode, FLadderPlan
#include "LadderWidget.generated.h"

UCLASS()
class CASINO_SIMULATOR_API ULadderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ─────────── 그리기 파라미터 ───────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float LineThickness = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FLinearColor LineColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float LadderPadding = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	float RailOverhang = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LadderWidthRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LadderHeightRatio = 1.0f;

	/** 사다리 픽셀 크기 강제. (0,0)이면 위젯 크기를 그대로 채움. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FVector2D LadderSize = FVector2D(0.0f, 0.0f);

	// ─────────── 트레이스(구슬) 연출 파라미터 ───────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	float TraceDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FSlateBrush MarbleBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FLinearColor MarbleColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	float MarbleSize = 22.0f;

	// ─────────── 머신 연결 ───────────
	/** 이 뷰가 표시하는 머신. SetMachine 으로 주입. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder")
	ALadderMachine* Machine = nullptr;

	/** 머신 참조를 받아 dispatcher 구독 + 현재 상태 동기화. BP(상호작용/BeginPlay)에서 1회 호출. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void SetMachine(ALadderMachine* InMachine);

	/** SetMachine 으로 머신이 연결된 직후 호출됨 — BP에서 머신 dispatcher 바인딩/초기 UI 구성용.
	 *  (여기서 OnCursorChanged/OnRailCountChanged/OnBetChanged/OnResultsChanged 등을 바인딩하고,
	 *   마지막에 Machine.BroadcastInitialState() 를 부르면 초기 버튼/텍스트가 채워짐.) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder")
	void OnMachineAttached();

	// ─────────── 좌표 헬퍼 (BP 버튼/결과칸 배치용, 위젯 로컬 좌표/앵커) ───────────
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector2D GetRailTop(int32 Rail) const;

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector2D GetRailBottom(int32 Rail) const;

	UFUNCTION(BlueprintPure, Category = "Ladder")
	float GetRailAnchorX(int32 Rail) const;

	UFUNCTION(BlueprintPure, Category = "Ladder")
	float GetLadderTopAnchorY() const;

	UFUNCTION(BlueprintPure, Category = "Ladder")
	float GetLadderBottomAnchorY() const;

	UFUNCTION(BlueprintPure, Category = "Ladder")
	bool IsTracing() const { return bTracing; }

protected:
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// ── 머신 dispatcher 핸들러 ──
	UFUNCTION() void HandlePlayStarted(FLadderPlan InPlan);
	UFUNCTION() void HandleBoardCleared();
	UFUNCTION() void HandleRailCountChanged(int32 NewRails);

	/** 주어진 위젯 크기로 그리기 영역(Origin/DrawSize)과 세로줄 간격(dx) 계산. */
	bool GetLayoutForSize(const FVector2D& Full, FVector2D& OutOrigin, FVector2D& OutDrawSize, float& OutDx) const;

	// ── 그리기용 미러 상태 (머신에서 동기화) ──
	int32 Rails = 3;
	int32 Rows = 12;

	// ── 현재 판 (머신에서 받은 그리기 데이터) ──
	FLadderPlan Plan;
	bool bHasPlan = false;

	// ── 트레이스 상태 ──
	bool  bTracing = false;
	float TraceProgress = 0.0f;   // 0..1
};
