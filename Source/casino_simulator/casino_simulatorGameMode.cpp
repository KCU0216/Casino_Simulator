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

void Acasino_simulatorGameMode::StealMoney(APawn* TargetPlayer, AActor* ThiefActor)
{
    if (!HasAuthority() || !TargetPlayer)
    {
        return;
    }
    
    Acasino_simulatorCharacter* Player = Cast<Acasino_simulatorCharacter>(TargetPlayer);

    if (!Player)
    {
        return;
    }

    const int32 StealAmount = FMath::RandRange(St_Money_Min / 50, St_Money_Max / 50) * 50;

    const float ActualStealAmount = FMath::Min(static_cast<float>(StealAmount), Player->GetCurrency());

    if (ActualStealAmount <= 0.0f)
    {
        return;
    }

    Player->TrySpendCurrency(ActualStealAmount);
    
    
}
