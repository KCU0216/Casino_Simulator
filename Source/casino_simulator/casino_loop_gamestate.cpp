#include "casino_loop_gamestate.h"

#include "Net/UnrealNetwork.h"

float ACasinoLoopGameState::GetRemainingPaymentSeconds() const
{
    return LoopStatus.Phase == ECasinoLoopPhase::Settling
        ? static_cast<float>(FMath::Max(0.0, LoopStatus.PaymentEndServerTime - GetServerWorldTimeSeconds()))
        : 0.0f;
}

float ACasinoLoopGameState::GetRemainingDaySeconds() const
{
    if (LoopStatus.Phase != ECasinoLoopPhase::Playing)
    {
        return 0.0f;
    }

    const double Remaining =
        LoopStatus.DayEndServerTime - GetServerWorldTimeSeconds();

    return static_cast<float>(FMath::Max(0.0, Remaining));
}

void ACasinoLoopGameState::SetLoopStatus(
    const FCasinoLoopStatus& NewStatus)
{
    if (!HasAuthority())
    {
        return;
    }

    LoopStatus = NewStatus;

    // 리슨 서버의 UI도 갱신
    OnLoopChanged.Broadcast();
    ForceNetUpdate();
}

void ACasinoLoopGameState::OnRep_LoopStatus()
{
    // 클라이언트의 UI 갱신
    OnLoopChanged.Broadcast();
}

void ACasinoLoopGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACasinoLoopGameState, LoopStatus);
}
