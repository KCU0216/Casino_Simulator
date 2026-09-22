#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CasinoSettingsSubsystem.generated.h"

UENUM(BlueprintType)
enum class ECasinoWindowMode : uint8
{
	Windowed UMETA(DisplayName = "창 모드"),
	Borderless UMETA(DisplayName = "테두리 없는 창"),
	Fullscreen UMETA(DisplayName = "전체 화면")
};

USTRUCT(BlueprintType)
struct FCasinoGraphicsSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Graphics")
	ECasinoWindowMode WindowMode = ECasinoWindowMode::Borderless;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Graphics")
	FIntPoint Resolution = FIntPoint(1920, 1080);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Graphics", meta = (ClampMin = "0", ClampMax = "3"))
	int32 QualityLevel = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Graphics", meta = (ClampMin = "0.0"))
	float FrameRateLimit = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Graphics")
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

	UFUNCTION(BlueprintPure, Category = "Settings|Graphics")
	FCasinoGraphicsSettings GetGraphicsSettings() const;

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void ApplyGraphicsSettings(const FCasinoGraphicsSettings& NewSettings);

	UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
	void ResetGraphicsSettings();

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
