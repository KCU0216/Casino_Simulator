#include "Sound/CasinoSoundSubsystem.h"

#include "Sound/CasinoSoundTable.h"
#include "UI/CasinoSettingsSubsystem.h"
#include "Components/AudioComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

void UCasinoSoundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UCasinoSettingsSubsystem>();
	Super::Initialize(Collection);

	// The audio device of a new world is not ready inside OnPostWorldInitialization, so apply one tick later.
	const auto ApplyNextTick = [WeakThis = TWeakObjectPtr<UCasinoSoundSubsystem>(this)]()
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (UCasinoSoundSubsystem* Self = WeakThis.Get())
			{
				Self->ApplyMasterVolume();
			}
			return false;
		}));
	};

	PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddWeakLambda(this,
		[this, ApplyNextTick](UWorld* World, const UWorld::InitializationValues)
		{
			if (World && World->GetGameInstance() == GetGameInstance())
			{
				ApplyNextTick();
			}
		});

	ApplyNextTick();
}

void UCasinoSoundSubsystem::ApplyMasterVolume()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UCasinoSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UCasinoSettingsSubsystem>();
	USoundClass* MasterClass = Cast<USoundClass>(GetDefault<UAudioSettings>()->DefaultSoundClassName.TryLoad());
	if (!Settings || !MasterClass)
	{
		return;
	}

	if (!MasterVolumeMix)
	{
		MasterVolumeMix = NewObject<USoundMix>(this);
	}

	if (MixPushedWorld.Get() != World)
	{
		UGameplayStatics::PushSoundMixModifier(World, MasterVolumeMix);
		MixPushedWorld = World;
	}

	UGameplayStatics::SetSoundMixClassOverride(
		World, MasterVolumeMix, MasterClass, Settings->GetMasterVolume(), 1.0f, 0.0f, /*bApplyToChildren=*/true);
}

void UCasinoSoundSubsystem::Deinitialize()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
	PostWorldInitHandle.Reset();

	if (BGMComponent)
	{
		BGMComponent->Stop();
		BGMComponent = nullptr;
	}
	CurrentBGMId = NAME_None;
	SoundTable = nullptr;

	Super::Deinitialize();
}

void UCasinoSoundSubsystem::SetSoundTable(UCasinoSoundTable* InTable)
{
	SoundTable = InTable;
}

UCasinoSoundTable* UCasinoSoundSubsystem::GetSoundTable()
{
	if (!SoundTable && !SoundTablePath.IsNull())
	{
		SoundTable = SoundTablePath.LoadSynchronous();
	}
	return SoundTable;
}

void UCasinoSoundSubsystem::PlayUI(FName Id)
{
	PlaySFX(Id, false);
}

void UCasinoSoundSubsystem::PlaySFX(FName Id, bool bUseLocation, FVector Location)
{
	UCasinoSoundTable* Table = GetSoundTable();
	const FCasinoSoundEntry* Entry = Table ? Table->FindEntry(Id) : nullptr;
	if (!Entry || !Entry->Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("CasinoSound: no sound registered for '%s'"), *Id.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bUseLocation)
	{
		UGameplayStatics::PlaySoundAtLocation(
			World, Entry->Sound, Location, FRotator::ZeroRotator,
			Entry->VolumeMultiplier, Entry->PitchMultiplier, 0.0f, Entry->Attenuation);
	}
	else
	{
		UGameplayStatics::PlaySound2D(World, Entry->Sound, Entry->VolumeMultiplier, Entry->PitchMultiplier);
	}
}

void UCasinoSoundSubsystem::PlayBGM(FName Id, float FadeTime)
{
	if (Id == CurrentBGMId && BGMComponent && BGMComponent->IsPlaying())
	{
		return;
	}

	UCasinoSoundTable* Table = GetSoundTable();
	const FCasinoSoundEntry* Entry = Table ? Table->FindEntry(Id) : nullptr;
	if (!Entry || !Entry->Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("CasinoSound: no BGM registered for '%s'"), *Id.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Old BGM fades out and destroys itself (bAutoDestroy) while the new one fades in.
	if (BGMComponent)
	{
		BGMComponent->FadeOut(FMath::Max(FadeTime, 0.0f), 0.0f);
		BGMComponent = nullptr;
	}

	BGMComponent = UGameplayStatics::CreateSound2D(
		World, Entry->Sound, Entry->VolumeMultiplier, Entry->PitchMultiplier,
		0.0f, nullptr, /*bPersistAcrossLevelTransition=*/true, /*bAutoDestroy=*/true);
	if (BGMComponent)
	{
		BGMComponent->FadeIn(FMath::Max(FadeTime, 0.0f), 1.0f);
		CurrentBGMId = Id;
	}
	else
	{
		CurrentBGMId = NAME_None;
	}
}

void UCasinoSoundSubsystem::StopBGM(float FadeTime)
{
	if (BGMComponent)
	{
		BGMComponent->FadeOut(FMath::Max(FadeTime, 0.0f), 0.0f);
		BGMComponent = nullptr;
	}
	CurrentBGMId = NAME_None;
}
