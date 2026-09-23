// Fill out your copyright notice in the Description page of Project Settings.



#include "ThiefCharacter.h"
#include "casino_simulatorCharacter.h"
#include "Net/UnrealNetwork.h"

AThiefCharacter::AThiefCharacter()
{
	Job = 2;
}

bool AThiefCharacter::CanSteal() const
{
	return ThiefState == EThiefState::Roaming;
}

bool AThiefCharacter::TryStealFrom(
    Acasino_simulatorCharacter* Player,
    float RequestedAmount)
{
    if (!HasAuthority() || !IsValid(Player) || !CanSteal())
    {
        return false;
    }

    if (!FMath::IsFinite(RequestedAmount) || RequestedAmount <= 0.0f)
    {
        return false;
    }

    const float ActualAmount =
        FMath::Min(RequestedAmount, Player->GetCurrency());

    if (ActualAmount <= 0.0f)
    {
        return false;
    }

    if (!Player->TrySpendCurrency(ActualAmount))
    {
        return false;
    }

    StolenMoney += ActualAmount;
    return true;
}

void AThiefCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AThiefCharacter, ThiefState);
}

void AThiefCharacter::HandleEnemyHitReaction(AActor* Attacker)
{
    if (!HasAuthority() || !IsValid(Attacker))
    {
        return;
    }

    Acasino_simulatorCharacter* AttackingPlayer =
        Cast<Acasino_simulatorCharacter>(Attacker);

    if (IsValid(AttackingPlayer)
        && AttackingPlayer->IsPlayerControlled()
        && StolenMoney > 0.0f)
    {
        if (ThiefState == EThiefState::Roaming)
        {
            
            EscapeFromActor = AttackingPlayer;
            ThiefState = EThiefState::Escaping;

            ForceNetUpdate();
        }
        else if (ThiefState == EThiefState::Escaping)
        {
           
            if (AttackingPlayer->GetAbilitySystemComponent() != nullptr)
            {
                const float Reward = StolenMoney;

               
                StolenMoney = 0.0f;
                EscapeFromActor = nullptr;
                ThiefState = EThiefState::Roaming;

                AttackingPlayer->AddCurrency(Reward);

                ApplyRandomPatrolSpeed();
                ForceNetUpdate();
            }
        }
    }
    Super::HandleEnemyHitReaction(Attacker);
}
