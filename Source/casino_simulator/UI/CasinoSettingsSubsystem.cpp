#include "UI/CasinoSettingsSubsystem.h"

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
