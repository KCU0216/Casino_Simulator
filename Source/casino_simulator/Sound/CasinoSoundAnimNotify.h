#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CasinoSoundAnimNotify.generated.h"

/** Plays a sound from the casino sound table when the animation reaches this notify. Runs wherever the animation plays. */
UCLASS(meta = (DisplayName = "Casino Play Sound"))
class CASINO_SIMULATOR_API UCasinoSoundAnimNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** ID registered in the casino sound table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	FName SoundId;

	/** true = 3D sound at the mesh's location, false = 2D. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	bool bUseLocation = true;
};
