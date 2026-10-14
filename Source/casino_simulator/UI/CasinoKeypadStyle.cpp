#include "UI/CasinoKeypadStyle.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

namespace CasinoKeypadStyle
{
void StyleButton(UButton* Button)
{
	if (!IsValid(Button)) return;
	FButtonStyle Style = Button->GetStyle();
	if (Button->GetFName() == TEXT("Button_BG"))
	{
		// Retain the existing background hit target and its Blueprint events.
		Style.Normal.DrawAs = Style.Hovered.DrawAs = Style.Pressed.DrawAs = Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
		Button->SetStyle(Style);
		return;
	}
	const FLinearColor Navy(.023f, .032f, .049f, 1.f);
	const FLinearColor Gold(.76f, .49f, .15f, 1.f);
	const FLinearColor Ivory(.91f, .86f, .75f, 1.f);
	const bool bPrimary = Button->GetFName() == TEXT("Button_OK");
	const auto ApplyBrush = [](FSlateBrush& Brush, const FLinearColor& Color)
	{
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(8.f, FSlateColor(FLinearColor(.76f, .49f, .15f, .45f)), 1.f);
	};
	ApplyBrush(Style.Normal, bPrimary ? Gold : Navy);
	ApplyBrush(Style.Hovered, bPrimary ? FLinearColor(.94f, .67f, .29f) : FLinearColor(.05f, .065f, .095f));
	ApplyBrush(Style.Pressed, bPrimary ? FLinearColor(.58f, .33f, .075f) : FLinearColor(.035f, .043f, .06f));
	ApplyBrush(Style.Disabled, Navy);
	Style.NormalForeground = Style.HoveredForeground = Style.PressedForeground = FSlateColor(bPrimary ? FLinearColor(.007f, .011f, .019f) : Ivory);
	Style.DisabledForeground = FSlateColor(FLinearColor(.39f, .46f, .55f));
	Style.NormalPadding = Style.PressedPadding = FMargin(10.f);
	Button->SetStyle(Style);
	Button->SetBackgroundColor(FLinearColor::White);
	if (UTextBlock* Text = Cast<UTextBlock>(Button->GetContent()))
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = bPrimary ? 22.f : 28.f;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor::UseForeground());
	}
}
}

void ApplyCasinoKeypadStyle(UUserWidget* Keypad)
{
	if (!IsValid(Keypad)) return;
	for (int32 Digit = 0; Digit < 10; ++Digit)
		CasinoKeypadStyle::StyleButton(Cast<UButton>(Keypad->GetWidgetFromName(FName(*FString::Printf(TEXT("Button_%d"), Digit)))));
	for (const TCHAR* Name : { TEXT("Button_Delete"), TEXT("Button_Backspace"), TEXT("Button_OK"), TEXT("Button_BG") })
		CasinoKeypadStyle::StyleButton(Cast<UButton>(Keypad->GetWidgetFromName(FName(Name))));
}

void ApplyCasinoPaymentKeypadLayout(UUserWidget* Keypad)
{
	if (!IsValid(Keypad) || !Keypad->GetParent()
		|| Keypad->GetParent()->GetFName() != TEXT("CasinoPaymentKeypadBounds")) return;
	UGridPanel* Grid = Cast<UGridPanel>(Keypad->GetWidgetFromName(TEXT("GridPanel_35")));
	UCanvasPanelSlot* Slot = Grid ? Cast<UCanvasPanelSlot>(Grid->Slot) : nullptr;
	if (!Slot) return;
	Slot->SetAnchors(FAnchors(.5f, .5f));
	Slot->SetOffsets(FMargin(0.f, 0.f, 560.f, 320.f));
	Slot->SetAlignment(FVector2D(.5f, .5f));
	for (int32 Index = 0; Index < 4; ++Index)
	{ Grid->SetColumnFill(Index, 1.f); Grid->SetRowFill(Index, 1.f); }
	for (UWidget* Cell : Grid->GetAllChildren())
	{
		if (USizeBox* Size = Cast<USizeBox>(Cell))
		{ Size->SetWidthOverride(130.f); Size->SetHeightOverride(72.f); }
		if (UGridSlot* CellSlot = Cast<UGridSlot>(Cell->Slot)) CellSlot->SetPadding(FMargin(4.f));
	}
}
