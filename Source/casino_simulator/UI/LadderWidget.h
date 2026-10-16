// Copyright Epic Games, Inc. All Rights Reserved.
//
// Presentation only: scripts supply dimensions, timing, and a generated plan.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LadderGenerator.h"
#include "LadderDisplayState.h"
#include "LadderWidget.generated.h"

UCLASS()
class CASINO_SIMULATOR_API ULadderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Ladder|State")
	int32 Rails = 3;

	/** Zero-based rail index used for the selection image. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder|State")
	int32 SelectedRail = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ladder|State")
	int32 Rows = 12;

	/** Multipliers in rail index order, supplied separately from the play plan. */
	UPROPERTY(BlueprintReadOnly, Category = "Ladder|State")
	TArray<int32> Odds;

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
	float TraceDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FLinearColor TraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FSlateBrush MarbleBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	FLinearColor MarbleColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Trace")
	float MarbleSize = 22.0f;

	/** Configure the display and clear any previous animation. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void SetLadderState(int32 InRails, int32 InRows, float InTraceDuration);

	/** Apply all display fields together without restarting an unchanged board. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void SetDisplayState(const FLadderDisplayState& State);

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void SetOdds(const TArray<int32>& InOdds);

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void SetSelectedRail(int32 InSelectedRail);

	/** Read Rails, SelectedRail, and Odds here to refresh presentation widgets. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ladder")
	void OnDisplayStateChanged();

	/** Display a supplied plan and restart its trace animation. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void PlayPlan(const FLadderPlan& InPlan);

	/** Clear the plan and trace, retaining the configured vertical rails. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void ClearBoard();

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
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 주어진 위젯 크기로 그리기 영역(Origin/DrawSize)과 세로줄 간격(dx) 계산. */
	bool GetLayoutForSize(const FVector2D& Full, FVector2D& OutOrigin, FVector2D& OutDrawSize, float& OutDx) const;

	// Supplied drawing data.
	FLadderPlan Plan;
	bool bHasPlan = false;

	// ── 트레이스 상태 ──
	bool  bTracing = false;
	float TraceProgress = 0.0f;   // 0..1
};
