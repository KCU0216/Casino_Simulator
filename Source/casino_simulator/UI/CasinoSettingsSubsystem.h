#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CasinoSettingsSubsystem.generated.h"

UCLASS()
class CASINO_SIMULATOR_API UCasinoSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	float GetMouseSensitivity() const { return MouseSensitivity; }

	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	bool IsMouseYInverted() const { return bInvertMouseY; }

	void SavePlayerSettings(float InMasterVolume, float InMouseSensitivity, bool bInInvertMouseY);
	void ResetPlayerSettings();

private:
	void LoadPlayerSettings();
	void ApplyAudioSettings() const;

	float MasterVolume = 1.0f;
	float MouseSensitivity = 1.0f;
	bool bInvertMouseY = false;
};
