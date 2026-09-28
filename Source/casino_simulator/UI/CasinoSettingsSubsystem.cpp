#include "UI/CasinoSettingsSubsystem.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

namespace CasinoSettings
{
	const TCHAR* Section = TEXT("Casino.PlayerSettings");
	constexpr float FrameRateOptions[] = { 30.0f, 60.0f, 120.0f, 0.0f };

	TArray<FIntPoint> BuildResolutionOptions(const FIntPoint& CurrentResolution)
	{
		TArray<FIntPoint> SupportedResolutions;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(SupportedResolutions);

		TArray<FIntPoint> Result;
		for (const FIntPoint& Resolution : SupportedResolutions)
		{
			if (Resolution.X > 0 && Resolution.Y > 0)
			{
				Result.AddUnique(Resolution);
			}
		}

		if (Result.IsEmpty())
		{
			Result = {
				FIntPoint(1280, 720),
				FIntPoint(1600, 900),
				FIntPoint(1920, 1080),
				FIntPoint(2560, 1440)
			};
		}

		Result.AddUnique(CurrentResolution);
		Result.Sort([](const FIntPoint& Left, const FIntPoint& Right)
		{
			return Left.X == Right.X ? Left.Y < Right.Y : Left.X < Right.X;
		});
		return Result;
	}

	int32 WindowModeToIndex(EWindowMode::Type WindowMode)
	{
		switch (WindowMode)
		{
		case EWindowMode::Windowed:
			return 0;
		case EWindowMode::WindowedFullscreen:
			return 1;
		case EWindowMode::Fullscreen:
			return 2;
		default:
			return 1;
		}
	}

	int32 FrameRateToIndex(float FrameRateLimit)
	{
		if (FrameRateLimit <= 0.0f)
		{
			return 3;
		}
		if (FrameRateLimit <= 45.0f)
		{
			return 0;
		}
		if (FrameRateLimit <= 90.0f)
		{
			return 1;
		}
		return 2;
	}
}

void UCasinoSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadPlayerSettings();
	ApplyAudioSettings();
}

void UCasinoSettingsSubsystem::ResetGraphicsSettings()
{
	UGameUserSettings* GameSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GameSettings)
	{
		return;
	}

	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	const FIntPoint DesktopResolution(
		FMath::Max(DisplayMetrics.PrimaryDisplayWidth, 640),
		FMath::Max(DisplayMetrics.PrimaryDisplayHeight, 480));
	GameSettings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
	GameSettings->SetScreenResolution(DesktopResolution);
	GameSettings->SetOverallScalabilityLevel(2);
	GameSettings->SetFrameRateLimit(60.0f);
	GameSettings->SetVSyncEnabled(false);
	GameSettings->ApplySettings(false);
	GameSettings->SaveSettings();
}

FCasinoGraphicsMenuSettings UCasinoSettingsSubsystem::GetGraphicsMenuSettings() const
{
	FCasinoGraphicsMenuSettings Result;
	const UGameUserSettings* GameSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GameSettings)
	{
		return Result;
	}

	const FIntPoint CurrentResolution = GameSettings->GetScreenResolution();
	const TArray<FIntPoint> ResolutionValues = CasinoSettings::BuildResolutionOptions(CurrentResolution);
	Result.ResolutionOptions.Reserve(ResolutionValues.Num());
	for (const FIntPoint& Resolution : ResolutionValues)
	{
		Result.ResolutionOptions.Add(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
	}

	Result.WindowModeIndex = CasinoSettings::WindowModeToIndex(GameSettings->GetFullscreenMode());
	Result.ResolutionIndex = ResolutionValues.IndexOfByKey(CurrentResolution);
	const int32 QualityLevel = GameSettings->GetOverallScalabilityLevel();
	Result.QualityIndex = QualityLevel == INDEX_NONE ? 2 : FMath::Clamp(QualityLevel, 0, 3);
	Result.FrameRateIndex = CasinoSettings::FrameRateToIndex(GameSettings->GetFrameRateLimit());
	Result.bVSyncEnabled = GameSettings->IsVSyncEnabled();
	return Result;
}

void UCasinoSettingsSubsystem::ApplyGraphicsMenuSettings(
	int32 WindowModeIndex,
	int32 ResolutionIndex,
	int32 QualityIndex,
	int32 FrameRateIndex,
	bool bVSyncEnabled)
{
	UGameUserSettings* GameSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GameSettings)
	{
		return;
	}

	switch (WindowModeIndex)
	{
	case 0:
		GameSettings->SetFullscreenMode(EWindowMode::Windowed);
		break;
	case 1:
		GameSettings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
		break;
	case 2:
		GameSettings->SetFullscreenMode(EWindowMode::Fullscreen);
		break;
	default:
		break;
	}

	const TArray<FIntPoint> ResolutionValues =
		CasinoSettings::BuildResolutionOptions(GameSettings->GetScreenResolution());
	if (ResolutionValues.IsValidIndex(ResolutionIndex))
	{
		GameSettings->SetScreenResolution(ResolutionValues[ResolutionIndex]);
	}

	if (QualityIndex >= 0)
	{
		GameSettings->SetOverallScalabilityLevel(FMath::Clamp(QualityIndex, 0, 3));
	}

	if (FrameRateIndex >= 0 && FrameRateIndex < UE_ARRAY_COUNT(CasinoSettings::FrameRateOptions))
	{
		GameSettings->SetFrameRateLimit(CasinoSettings::FrameRateOptions[FrameRateIndex]);
	}

	GameSettings->SetVSyncEnabled(bVSyncEnabled);
	GameSettings->ApplySettings(false);
	GameSettings->SaveSettings();
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
