// RaceRunner.cpp
#include "RaceRunner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "RaceManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

ARaceRunner::ARaceRunner()
{
	PrimaryActorTick.bCanEverTick = true;   // 서버·클라 각자 매 프레임 시뮬
	bReplicates = true;
	SetReplicateMovement(false);            // ★ 위치 복제 안 함 — 레시피로 각자 계산

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Skin = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Skin"));
	Skin->SetupAttachment(SceneRoot);
	FootstepAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FootstepAudio"));
	FootstepAudio->SetupAttachment(SceneRoot);
	FootstepAudio->bAutoActivate = false;
	FootstepAudio->bStopWhenOwnerDestroyed = true;
	FootstepAudio->bOverrideAttenuation = true;
	FootstepAudio->AttenuationOverrides.bAttenuate = true;
	FootstepAudio->AttenuationOverrides.bSpatialize = true;
	FootstepAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(150.f, 0.f, 0.f);
	FootstepAudio->AttenuationOverrides.FalloffDistance = 1200.f;
	FootstepAudio->SetVolumeMultiplier(.45f);
	static ConstructorHelpers::FObjectFinder<USoundBase> Footsteps(
		TEXT("/Game/6_Sound/RaceFootsteps.RaceFootsteps"));
	if (Footsteps.Succeeded()) FootstepAudio->SetSound(Footsteps.Object);
    Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BurstAsset(
        TEXT("/Game/KCU/HorseRacing/FX/NS_RunnerAwakenBurst.NS_RunnerAwakenBurst"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowAsset(
        TEXT("/Game/KCU/HorseRacing/FX/M_RunnerAwakenGlow.M_RunnerAwakenGlow"));
    AwakenBurstTemplate = BurstAsset.Object;
    AwakenGlowMaterial = GlowAsset.Object;
}

void ARaceRunner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaceRunner, Stats);
	DOREPLIFETIME(ARaceRunner, RunnerPortrait);
    DOREPLIFETIME(ARaceRunner, RunnerAwakenImage);
	DOREPLIFETIME(ARaceRunner, RaceScript);
	DOREPLIFETIME(ARaceRunner, bRacing);
	DOREPLIFETIME(ARaceRunner, bIsEntering);
	DOREPLIFETIME(ARaceRunner, bIsExiting);
}

void ARaceRunner::InitStats(const FRaceRunnerStats& In)
{
	Stats = In;
	if (HasAuthority()) OnStatsUpdated();
}

UTexture2D* ARaceRunner::GetRunnerPortrait() const
{
	return RunnerPortrait.Get();
}

void ARaceRunner::SetRunnerName(const FString& NewName)
{
	if (!HasAuthority()) return;

	Stats.Name = NewName;
	ForceNetUpdate();
}

void ARaceRunner::ServerSetupScript(const FRunnerRaceScript& S)
{
	if (!HasAuthority()) return;
	RaceScript = S;
	OnRep_RaceScript();
	ForceNetUpdate();
}

void ARaceRunner::OnRep_RaceScript()
{
	if (!bHasRaceScript)
	{
		SetActorLocation(GetServerTime() >= RaceScript.EnterStartServerTime + RaceScript.EnterDuration
			? RaceScript.StartLoc : RaceScript.SpawnLoc);
	}
	bHasRaceScript = true;
	SetActorRotation(RaceScript.Dir.Rotation());
	ApplyMovementState();
}

void ARaceRunner::ServerSetEntering(bool bNew)
{
	if (!HasAuthority() || (bNew && (bRacing || bIsExiting))) return;
	bIsEntering = bNew;
	OnRep_Entering();
	ForceNetUpdate();
}

void ARaceRunner::ServerSetRacing(bool bNew, double StartServerTime)
{
	if (!HasAuthority() || (bNew && (bIsEntering || bIsExiting))) return;
	if (bNew)
	{
		RaceScript.RaceStartServerTime = StartServerTime >= 0.0 ? StartServerTime : GetServerTime();
	}
	bRacing = bNew;
	OnRep_Racing();
	ForceNetUpdate();
}

void ARaceRunner::ServerSetExiting(bool bNew, double StartServerTime)
{
	if (!HasAuthority() || (bNew && (bIsEntering || bRacing))) return;
	if (bNew)
	{
		RaceScript.ExitStartServerTime = StartServerTime >= 0.0 ? StartServerTime : GetServerTime();
	}
	bIsExiting = bNew;
	OnRep_Exiting();
	ForceNetUpdate();
}

void ARaceRunner::ResetVisual()
{
	FootstepAudio->Stop();
	ClearAwakenEffect();
	LocalTime = 0.f;
	PosUnits = 0.f;
	bAwakenedLocal = false;
	bStumbledLocal = false;
	StumbleUntil = 0.f;
	bIsRunning = false;
}

void ARaceRunner::OnRep_Racing()
{
	ApplyMovementState();
}

void ARaceRunner::OnRep_Entering()
{
	ApplyMovementState();
}

void ARaceRunner::OnRep_Exiting()
{
	ApplyMovementState();
}

void ARaceRunner::ApplyMovementState()
{
	// Either the script or the replicated start flag may arrive first.
	if (!bHasRaceScript) return;
	if ((bRacing && RaceScript.RaceStartServerTime < 0.0)
		|| (bIsExiting && RaceScript.ExitStartServerTime < 0.0)) return;

	const ERacePhase NewPhase = bIsExiting ? ERacePhase::Exiting
		: bRacing ? ERacePhase::Racing
		: bIsEntering ? ERacePhase::Entering : ERacePhase::Idle;
	const ERacePhase PreviousPhase = LocalMovementPhase;
	if (NewPhase != LocalMovementPhase)
	{
		ResetVisual();
		LocalMovementPhase = NewPhase;
		bIsRunning = NewPhase == ERacePhase::Entering || NewPhase == ERacePhase::Racing
			|| NewPhase == ERacePhase::Exiting;
	}

	if (NewPhase == ERacePhase::Idle)
	{
		if (PreviousPhase == ERacePhase::Entering) SetActorLocation(RaceScript.StartLoc);
		else if (PreviousPhase == ERacePhase::Racing) SetActorLocation(RaceScript.FinishLoc);
		else if (PreviousPhase == ERacePhase::Exiting) SetActorLocation(RaceScript.ExitLoc);
	}
	else if (NewPhase == ERacePhase::Entering)
	{
		TickEntering();
	}
	else if (NewPhase == ERacePhase::Exiting)
	{
		TickExiting();
	}
	else if (NewPhase != PreviousPhase)
	{
		SetActorLocation(RaceScript.StartLoc);
	}
	UpdateFootstepAudio();
}

void ARaceRunner::UpdateFootstepAudio()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const bool bShouldPlay = bIsRunning && LocalMovementPhase == ERacePhase::Racing;
	if (!bShouldPlay)
	{
		FootstepAudio->Stop();
		return;
	}
	const float SpeedScale = FMath::Clamp(RaceScript.Speed / 200.f, .75f, 1.4f);
	const float MovementScale = LocalTime < StumbleUntil ? .65f : (bAwakenedLocal ? 1.35f : 1.f);
	FootstepAudio->SetPitchMultiplier(SpeedScale * MovementScale);
	if (!FootstepAudio->IsPlaying()) FootstepAudio->Play();
}

void ARaceRunner::OnRep_Stats() { OnStatsUpdated(); }

void ARaceRunner::Tick(float Dt)
{
	Super::Tick(Dt);
    TickAwakenEffect(Dt);
	if (!bHasRaceScript) return;
	if (LocalMovementPhase == ERacePhase::Entering && bIsEntering) TickEntering();
	else if (LocalMovementPhase == ERacePhase::Racing && bRacing) TickRacing();
	else if (LocalMovementPhase == ERacePhase::Exiting && bIsExiting) TickExiting();
	UpdateFootstepAudio();
}

double ARaceRunner::GetServerTime() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void ARaceRunner::TickTransit(const FVector& From, const FVector& To, double StartServerTime, float Duration)
{
	const float Alpha = Duration > 0.f ? FMath::Clamp(static_cast<float>(
		(GetServerTime() - StartServerTime) / Duration), 0.f, 1.f) : 1.f;
	SetActorLocation(FMath::Lerp(From, To, Alpha));
	bIsRunning = Alpha < 1.f;
}

void ARaceRunner::TickEntering()
{
	TickTransit(RaceScript.SpawnLoc, RaceScript.StartLoc,
		RaceScript.EnterStartServerTime, RaceScript.EnterDuration);
}

void ARaceRunner::TickExiting()
{
	if (RaceScript.ExitStartServerTime < 0.0) return;
	TickTransit(RaceScript.FinishLoc, RaceScript.ExitLoc,
		RaceScript.ExitStartServerTime, RaceScript.ExitDuration);
}

void ARaceRunner::TickRacing()
{
	if (!bIsRunning || RaceScript.RaceStartServerTime < 0.0) return;
	const float Elapsed = FMath::Max(0.f, static_cast<float>(GetServerTime() - RaceScript.RaceStartServerTime));
	constexpr float Step = 1.f / 120.f;
    bool bPlayAwakenEffect = false;
	// Match the manager's fixed-step finish simulation, including late replication.
	while (LocalTime + Step <= Elapsed && PosUnits < RaceScript.TrackLength)
	{
		// 각성 (레시피에 이미 정해져 있음 → 서버·클라 동일 지점에서 발동)
		if (RaceScript.bWillAwaken && !bAwakenedLocal && PosUnits >= RaceScript.AwakenAtPos)
		{
			bAwakenedLocal = true;
            if (HasAuthority())
                if (auto* Manager = Cast<ARaceManager>(GetOwner())) Manager->NotifyRunnerAwakened(this);
            // Don't replay old visual bursts when catching up after late replication.
            bPlayAwakenEffect = Elapsed - LocalTime < 0.5f;
		}
		// 장애물 (각성 안 했을 때만)
		if (!bAwakenedLocal && RaceScript.bWillStumble && !bStumbledLocal && PosUnits >= RaceScript.StumbleAtPos)
		{
			bStumbledLocal = true;
			StumbleUntil = LocalTime + 0.7f;
			OnStumbleFX();
		}

		float Mult = bAwakenedLocal ? 2.3f : 1.f;
		if (LocalTime < StumbleUntil) Mult *= 0.3f;

		PosUnits += RaceScript.Speed * Mult * Step;
		LocalTime += Step;
	}
	if (PosUnits >= RaceScript.TrackLength)
	{
		PosUnits = RaceScript.TrackLength;
		bIsRunning = false;
	}

	SetActorLocation(RaceScript.StartLoc + RaceScript.Dir * PosUnits);
    if (bPlayAwakenEffect)
    {
        PlayAwakenEffect();
        OnAwakenFX();
    }
}

void ARaceRunner::PlayAwakenEffect()
{
    if (GetNetMode() == NM_DedicatedServer) return;
    ClearAwakenEffect();
    AwakenEffectRemaining = 2.f;
    if (AwakenBurstTemplate)
    {
        AwakenBurst = UNiagaraFunctionLibrary::SpawnSystemAttached(AwakenBurstTemplate,
            SceneRoot, NAME_None, FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator,
            FVector(0.6f), EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None, true, false);
    }
    if (Skin && AwakenGlowMaterial)
    {
        PreviousOverlayMaterial = Skin->GetOverlayMaterial();
        AwakenGlow = UMaterialInstanceDynamic::Create(AwakenGlowMaterial, this);
        Skin->SetOverlayMaterial(AwakenGlow);
    }
}

void ARaceRunner::TickAwakenEffect(float DeltaSeconds)
{
    if (AwakenEffectRemaining <= 0.f) return;
    AwakenEffectRemaining = FMath::Max(0.f, AwakenEffectRemaining - DeltaSeconds);
    if (AwakenGlow)
    {
        const float Fade = FMath::Clamp(AwakenEffectRemaining / 0.5f, 0.f, 1.f);
        const float Pulse = 0.25f + 0.1f * FMath::Sin(AwakenEffectRemaining * 18.f);
        AwakenGlow->SetScalarParameterValue(TEXT("GlowStrength"), Fade * Pulse);
    }
    if (AwakenEffectRemaining <= 0.f) ClearAwakenEffect();
}

void ARaceRunner::ClearAwakenEffect()
{
    if (Skin && AwakenGlow && Skin->GetOverlayMaterial() == AwakenGlow)
        Skin->SetOverlayMaterial(PreviousOverlayMaterial);
    if (IsValid(AwakenBurst)) AwakenBurst->DestroyComponent();
    AwakenBurst = nullptr;
    AwakenGlow = nullptr;
    PreviousOverlayMaterial = nullptr;
    AwakenEffectRemaining = 0.f;
}
