#include "UI/CasinoSettingsSubsystem.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

namespace CasinoSettings
{
	const TCHAR* Section = TEXT("Casino.PlayerSettings");
}

void UCasinoSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadPlayerSettings();
	ApplyAudioSettings();
}

FCasinoGraphicsSettings UCasinoSettingsSubsystem::GetGraphicsSettings() const
{
	FCasinoGraphicsSettings Result;
	const UGameUserSettings* GameSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GameSettings)
	{
		return Result;
	}

	switch (GameSettings->GetFullscreenMode())
	{
	case EWindowMode::Fullscreen:
		Result.WindowMode = ECasinoWindowMode::Fullscreen;
		break;
	case EWindowMode::WindowedFullscreen:
		Result.WindowMode = ECasinoWindowMode::Borderless;
		break;
	default:
		Result.WindowMode = ECasinoWindowMode::Windowed;
		break;
	}

	Result.Resolution = GameSettings->GetScreenResolution();
	const int32 QualityLevel = GameSettings->GetOverallScalabilityLevel();
	Result.QualityLevel = QualityLevel == INDEX_NONE ? 2 : FMath::Clamp(QualityLevel, 0, 3);
	Result.FrameRateLimit = GameSettings->GetFrameRateLimit();
	Result.bVSyncEnabled = GameSettings->IsVSyncEnabled();
	return Result;
}

void UCasinoSettingsSubsystem::ApplyGraphicsSettings(const FCasinoGraphicsSettings& NewSettings)
{
	UGameUserSettings* GameSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GameSettings)
	{
		return;
	}

	switch (NewSettings.WindowMode)
	{
	case ECasinoWindowMode::Fullscreen:
		GameSettings->SetFullscreenMode(EWindowMode::Fullscreen);
		break;
	case ECasinoWindowMode::Borderless:
		GameSettings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
		break;
	default:
		GameSettings->SetFullscreenMode(EWindowMode::Windowed);
		break;
	}

	const FIntPoint SafeResolution(
		FMath::Max(NewSettings.Resolution.X, 640),
		FMath::Max(NewSettings.Resolution.Y, 480));
	GameSettings->SetScreenResolution(SafeResolution);
	GameSettings->SetOverallScalabilityLevel(FMath::Clamp(NewSettings.QualityLevel, 0, 3));
	GameSettings->SetFrameRateLimit(FMath::Max(NewSettings.FrameRateLimit, 0.0f));
	GameSettings->SetVSyncEnabled(NewSettings.bVSyncEnabled);
	GameSettings->ApplySettings(false);
	GameSettings->SaveSettings();
}

void UCasinoSettingsSubsystem::ResetGraphicsSettings()
{
	FCasinoGraphicsSettings Defaults;
	ApplyGraphicsSettings(Defaults);
}

FCasinoPlayerSettings UCasinoSettingsSubsystem::GetPlayerSettings() const
{
	FCasinoPlayerSettings Result;
	Result.MasterVolume = MasterVolume;
	Result.MouseSensitivity = MouseSensitivity;
	Result.bInvertMouseY = bInvertMouseY;
	return Result;
}

void UCasinoSettingsSubsystem::LoadPlayerSettings()
{
	GConfig->GetFloat(CasinoSettings::Section, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(CasinoSettings::Section, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->GetBool(CasinoSettings::Section, TEXT("InvertMouseY"), bInvertMouseY, GGameUserSettingsIni);

	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 2.0f);
}

void UCasinoSettingsSubsystem::SavePlayerSettings(float InMasterVolume, float InMouseSensitivity, bool bInInvertMouseY)
{
	MasterVolume = FMath::Clamp(InMasterVolume, 0.0f, 1.0f);
	MouseSensitivity = FMath::Clamp(InMouseSensitivity, 0.1f, 2.0f);
	bInvertMouseY = bInInvertMouseY;

	GConfig->SetFloat(CasinoSettings::Section, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(CasinoSettings::Section, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->SetBool(CasinoSettings::Section, TEXT("InvertMouseY"), bInvertMouseY, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	ApplyAudioSettings();
}

void UCasinoSettingsSubsystem::ResetPlayerSettings()
{
	SavePlayerSettings(1.0f, 1.0f, false);
}

void UCasinoSettingsSubsystem::ApplyAudioSettings() const
{
	FApp::SetVolumeMultiplier(MasterVolume);
}
