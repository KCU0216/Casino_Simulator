// Copyright Epic Games, Inc. All Rights Reserved.

#include "casino_simulatorGameMode.h"
#include "casino_simulatorPlayerState.h"
#include "GameFramework/Pawn.h"
#include "casino_simulatorCharacter.h"
Acasino_simulatorGameMode::Acasino_simulatorGameMode()
{
	PlayerStateClass = Acasino_simulatorPlayerState::StaticClass();
}

void Acasino_simulatorGameMode::ArrestPlayer(
    APawn* TargetPlayer,
    AActor* PoliceActor,
    AActor* JailPoint)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!IsValid(TargetPlayer) || !IsValid(JailPoint))
    {
        return;
    }

    TargetPlayer->SetActorLocation(
        JailPoint->GetActorLocation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    if (IsValid(PoliceActor))
    {
        PoliceActor->Destroy();
    }
}

bool Acasino_simulatorGameMode::PayBail(
    APawn* Player,
    float BailAmount)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!IsValid(Player) || BailAmount < 0.0f)
    {
        return false;
    }

    Acasino_simulatorCharacter* CasinoPlayer =
        Cast<Acasino_simulatorCharacter>(Player);

    if (!CasinoPlayer)
    {
        return false;
    }

    return CasinoPlayer->TrySpendCurrency(BailAmount);
}