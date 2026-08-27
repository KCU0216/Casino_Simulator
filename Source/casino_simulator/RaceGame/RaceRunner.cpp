// RaceRunner.cpp
#include "RaceRunner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

ARaceRunner::ARaceRunner()
{
	PrimaryActorTick.bCanEverTick = true;   // 서버·클라 각자 매 프레임 시뮬
	bReplicates = true;
	SetReplicateMovement(false);            // ★ 위치 복제 안 함 — 레시피로 각자 계산

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Skin = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Skin"));
	Skin->SetupAttachment(SceneRoot);
	Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARaceRunner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaceRunner, Stats);
	DOREPLIFETIME(ARaceRunner, RaceScript);
	DOREPLIFETIME(ARaceRunner, bRacing);
}

void ARaceRunner::InitStats(const FRaceRunnerStats& In)
{
	Stats = In;
	if (HasAuthority()) OnStatsUpdated();
}

void ARaceRunner::ServerSetupScript(const FRunnerRaceScript& S)
{
	RaceScript = S;
	ResetVisual();
	SetActorLocation(RaceScript.StartLoc);
}

void ARaceRunner::ServerSetRacing(bool bNew)
{
	bRacing = bNew;
	if (HasAuthority()) OnRep_Racing();   // 서버에서도 리셋 반영
}

void ARaceRunner::ResetVisual()
{
	LocalTime = 0.f;
	PosUnits = 0.f;
	bAwakenedLocal = false;
	bStumbledLocal = false;
	StumbleUntil = 0.f;
	bIsRunning = false;
}

void ARaceRunner::OnRep_Racing()
{
	// bRacing이 true로 바뀌는 순간 = 이 머신의 출발 신호
	if (bRacing)
	{
		ResetVisual();
		SetActorLocation(RaceScript.StartLoc);
		bIsRunning = true;
	}
}

void ARaceRunner::OnRep_Stats() { OnStatsUpdated(); }

void ARaceRunner::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bIsRunning) return;

	LocalTime += Dt;

	// 각성 (레시피에 이미 정해져 있음 → 서버·클라 동일 지점에서 발동)
	if (RaceScript.bWillAwaken && !bAwakenedLocal && PosUnits >= RaceScript.AwakenAtPos)
	{
		bAwakenedLocal = true;
		OnAwakenFX();
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

	PosUnits += RaceScript.Speed * Mult * Dt;
	if (PosUnits >= RaceScript.TrackLength) { PosUnits = RaceScript.TrackLength; bIsRunning = false; }

	SetActorLocation(RaceScript.StartLoc + RaceScript.Dir * PosUnits);
}
