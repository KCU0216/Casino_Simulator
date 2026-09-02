// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NumberKeypadWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNumberKeypadConfirmed, int32, Amount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNumberKeypadCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNumberKeypadValueChanged, int32, Amount, const FText&, DisplayText);

/**
 * Reusable integer keypad logic.
 *
 * BP owns the look and buttons. This class only owns numeric input, min/max validation,
 * display text, and confirm/cancel delegates for callers like blackjack betting.
 */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API UNumberKeypadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Number Keypad")
	void ConfigureKeypad(int32 InMinValue, int32 InMaxValue, int32 InInitialValue = 0, bool bInAllowEmptyConfirm = false);

	UFUNCTION(BlueprintCallable, Category="Number Keypad")
	void PressDigit(int32 Digit);

	UFUNCTION(BlueprintCallable, Category="Number Keypad")
	void Backspace();

	UFUNCTION(BlueprintCallable, Category="Number Keypad")
	void ClearInput();

	UFUNCTION(BlueprintCallable, Category="Number Keypad")
	void Confirm();

	UFUNCTION(BlueprintCallable, Category="Number Keypad")
	void Cancel();

	UFUNCTION(BlueprintCallable, Category="Number Keypad")
	void SetValue(int32 NewValue);

	UFUNCTION(BlueprintPure, Category="Number Keypad")
	int32 GetValue() const { return CurrentValue; }

	UFUNCTION(BlueprintPure, Category="Number Keypad")
	FText GetDisplayText() const;

	UFUNCTION(BlueprintPure, Category="Number Keypad")
	bool CanConfirm() const;

	UPROPERTY(BlueprintAssignable, Category="Number Keypad")
	FNumberKeypadConfirmed OnConfirmed;

	UPROPERTY(BlueprintAssignable, Category="Number Keypad")
	FNumberKeypadCancelled OnCancelled;

	UPROPERTY(BlueprintAssignable, Category="Number Keypad")
	FNumberKeypadValueChanged OnValueChanged;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Number Keypad", meta=(ClampMin="0"))
	int32 MinValue = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Number Keypad", meta=(ClampMin="1"))
	int32 MaxValue = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Number Keypad", meta=(ClampMin="1", ClampMax="10"))
	int32 MaxDigits = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Number Keypad")
	bool bAllowEmptyConfirm = false;

	UPROPERTY(BlueprintReadOnly, Category="Number Keypad")
	FString InputText;

	UPROPERTY(BlueprintReadOnly, Category="Number Keypad")
	int32 CurrentValue = 0;

	UFUNCTION(BlueprintImplementableEvent, Category="Number Keypad", meta=(DisplayName="On Display Text Changed"))
	void BP_OnDisplayTextChanged(const FText& DisplayText, int32 Amount);

	UFUNCTION(BlueprintImplementableEvent, Category="Number Keypad", meta=(DisplayName="On Confirm Availability Changed"))
	void BP_OnConfirmAvailabilityChanged(bool bCanConfirm);

private:
	void CommitInputText(const FString& NewInputText);
	void BroadcastValueChanged();
};
