#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CasinoSoundSubsystem.generated.h"

class UAudioComponent;
class UCasinoSoundTable;
class USoundMix;
class UWorld;

UCLASS(Config = Game)
class CASINO_SIMULATOR_API UCasinoSoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Applies the player's master volume (UCasinoSettingsSubsystem) to the engine's default Master sound class
	 * through a runtime SoundMix. FApp::SetVolumeMultiplier is not usable here: the engine resets it on window focus changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void ApplyMasterVolume();

	/** Sets the table at runtime. Overrides the DefaultGame.ini value. */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void SetSoundTable(UCasinoSoundTable* InTable);

	/** 2D sound for UI (clicks, popups). */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void PlayUI(FName Id);

	/** SFX. Plays in 3D when bUseLocation is true, otherwise in 2D. */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void PlaySFX(FName Id, bool bUseLocation = false, FVector Location = FVector::ZeroVector);

	/** Plays a looping-capable BGM. Crossfades from the current BGM. Ignored if the same ID is already playing. */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void PlayBGM(FName Id, float FadeTime = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Sound")
	void StopBGM(float FadeTime = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Sound")
	FName GetCurrentBGMId() const { return CurrentBGMId; }

private:
	/** Resolves the table (loads the soft reference on first use). */
	UCasinoSoundTable* GetSoundTable();

	/** Path of the sound table asset, e.g. in DefaultGame.ini:
	 *  [/Script/casino_simulator.CasinoSoundSubsystem]
	 *  SoundTablePath=/Game/6_Sound/DA_CasinoSoundTable.DA_CasinoSoundTable */
	UPROPERTY(Config)
	TSoftObjectPtr<UCasinoSoundTable> SoundTablePath;

	UPROPERTY(Transient)
	TObjectPtr<UCasinoSoundTable> SoundTable;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMComponent;

	/** Mix that carries the master volume override. Created at runtime, no asset needed. */
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> MasterVolumeMix;

	/** World the mix was last pushed to. Mix modifiers are per audio device, so a new world needs a new push. */
	TWeakObjectPtr<UWorld> MixPushedWorld;

	FDelegateHandle PostWorldInitHandle;

	FName CurrentBGMId;
};
