// Copyright Epic Games, Inc. All Rights Reserved.

#include "LadderWidget.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateColorBrush.h"

// ─────────── 머신 연결 ───────────

void ULadderWidget::SetMachine(ALadderMachine* InMachine)
{
	// 기존 머신 구독 해제
	if (Machine)
	{
		Machine->OnPlayStarted.RemoveDynamic(this, &ULadderWidget::HandlePlayStarted);
		Machine->OnBoardCleared.RemoveDynamic(this, &ULadderWidget::HandleBoardCleared);
		Machine->OnRailCountChanged.RemoveDynamic(this, &ULadderWidget::HandleRailCountChanged);
	}

	Machine = InMachine;
	if (!Machine)
	{
		return;
	}

	// 그리기에 필요한 dispatcher만 구독 (돈/모드/커서 등 UI 표시용은 BP가 직접 구독).
	Machine->OnPlayStarted.AddDynamic(this, &ULadderWidget::HandlePlayStarted);
	Machine->OnBoardCleared.AddDynamic(this, &ULadderWidget::HandleBoardCleared);
	Machine->OnRailCountChanged.AddDynamic(this, &ULadderWidget::HandleRailCountChanged);

	// 현재 상태 pull (방송 타이밍과 무관하게 즉시 동기화)
	Rails = Machine->GetRails();
	Rows  = Machine->GetRows();
	Plan = FLadderPlan();
	bHasPlan = false;
	bTracing = false;
	TraceProgress = 0.0f;

	// BP에서 머신 dispatcher 바인딩 + 초기 UI 구성할 수 있게 알림.
	OnMachineAttached();
}

void ULadderWidget::NativeDestruct()
{
	if (Machine)
	{
		Machine->OnPlayStarted.RemoveDynamic(this, &ULadderWidget::HandlePlayStarted);
		Machine->OnBoardCleared.RemoveDynamic(this, &ULadderWidget::HandleBoardCleared);
		Machine->OnRailCountChanged.RemoveDynamic(this, &ULadderWidget::HandleRailCountChanged);
	}
	Super::NativeDestruct();
}

void ULadderWidget::HandlePlayStarted(FLadderPlan InPlan)
{
	Plan = InPlan;
	bHasPlan = true;

	if (Machine)
	{
		Rails = Machine->GetRails();
		Rows  = Machine->GetRows();
	}

	TraceProgress = 0.0f;
	bTracing = true;
}

void ULadderWidget::HandleBoardCleared()
{
	Plan = FLadderPlan();
	bHasPlan = false;
	bTracing = false;
	TraceProgress = 0.0f;
}

void ULadderWidget::HandleRailCountChanged(int32 NewRails)
{
	Rails = NewRails;
	if (Machine)
	{
		Rows = Machine->GetRows();
	}
}

// ─────────── 트레이스 진행 ───────────

void ULadderWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bTracing)
	{
		return;
	}

	// 트레이스 길이는 머신 값과 공유(서버 정산 타이머와 동기화). 없으면 위젯 기본값.
	const float Dur = (Machine && Machine->TraceDuration > 0.0f) ? Machine->TraceDuration : TraceDuration;
	TraceProgress += (Dur > 0.0f) ? (InDeltaTime / Dur) : 1.0f;

	if (TraceProgress >= 1.0f)
	{
		TraceProgress = 1.0f;
		bTracing = false;
		// 결과창/정산은 전부 서버가 담당(Multicast_Result). 뷰는 구슬을 바닥에 세워둘 뿐.
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
	for (int32 r = 0; r < Plan.DrawRung.Num(); ++r)
	{
		const float y = RungTop + r * dy;
		const TArray<bool>& Cells = Plan.DrawRung[r].Cells;
		for (int32 g = 0; g < Cells.Num(); ++g)
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
		for (int32 k = 0; k < Plan.CrossRows.Num(); ++k)
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
