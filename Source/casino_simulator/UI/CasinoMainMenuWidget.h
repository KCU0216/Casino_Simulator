#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CasinoMainMenuWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UCheckBox;
class UComboBoxString;
class UHorizontalBox;
class USlider;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;

UCLASS()
class CASINO_SIMULATOR_API UCasinoMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

private:
	void BuildSettingsUI(UCanvasPanel* SettingsPanel);
	UButton* MakeTabButton(UHorizontalBox* Parent, const FText& Label, FName Name);
	UHorizontalBox* MakeSettingRow(UVerticalBox* Parent, const FText& Label, FName Name);
	UTextBlock* MakeText(const FText& Text, int32 Size, FName Name);
	UComboBoxString* MakeCombo(UHorizontalBox* Parent, FName Name, const TArray<FString>& Options);
	USlider* MakeSlider(UHorizontalBox* Parent, FName Name, float Min, float Max, float Value);
	UCheckBox* MakeCheckBox(UHorizontalBox* Parent, FName Name, bool bChecked);
	void PopulateFromCurrentSettings();
	void ApplyCurrentSelections();

	UFUNCTION()
	void ShowGraphicsTab();

	UFUNCTION()
	void ShowAudioTab();

	UFUNCTION()
	void ShowControlsTab();

	UFUNCTION()
	void ApplySettings();

	UFUNCTION()
	void RestoreDefaults();

	UFUNCTION()
	void ShowCategorySelection();

	UFUNCTION()
	void UpdateMasterVolumeLabel(float Value);

	UFUNCTION()
	void UpdateSensitivityLabel(float Value);

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> SettingsSwitcher;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SettingsBackground;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> WindowModeCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> ResolutionCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> QualityCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> FrameRateCombo;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> VSyncCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MasterVolumeValue;

	UPROPERTY(Transient)
	TObjectPtr<USlider> SensitivitySlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SensitivityValue;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> InvertYCheckBox;

	bool bSettingsUIBuilt = false;
};
