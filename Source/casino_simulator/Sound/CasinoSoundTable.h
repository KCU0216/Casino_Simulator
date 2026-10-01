#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CasinoSoundTable.generated.h"

class USoundBase;
class USoundAttenuation;

USTRUCT(BlueprintType)
struct FCasinoSoundEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.0f;

	/** Only used by PlaySFX at a location. Null = Sound's own attenuation settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundAttenuation> Attenuation = nullptr;
};

/** Maps a sound ID (FName) to a sound asset. Used by UCasinoSoundSubsystem. */
UCLASS(BlueprintType)
class CASINO_SIMULATOR_API UCasinoSoundTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TMap<FName, FCasinoSoundEntry> Sounds;

	const FCasinoSoundEntry* FindEntry(FName Id) const { return Sounds.Find(Id); }
};
