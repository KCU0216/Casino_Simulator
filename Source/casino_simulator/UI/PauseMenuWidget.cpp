#include "PauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"

void UPauseMenuWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyInGameSettingsAppearance();
}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyInGameSettingsAppearance();
}

void UPauseMenuWidget::ApplyInGameSettingsAppearance()
{
	UUserWidget* Settings = Cast<UUserWidget>(GetWidgetFromName(TEXT("WBP_Settings")));
	if (!Settings || !Settings->WidgetTree)
	{
		return;
	}

	// Change only this menu's live child, leaving the shared settings asset opaque.
	const auto SetBackgroundOpacity = [Settings](const TCHAR* Name, float Opacity)
	{
		if (UBorder* Border = Cast<UBorder>(Settings->WidgetTree->FindWidget(Name)))
		{
			FLinearColor Color = Border->GetBrushColor();
			Color.A = Opacity;
			Border->SetBrushColor(Color);
		}
	};

	SetBackgroundOpacity(TEXT("CasinoThemeBackground"), 0.0f);
	for (const TCHAR* Name : {
		TEXT("CasinoSettingsOverviewFrame"),
		TEXT("CasinoSettingsFrame_GraphicPanel"),
		TEXT("CasinoSettingsFrame_AudioPanel"),
		TEXT("CasinoSettingsFrame_KeySettingsPanel")})
	{
		SetBackgroundOpacity(Name, 0.78f);
	}
}
