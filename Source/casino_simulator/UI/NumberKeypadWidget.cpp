// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/NumberKeypadWidget.h"

void UNumberKeypadWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetValue(CurrentValue);
}

void UNumberKeypadWidget::ConfigureKeypad(int32 InMinValue, int32 InMaxValue, int32 InInitialValue, bool bInAllowEmptyConfirm)
{
	MinValue = FMath::Max(0, InMinValue);
	MaxValue = FMath::Max(MinValue, InMaxValue);
	bAllowEmptyConfirm = bInAllowEmptyConfirm;
	SetValue(InInitialValue);
}

void UNumberKeypadWidget::PressDigit(int32 Digit)
{
	if (Digit < 0 || Digit > 9)
	{
		return;
	}

	if (InputText.Len() >= MaxDigits)
	{
		return;
	}

	FString NewInputText = InputText;
	if (NewInputText == TEXT("0"))
	{
		NewInputText.Reset();
	}

	NewInputText.AppendInt(Digit);
	CommitInputText(NewInputText);
}

void UNumberKeypadWidget::Backspace()
{
	if (InputText.IsEmpty())
	{
		return;
	}

	FString NewInputText = InputText;
	NewInputText.LeftChopInline(1);
	CommitInputText(NewInputText);
}

void UNumberKeypadWidget::ClearInput()
{
	CommitInputText(TEXT(""));
}

void UNumberKeypadWidget::Confirm()
{
	if (!CanConfirm())
	{
		return;
	}

	OnConfirmed.Broadcast(CurrentValue);
}

void UNumberKeypadWidget::Cancel()
{
	OnCancelled.Broadcast();
}

void UNumberKeypadWidget::SetValue(int32 NewValue)
{
	const int32 ClampedValue = FMath::Clamp(NewValue, MinValue, MaxValue);
	CommitInputText(ClampedValue > 0 ? FString::FromInt(ClampedValue) : FString());
}

FText UNumberKeypadWidget::GetDisplayText() const
{
	return InputText.IsEmpty() ? FText::FromString(TEXT("0")) : FText::FromString(InputText);
}

bool UNumberKeypadWidget::CanConfirm() const
{
	if (InputText.IsEmpty())
	{
		return bAllowEmptyConfirm && CurrentValue >= MinValue && CurrentValue <= MaxValue;
	}

	return CurrentValue >= MinValue && CurrentValue <= MaxValue;
}

void UNumberKeypadWidget::CommitInputText(const FString& NewInputText)
{
	InputText = NewInputText.Left(MaxDigits);

	if (InputText.IsEmpty())
	{
		CurrentValue = 0;
	}
	else
	{
		CurrentValue = FMath::Clamp(FCString::Atoi(*InputText), 0, MaxValue);
		if (FString::FromInt(CurrentValue) != InputText)
		{
			InputText = CurrentValue > 0 ? FString::FromInt(CurrentValue) : FString();
		}
	}

	BroadcastValueChanged();
}

void UNumberKeypadWidget::BroadcastValueChanged()
{
	const FText DisplayText = GetDisplayText();
	OnValueChanged.Broadcast(CurrentValue, DisplayText);
	BP_OnDisplayTextChanged(DisplayText, CurrentValue);
	BP_OnConfirmAvailabilityChanged(CanConfirm());
}
