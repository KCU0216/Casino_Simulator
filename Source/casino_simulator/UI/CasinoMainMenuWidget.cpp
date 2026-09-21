#include "UI/CasinoMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "UI/CasinoSettingsSubsystem.h"

namespace CasinoSettingsUI
{
	const FLinearColor Gold(0.95f, 0.66f, 0.13f, 1.0f);
	const FLinearColor Ivory(0.95f, 0.93f, 0.86f, 1.0f);
	const FLinearColor Panel(0.035f, 0.03f, 0.025f, 0.96f);
}

void UCasinoMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bSettingsUIBuilt || !WidgetTree)
	{
		return;
	}

	if (UCanvasPanel* SettingsPanel = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("SettingsPanel"))))
	{
		BuildSettingsUI(SettingsPanel);
		bSettingsUIBuilt = true;
	}
}

void UCasinoMainMenuWidget::BuildSettingsUI(UCanvasPanel* SettingsPanel)
{
	SettingsBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NativeSettingsBackground"));
	SettingsBackground->SetBrushColor(CasinoSettingsUI::Panel);
	SettingsBackground->SetPadding(FMargin(32.0f, 24.0f));

	UCanvasPanelSlot* BackgroundSlot = SettingsPanel->AddChildToCanvas(SettingsBackground);
	BackgroundSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	BackgroundSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BackgroundSlot->SetPosition(FVector2D(0.0f, 70.0f));
	BackgroundSlot->SetSize(FVector2D(920.0f, 520.0f));
	BackgroundSlot->SetZOrder(0);

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsRoot"));
	SettingsBackground->SetContent(Root);

	UButton* GraphicsButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("GraphicButton")));
	UButton* AudioButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("AudioButton")));
	UButton* ControlsButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("KeySettingsButton")));
	if (!GraphicsButton || !AudioButton || !ControlsButton)
	{
		UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsTabs"));
		Root->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
		GraphicsButton = MakeTabButton(Tabs, FText::FromString(TEXT("그래픽")), TEXT("GraphicsTabButton"));
		AudioButton = MakeTabButton(Tabs, FText::FromString(TEXT("오디오")), TEXT("AudioTabButton"));
		ControlsButton = MakeTabButton(Tabs, FText::FromString(TEXT("조작")), TEXT("ControlsTabButton"));
	}
	GraphicsButton->OnClicked.AddDynamic(this, &UCasinoMainMenuWidget::ShowGraphicsTab);
	AudioButton->OnClicked.AddDynamic(this, &UCasinoMainMenuWidget::ShowAudioTab);
	ControlsButton->OnClicked.AddDynamic(this, &UCasinoMainMenuWidget::ShowControlsTab);

	SettingsSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("NativeSettingsSwitcher"));
	UVerticalBoxSlot* SwitcherSlot = Root->AddChildToVerticalBox(SettingsSwitcher);
	SwitcherSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox* GraphicsPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GraphicsSettingsPage"));
	SettingsSwitcher->AddChild(GraphicsPage);
	WindowModeCombo = MakeCombo(MakeSettingRow(GraphicsPage, FText::FromString(TEXT("화면 모드")), TEXT("WindowModeRow")), TEXT("WindowModeCombo"),
		{ TEXT("창 모드"), TEXT("테두리 없는 창"), TEXT("전체 화면") });
	ResolutionCombo = MakeCombo(MakeSettingRow(GraphicsPage, FText::FromString(TEXT("해상도")), TEXT("ResolutionRow")), TEXT("ResolutionCombo"),
		{ TEXT("1280 x 720"), TEXT("1600 x 900"), TEXT("1920 x 1080"), TEXT("2560 x 1440") });
	QualityCombo = MakeCombo(MakeSettingRow(GraphicsPage, FText::FromString(TEXT("그래픽 품질")), TEXT("QualityRow")), TEXT("QualityCombo"),
		{ TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("매우 높음") });
	FrameRateCombo = MakeCombo(MakeSettingRow(GraphicsPage, FText::FromString(TEXT("프레임 제한")), TEXT("FrameRateRow")), TEXT("FrameRateCombo"),
		{ TEXT("30 FPS"), TEXT("60 FPS"), TEXT("120 FPS"), TEXT("제한 없음") });
	VSyncCheckBox = MakeCheckBox(MakeSettingRow(GraphicsPage, FText::FromString(TEXT("수직 동기화")), TEXT("VSyncRow")), TEXT("VSyncCheckBox"), false);

	UVerticalBox* AudioPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AudioSettingsPage"));
	SettingsSwitcher->AddChild(AudioPage);
	UHorizontalBox* MasterRow = MakeSettingRow(AudioPage, FText::FromString(TEXT("전체 음량")), TEXT("MasterVolumeRow"));
	MasterVolumeSlider = MakeSlider(MasterRow, TEXT("MasterVolumeSlider"), 0.0f, 1.0f, 1.0f);
	MasterVolumeValue = MakeText(FText::FromString(TEXT("100%")), 18, TEXT("MasterVolumeValue"));
	MasterVolumeValue->SetMinDesiredWidth(72.0f);
	MasterVolumeValue->SetJustification(ETextJustify::Right);
	MasterRow->AddChildToHorizontalBox(MasterVolumeValue);
	MasterVolumeSlider->OnValueChanged.AddDynamic(this, &UCasinoMainMenuWidget::UpdateMasterVolumeLabel);

	UVerticalBox* ControlsPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ControlsSettingsPage"));
	SettingsSwitcher->AddChild(ControlsPage);
	UHorizontalBox* SensitivityRow = MakeSettingRow(ControlsPage, FText::FromString(TEXT("마우스 감도")), TEXT("SensitivityRow"));
	SensitivitySlider = MakeSlider(SensitivityRow, TEXT("SensitivitySlider"), 0.1f, 2.0f, 1.0f);
	SensitivityValue = MakeText(FText::FromString(TEXT("1.00")), 18, TEXT("SensitivityValue"));
	SensitivityValue->SetMinDesiredWidth(72.0f);
	SensitivityValue->SetJustification(ETextJustify::Right);
	SensitivityRow->AddChildToHorizontalBox(SensitivityValue);
	SensitivitySlider->OnValueChanged.AddDynamic(this, &UCasinoMainMenuWidget::UpdateSensitivityLabel);
	InvertYCheckBox = MakeCheckBox(MakeSettingRow(ControlsPage, FText::FromString(TEXT("Y축 반전")), TEXT("InvertYRow")), TEXT("InvertYCheckBox"), false);

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsFooter"));
	Root->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
	UButton* BackButton = MakeTabButton(Footer, FText::FromString(TEXT("뒤로")), TEXT("CategoryBackButton"));
	Footer->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* DefaultsButton = MakeTabButton(Footer, FText::FromString(TEXT("기본값")), TEXT("DefaultsButton"));
	UButton* ApplyButton = MakeTabButton(Footer, FText::FromString(TEXT("적용")), TEXT("ApplyButton"));
	BackButton->OnClicked.AddDynamic(this, &UCasinoMainMenuWidget::ShowCategorySelection);
	DefaultsButton->OnClicked.AddDynamic(this, &UCasinoMainMenuWidget::RestoreDefaults);
	ApplyButton->OnClicked.AddDynamic(this, &UCasinoMainMenuWidget::ApplySettings);

	PopulateFromCurrentSettings();
	ShowCategorySelection();
}

UButton* UCasinoMainMenuWidget::MakeTabButton(UHorizontalBox* Parent, const FText& Label, FName Name)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetBackgroundColor(FLinearColor(0.16f, 0.13f, 0.09f, 1.0f));
	UTextBlock* Text = MakeText(Label, 19, FName(*FString::Printf(TEXT("%sText"), *Name.ToString())));
	Text->SetJustification(ETextJustify::Center);
	Button->SetContent(Text);
	UHorizontalBoxSlot* ButtonSlot = Parent->AddChildToHorizontalBox(Button);
	ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ButtonSlot->SetPadding(FMargin(5.0f));
	return Button;
}

UHorizontalBox* UCasinoMainMenuWidget::MakeSettingRow(UVerticalBox* Parent, const FText& Label, FName Name)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name);
	Parent->AddChildToVerticalBox(Row)->SetPadding(FMargin(14.0f, 11.0f));

	UTextBlock* LabelText = MakeText(Label, 20, FName(*FString::Printf(TEXT("%sLabel"), *Name.ToString())));
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	return Row;
}

UTextBlock* UCasinoMainMenuWidget::MakeText(const FText& Text, int32 Size, FName Name)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TextBlock->SetText(Text);
	TextBlock->SetColorAndOpacity(CasinoSettingsUI::Ivory);
	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = Size;
	TextBlock->SetFont(Font);
	return TextBlock;
}

UComboBoxString* UCasinoMainMenuWidget::MakeCombo(UHorizontalBox* Parent, FName Name, const TArray<FString>& Options)
{
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(300.0f);
	UComboBoxString* Combo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), Name);
	for (const FString& Option : Options)
	{
		Combo->AddOption(Option);
	}
	SizeBox->SetContent(Combo);
	Parent->AddChildToHorizontalBox(SizeBox)->SetVerticalAlignment(VAlign_Center);
	return Combo;
}

USlider* UCasinoMainMenuWidget::MakeSlider(UHorizontalBox* Parent, FName Name, float Min, float Max, float Value)
{
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(228.0f);
	USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
	Slider->SetMinValue(Min);
	Slider->SetMaxValue(Max);
	Slider->SetValue(Value);
	Slider->SetSliderBarColor(FLinearColor(0.32f, 0.28f, 0.22f, 1.0f));
	Slider->SetSliderHandleColor(CasinoSettingsUI::Gold);
	SizeBox->SetContent(Slider);
	Parent->AddChildToHorizontalBox(SizeBox)->SetVerticalAlignment(VAlign_Center);
	return Slider;
}

UCheckBox* UCasinoMainMenuWidget::MakeCheckBox(UHorizontalBox* Parent, FName Name, bool bChecked)
{
	UCheckBox* CheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), Name);
	CheckBox->SetIsChecked(bChecked);
	Parent->AddChildToHorizontalBox(CheckBox)->SetVerticalAlignment(VAlign_Center);
	return CheckBox;
}

void UCasinoMainMenuWidget::PopulateFromCurrentSettings()
{
	UGameUserSettings* GameSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (GameSettings)
	{
		switch (GameSettings->GetFullscreenMode())
		{
		case EWindowMode::Fullscreen: WindowModeCombo->SetSelectedOption(TEXT("전체 화면")); break;
		case EWindowMode::WindowedFullscreen: WindowModeCombo->SetSelectedOption(TEXT("테두리 없는 창")); break;
		default: WindowModeCombo->SetSelectedOption(TEXT("창 모드")); break;
		}

		const FIntPoint Resolution = GameSettings->GetScreenResolution();
		const FString ResolutionText = FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
		if (ResolutionCombo->FindOptionIndex(ResolutionText) == INDEX_NONE)
		{
			ResolutionCombo->AddOption(ResolutionText);
		}
		ResolutionCombo->SetSelectedOption(ResolutionText);

		const TArray<FString> Qualities = { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("매우 높음") };
		const int32 CurrentQuality = GameSettings->GetOverallScalabilityLevel();
		QualityCombo->SetSelectedOption(Qualities[CurrentQuality == INDEX_NONE ? 2 : FMath::Clamp(CurrentQuality, 0, 3)]);
		VSyncCheckBox->SetIsChecked(GameSettings->IsVSyncEnabled());

		const float Limit = GameSettings->GetFrameRateLimit();
		FrameRateCombo->SetSelectedOption(Limit <= 0.0f ? TEXT("제한 없음") :
			Limit <= 45.0f ? TEXT("30 FPS") : Limit <= 90.0f ? TEXT("60 FPS") : TEXT("120 FPS"));
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UCasinoSettingsSubsystem* PlayerSettings = GameInstance->GetSubsystem<UCasinoSettingsSubsystem>())
		{
			MasterVolumeSlider->SetValue(PlayerSettings->GetMasterVolume());
			SensitivitySlider->SetValue(PlayerSettings->GetMouseSensitivity());
			InvertYCheckBox->SetIsChecked(PlayerSettings->IsMouseYInverted());
		}
	}

	UpdateMasterVolumeLabel(MasterVolumeSlider->GetValue());
	UpdateSensitivityLabel(SensitivitySlider->GetValue());
}

void UCasinoMainMenuWidget::ApplyCurrentSelections()
{
	if (UGameUserSettings* GameSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const FString Mode = WindowModeCombo->GetSelectedOption();
		GameSettings->SetFullscreenMode(Mode == TEXT("전체 화면") ? EWindowMode::Fullscreen :
			Mode == TEXT("테두리 없는 창") ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);

		FString WidthText;
		FString HeightText;
		if (ResolutionCombo->GetSelectedOption().Split(TEXT(" x "), &WidthText, &HeightText))
		{
			GameSettings->SetScreenResolution(FIntPoint(FCString::Atoi(*WidthText), FCString::Atoi(*HeightText)));
		}

		const int32 Quality = QualityCombo->FindOptionIndex(QualityCombo->GetSelectedOption());
		GameSettings->SetOverallScalabilityLevel(FMath::Clamp(Quality, 0, 3));
		GameSettings->SetVSyncEnabled(VSyncCheckBox->IsChecked());
		const int32 FrameRateIndex = FrameRateCombo->FindOptionIndex(FrameRateCombo->GetSelectedOption());
		const float FrameRates[] = { 30.0f, 60.0f, 120.0f, 0.0f };
		GameSettings->SetFrameRateLimit(FrameRates[FMath::Clamp(FrameRateIndex, 0, 3)]);
		GameSettings->ApplySettings(false);
		GameSettings->SaveSettings();
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UCasinoSettingsSubsystem* PlayerSettings = GameInstance->GetSubsystem<UCasinoSettingsSubsystem>())
		{
			PlayerSettings->SavePlayerSettings(MasterVolumeSlider->GetValue(), SensitivitySlider->GetValue(), InvertYCheckBox->IsChecked());
		}
	}
}

void UCasinoMainMenuWidget::ShowGraphicsTab()
{
	if (SettingsBackground) SettingsBackground->SetVisibility(ESlateVisibility::Visible);
	if (SettingsSwitcher) SettingsSwitcher->SetActiveWidgetIndex(0);
}

void UCasinoMainMenuWidget::ShowAudioTab()
{
	if (SettingsBackground) SettingsBackground->SetVisibility(ESlateVisibility::Visible);
	if (SettingsSwitcher) SettingsSwitcher->SetActiveWidgetIndex(1);
}

void UCasinoMainMenuWidget::ShowControlsTab()
{
	if (SettingsBackground) SettingsBackground->SetVisibility(ESlateVisibility::Visible);
	if (SettingsSwitcher) SettingsSwitcher->SetActiveWidgetIndex(2);
}

void UCasinoMainMenuWidget::ShowCategorySelection()
{
	if (SettingsBackground) SettingsBackground->SetVisibility(ESlateVisibility::Collapsed);
}

void UCasinoMainMenuWidget::ApplySettings()
{
	ApplyCurrentSelections();
}

void UCasinoMainMenuWidget::RestoreDefaults()
{
	WindowModeCombo->SetSelectedOption(TEXT("테두리 없는 창"));
	ResolutionCombo->SetSelectedOption(TEXT("1920 x 1080"));
	QualityCombo->SetSelectedOption(TEXT("높음"));
	FrameRateCombo->SetSelectedOption(TEXT("60 FPS"));
	VSyncCheckBox->SetIsChecked(false);
	MasterVolumeSlider->SetValue(1.0f);
	SensitivitySlider->SetValue(1.0f);
	InvertYCheckBox->SetIsChecked(false);
	UpdateMasterVolumeLabel(1.0f);
	UpdateSensitivityLabel(1.0f);
	ApplyCurrentSelections();
}

void UCasinoMainMenuWidget::UpdateMasterVolumeLabel(float Value)
{
	if (MasterVolumeValue)
	{
		MasterVolumeValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f))));
	}
}

void UCasinoMainMenuWidget::UpdateSensitivityLabel(float Value)
{
	if (SensitivityValue)
	{
		SensitivityValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	}
}
