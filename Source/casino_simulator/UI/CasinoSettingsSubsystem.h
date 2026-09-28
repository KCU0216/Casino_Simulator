#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CasinoSettingsSubsystem.generated.h"

/** UI-facing values for ComboBox (String) widgets. */
USTRUCT(BlueprintType)
struct FCasinoGraphicsMenuSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics|Menu")
	TArray<FString> ResolutionOptions;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics|Menu")
	int32 WindowModeIndex = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics|Menu")
	int32 ResolutionIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics|Menu")
	int32 QualityIndex = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics|Menu")
	int32 FrameRateIndex = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Graphics|Menu")
	bool bVSyncEnabled = false;
};

USTRUCT(BlueprintType)
struct FCasinoPlayerSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Controls", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MouseSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Controls")
	bool bInvertMouseY = false;
};

UCLASS()
class CASINO_SIMULATOR_API UCasinoSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void ResetGraphicsSettings();

	UFUNCTION(BlueprintPure, Category = "Settings|Graphics|Menu")
	FCasinoGraphicsMenuSettings GetGraphicsMenuSettings() const;

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics|Menu")
	void ApplyGraphicsMenuSettings(
		int32 WindowModeIndex,
		int32 ResolutionIndex,
		int32 QualityIndex,
		int32 FrameRateIndex,
		bool bVSyncEnabled);

	UFUNCTION(BlueprintPure, Category = "Settings|Player")
	FCasinoPlayerSettings GetPlayerSettings() const;

	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	float GetMouseSensitivity() const { return MouseSensitivity; }

	UFUNCTION(BlueprintPure, Category = "Settings|Controls")
	bool IsMouseYInverted() const { return bInvertMouseY; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Player")
	void SavePlayerSettings(float InMasterVolume, float InMouseSensitivity, bool bInInvertMouseY);

	UFUNCTION(BlueprintCallable, Category = "Settings|Player")
	void ResetPlayerSettings();

private:
	void LoadPlayerSettings();
	void ApplyAudioSettings() const;

	float MasterVolume = 1.0f;
	float MouseSensitivity = 1.0f;
	bool bInvertMouseY = false;
};
