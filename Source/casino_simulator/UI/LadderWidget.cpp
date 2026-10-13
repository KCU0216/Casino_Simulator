// Copyright Epic Games, Inc. All Rights Reserved.

#include "LadderWidget.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateColorBrush.h"

void ULadderWidget::SetLadderState(int32 InRails, int32 InRows, float InTraceDuration)
{
	Rails = FMath::Max(InRails, 2);
	SelectedRail = FMath::Clamp(SelectedRail, 0, Rails - 1);
	Rows = FMath::Max(InRows, 2);
	TraceDuration = FMath::IsFinite(InTraceDuration) ? FMath::Max(InTraceDuration, 0.0f) : 0.0f;
	ClearBoard();
	OnDisplayStateChanged();
}

void ULadderWidget::SetDisplayState(const FLadderDisplayState& State)
{
	const int32 NewRails = FMath::Max(State.Rails, 2);
	const int32 NewRows = FMath::Max(State.Rows, 2);
	if (Rails != NewRails || Rows != NewRows)
	{
		ClearBoard();
	}
	Rails = NewRails;
	Rows = NewRows;
	SelectedRail = FMath::Clamp(State.SelectedRail, 0, Rails - 1);
	Odds = State.Odds;
	TraceDuration = FMath::IsFinite(State.TraceDuration) ? FMath::Max(State.TraceDuration, 0.0f) : 0.0f;
	OnDisplayStateChanged();
}

void ULadderWidget::SetOdds(const TArray<int32>& InOdds)
{
	Odds = InOdds;
	OnDisplayStateChanged();
}

void ULadderWidget::SetSelectedRail(int32 InSelectedRail)
{
	SelectedRail = FMath::Clamp(InSelectedRail, 0, Rails - 1);
	OnDisplayStateChanged();
}

void ULadderWidget::PlayPlan(const FLadderPlan& InPlan)
{
	Plan = InPlan;
	bHasPlan = true;

	TraceProgress = 0.0f;
	bTracing = true;
}

void ULadderWidget::ClearBoard()
{
	Plan = FLadderPlan();
	bHasPlan = false;
	bTracing = false;
	TraceProgress = 0.0f;
}

// ─────────── 트레이스 진행 ───────────

void ULadderWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bTracing)
	{
		return;
	}

	TraceProgress += (TraceDuration > 0.0f) ? (InDeltaTime / TraceDuration) : 1.0f;

	if (TraceProgress >= 1.0f)
	{
		TraceProgress = 1.0f;
		bTracing = false;
	}
}

// ─────────── 좌표 헬퍼 ───────────

bool ULadderWidget::GetLayoutForSize(const FVector2D& Full, FVector2D& OutOrigin, FVector2D& OutDrawSize, float& OutDx) const
{
	if (Rails < 2 || Full.X <= 0.0f || Full.Y <= 0.0f)
	{
		OutOrigin = FVector2D::ZeroVector;
		OutDrawSize = FVector2D::ZeroVector;
		OutDx = 0.0f;
		return false;
	}

	const FVector2D Avail(
		FMath::Max(Full.X - LadderPadding * 2.0f, 0.0f),
		FMath::Max(Full.Y - LadderPadding * 2.0f, 0.0f));

	OutDrawSize = Avail;
	if (LadderSize.X > 0.0f && LadderSize.Y > 0.0f)
	{
		OutDrawSize.X = FMath::Min(LadderSize.X, Avail.X);
		OutDrawSize.Y = FMath::Min(LadderSize.Y, Avail.Y);
	}
	OutOrigin = (Full - OutDrawSize) * 0.5f;
	OutDx = OutDrawSize.X / (Rails - 1);
	return true;
}

FVector2D ULadderWidget::GetRailTop(int32 Rail) const
{
	FVector2D Origin, DrawSize; float Dx;
	if (!GetLayoutForSize(GetCachedGeometry().GetLocalSize(), Origin, DrawSize, Dx))
	{
		return FVector2D::ZeroVector;
	}
	Rail = FMath::Clamp(Rail, 0, Rails - 1);
	return FVector2D(Origin.X + Rail * Dx, Origin.Y);
}

FVector2D ULadderWidget::GetRailBottom(int32 Rail) const
{
	FVector2D Origin, DrawSize; float Dx;
	if (!GetLayoutForSize(GetCachedGeometry().GetLocalSize(), Origin, DrawSize, Dx))
	{
		return FVector2D::ZeroVector;
	}
	Rail = FMath::Clamp(Rail, 0, Rails - 1);
	return FVector2D(Origin.X + Rail * Dx, Origin.Y + DrawSize.Y);
}

float ULadderWidget::GetRailAnchorX(int32 Rail) const
{
	Rail = FMath::Clamp(Rail, 0, Rails - 1);
	const float Start = (1.0f - LadderWidthRatio) * 0.5f;
	return Start + (Rail + 0.5f) / Rails * LadderWidthRatio;
}

float ULadderWidget::GetLadderTopAnchorY() const
{
	return (1.0f - LadderHeightRatio) * 0.5f;
}

float ULadderWidget::GetLadderBottomAnchorY() const
{
	return 1.0f - (1.0f - LadderHeightRatio) * 0.5f;
}

// ─────────── 그리기 ───────────

int32 ULadderWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (Rails < 2 || Rows < 2)
	{
		return LayerId;
	}

	const FVector2D Full = AllottedGeometry.GetLocalSize();

	const float UsableW = Full.X * LadderWidthRatio;
	const float UsableH = Full.Y * LadderHeightRatio;
	const float OriginX = (Full.X - UsableW) * 0.5f;
	const float OriginY = (Full.Y - UsableH) * 0.5f;

	auto RailX = [&](float r) { return OriginX + (r + 0.5f) / Rails * UsableW; };

	const float RungTop    = OriginY + RailOverhang;
	const float RungHeight = FMath::Max(UsableH - RailOverhang * 2.0f, 0.0f);
	const float dy = RungHeight / (Rows - 1);
	const FPaintGeometry PaintGeo = AllottedGeometry.ToPaintGeometry();

	// 세로줄
	for (int32 g = 0; g < Rails; ++g)
	{
		const float x = RailX(g);
		TArray<FVector2D> Pts;
		Pts.Add(FVector2D(x, OriginY));
		Pts.Add(FVector2D(x, OriginY + UsableH));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeo, Pts,
			ESlateDrawEffect::None, LineColor, true, LineThickness * 0.7f);
	}

	// 가로줄 (실제 + 위장 통합, 같은 색). Plan.DrawRung 이 없으면(판 비움) 건너뜀 → 세로줄만.
	for (int32 r = 0; r < FMath::Min(Plan.DrawRung.Num(), Rows); ++r)
	{
		const float y = RungTop + r * dy;
		const TArray<bool>& Cells = Plan.DrawRung[r].Cells;
		for (int32 g = 0; g < FMath::Min(Cells.Num(), Rails - 1); ++g)
		{
			if (!Cells[g]) continue;
			TArray<FVector2D> Pts;
			Pts.Add(FVector2D(RailX(g), y));
			Pts.Add(FVector2D(RailX(g + 1), y));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeo, Pts,
				ESlateDrawEffect::None, LineColor, true, LineThickness);
		}
	}

	// ── 실제 경로 트레이스(빨강) + 구슬 ──
	if (bHasPlan && Plan.PathRails.Num() >= 2 && TraceProgress > 0.0f)
	{
		auto RowY = [&](int32 Row) { return RungTop + Row * dy; };

		TArray<FVector2D> Poly;
		Poly.Add(FVector2D(RailX(Plan.PathRails[0]), OriginY));
		for (int32 k = 0; k < FMath::Min(Plan.CrossRows.Num(), Plan.PathRails.Num() - 1); ++k)
		{
			const float cy = RowY(Plan.CrossRows[k]);
			Poly.Add(FVector2D(RailX(Plan.PathRails[k]),     cy));
			Poly.Add(FVector2D(RailX(Plan.PathRails[k + 1]), cy));
		}
		Poly.Add(FVector2D(RailX(Plan.PathRails.Last()), OriginY + UsableH));

		float Total = 0.0f;
		for (int32 i = 1; i < Poly.Num(); ++i)
		{
			Total += FVector2D::Distance(Poly[i - 1], Poly[i]);
		}
		const float Target = Total * FMath::Clamp(TraceProgress, 0.0f, 1.0f);

		TArray<FVector2D> Trav;
		Trav.Add(Poly[0]);
		FVector2D Head = Poly[0];
		float Acc = 0.0f;
		for (int32 i = 1; i < Poly.Num(); ++i)
		{
			const float Seg = FVector2D::Distance(Poly[i - 1], Poly[i]);
			if (Seg <= KINDA_SMALL_NUMBER) continue;
			if (Acc + Seg <= Target)
			{
				Trav.Add(Poly[i]);
				Head = Poly[i];
				Acc += Seg;
			}
			else
			{
				const float t = (Target - Acc) / Seg;
				Head = FMath::Lerp(Poly[i - 1], Poly[i], t);
				Trav.Add(Head);
				break;
			}
		}

		if (Trav.Num() >= 2)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, PaintGeo, Trav,
				ESlateDrawEffect::None, TraceColor, true, LineThickness + 1.0f);
		}

		static const FSlateColorBrush WhiteFill(FLinearColor::White);
		const FSlateBrush* Brush = (MarbleBrush.GetResourceObject() != nullptr) ? &MarbleBrush : &WhiteFill;
		const FVector2D MHalf(MarbleSize * 0.5f, MarbleSize * 0.5f);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(MarbleSize, MarbleSize),
				FSlateLayoutTransform(1.0f, Head - MHalf)),
			Brush, ESlateDrawEffect::None, MarbleColor);

		return LayerId + 3;
	}

	return LayerId + 1;
}
