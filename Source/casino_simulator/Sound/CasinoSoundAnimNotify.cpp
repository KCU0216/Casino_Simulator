#include "Sound/CasinoSoundAnimNotify.h"

#include "Sound/CasinoSoundSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UCasinoSoundAnimNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || SoundId.IsNone())
	{
		return;
	}

	const UWorld* World = MeshComp->GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// No game instance in editor preview worlds.
	UGameInstance* GameInstance = World->GetGameInstance();
	if (UCasinoSoundSubsystem* Sound = GameInstance ? GameInstance->GetSubsystem<UCasinoSoundSubsystem>() : nullptr)
	{
		Sound->PlaySFX(SoundId, bUseLocation, MeshComp->GetComponentLocation());
	}
}

FString UCasinoSoundAnimNotify::GetNotifyName_Implementation() const
{
	return SoundId.IsNone() ? TEXT("Casino Play Sound") : FString::Printf(TEXT("Sound: %s"), *SoundId.ToString());
}
