// Fill out your copyright notice in the Description page of Project Settings.



#include "ThiefCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "casino_simulatorCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
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
            // 돈을 가진 상태에서 첫 번째로 맞으면 도주합니다.
            EscapeFromActor = AttackingPlayer;
            ThiefState = EThiefState::Escaping;

            // 추가: 도주 상태에 맞춰 스프린트를 재생합니다.
            UpdateEscapeMontage();

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

                // 추가: 순찰 상태로 돌아왔으므로 스프린트를 정지합니다.
                UpdateEscapeMontage();

                AttackingPlayer->AddCurrency(Reward);

                ApplyRandomPatrolSpeed();
                ForceNetUpdate();
            }
        }
    }

    Super::HandleEnemyHitReaction(Attacker);
}

void AThiefCharacter::ApplyEscapeSpeed()
{
    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->MaxWalkSpeed = EscapeSpeed;
    }
}

void AThiefCharacter::OnRep_ThiefState()
{
    UpdateEscapeMontage();
}

void AThiefCharacter::UpdateEscapeMontage()
{
    if (!IsValid(EscapeMontage) || !GetMesh())
    {
        return;
    }

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

    if (!AnimInstance)
    {
        return;
    }

    if (IsEscaping())
    {
        // 이미 재생 중이면 처음부터 다시 시작하지 않습니다.
        if (AnimInstance->Montage_IsPlaying(EscapeMontage.Get()))
        {
            return;
        }

        const float Duration = AnimInstance->Montage_Play(
            EscapeMontage.Get(),
            1.0f,
            EMontagePlayReturnType::MontageLength,
            0.0f,
            false
        );

        if (Duration > 0.0f)
        {
            // 현재 몽타주의 Default 섹션을 계속 반복합니다.
            AnimInstance->Montage_SetNextSection(
                FName(TEXT("Default")),
                FName(TEXT("Default")),
                EscapeMontage.Get()
            );
        }
    }
    else
    {
        // 스프린트 몽타주만 정지합니다.
        AnimInstance->Montage_Stop(0.2f, EscapeMontage.Get());
    }
}
