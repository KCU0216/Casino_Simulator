#include "Police/BailATM.h"

#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"
#include "casino_loop_gamestate.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ABailATM::ABailATM()
{
    bReplicates = true;

    // 내부 ATM의 결제 상태를 모든 플레이어에게 전달합니다.
    bAlwaysRelevant = true;

    if (InteractionSphere)
    {
        InteractionSphere->InitSphereRadius(150.0f);
    }
}

ABailATM* ABailATM::GetBailPaymentOwner() const
{
    // 내부 ATM은 자신이 결제를 관리합니다.
    if (PrimaryBailATM == nullptr)
    {
        return const_cast<ABailATM*>(this);
    }

    ABailATM* LinkedATM = PrimaryBailATM.Get();

    // 외부 ATM은 내부 ATM을 직접 참조해야 합니다.
    if (!IsValid(LinkedATM)
        || LinkedATM == this
        || LinkedATM->PrimaryBailATM != nullptr
        || LinkedATM->GetWorld() != GetWorld())
    {
        return nullptr;
    }

    return LinkedATM;
}

float ABailATM::GetRemainingBailAmount() const
{
    const ABailATM* PaymentOwner = GetBailPaymentOwner();

    if (!IsValid(PaymentOwner)
        || !FMath::IsFinite(PaymentOwner->CurrentBailAmount)
        || !FMath::IsFinite(PaymentOwner->PaidBailAmount)
        || PaymentOwner->PaidBailAmount < 0.0f)
    {
        return 0.0f;
    }

    return FMath::Max(
        PaymentOwner->CurrentBailAmount - PaymentOwner->PaidBailAmount,
        0.0f);
}

bool ABailATM::CanInteract(
    Acasino_simulatorCharacter* InteractingCharacter) const
{
    const ABailATM* PaymentOwner = GetBailPaymentOwner();

    return IsCasinoGameplayAllowed(this)
        && IsValid(InteractingCharacter)
        && Super::CanInteract(InteractingCharacter)
        && IsValid(PaymentOwner)
        && PaymentOwner->bBailPaymentAvailable
        && IsValid(PaymentOwner->PrisonDoor.Get())
        && GetRemainingBailAmount() > 0.0f;
}

void ABailATM::Interact(
    Acasino_simulatorCharacter* InteractingCharacter)
{
    if (!HasAuthority() || !CanInteract(InteractingCharacter))
    {
        return;
    }

    auto* PC = Cast<Acasino_simulatorPlayerController>(
        InteractingCharacter->GetController());

    if (IsValid(PC) && !PC->IsPoliceCinematicActive())
    {
        // 실제 사용한 ATM을 전달합니다.
        PC->ClientOpenBailPayment(this);
    }
}

EBailPaymentResult ABailATM::SubmitBailPayment(
    Acasino_simulatorCharacter* InteractingCharacter,
    float RequestedAmount,
    float& OutPaidAmount)
{
    OutPaidAmount = 0.0f;

    ABailATM* PaymentOwner = GetBailPaymentOwner();

    // 거리 검사는 실제 사용한 ATM 위치를 기준으로 합니다.
    if (!HasAuthority()
        || !CanInteract(InteractingCharacter)
        || !IsValid(PaymentOwner)
        || !PaymentOwner->HasAuthority()
        || PaymentOwner->bProcessingBailPayment)
    {
        return EBailPaymentResult::Unavailable;
    }

    auto* PC = Cast<Acasino_simulatorPlayerController>(
        InteractingCharacter->GetController());

    if (!IsValid(PC) || PC->IsPoliceCinematicActive())
    {
        return EBailPaymentResult::Unavailable;
    }

    if (!FMath::IsFinite(RequestedAmount) || RequestedAmount <= 0.0f)
    {
        return EBailPaymentResult::InvalidAmount;
    }

    const float RemainingAmount = GetRemainingBailAmount();

    // 잔금보다 많이 요청해도 잔금만 받습니다.
    const float ActualAmount = FMath::Min(
        RequestedAmount, RemainingAmount);

    const float NewPaidAmount = FMath::Min(
        PaymentOwner->PaidBailAmount + ActualAmount,
        PaymentOwner->CurrentBailAmount);

    // 너무 작은 값으로 누적액이 증가하지 않는 경우도 거절합니다.
    if (!FMath::IsFinite(NewPaidAmount)
        || NewPaidAmount <= PaymentOwner->PaidBailAmount)
    {
        return EBailPaymentResult::InvalidAmount;
    }

    // 함수 종료 시 자동으로 잠금이 해제됩니다.
    TGuardValue<bool> PaymentGuard(
        PaymentOwner->bProcessingBailPayment, true);

    const float AvailableCurrency = InteractingCharacter->GetCurrency();

    if (!FMath::IsFinite(AvailableCurrency)
        || AvailableCurrency < ActualAmount
        || !InteractingCharacter->TrySpendCurrency(ActualAmount))
    {
        return EBailPaymentResult::InsufficientFunds;
    }

    const bool bFullyPaid =
        ActualAmount >= RemainingAmount
        || NewPaidAmount >= PaymentOwner->CurrentBailAmount;

    PaymentOwner->PaidBailAmount = bFullyPaid
        ? PaymentOwner->CurrentBailAmount
        : NewPaidAmount;

    OutPaidAmount = ActualAmount;

    if (bFullyPaid)
    {
        PaymentOwner->bBailPaymentAvailable = false;
        PaymentOwner->OnBailPaid();
    }

    PaymentOwner->NotifyBailStateChanged();

    return EBailPaymentResult::Success;
}

void ABailATM::ActivateBailPayment()
{
    // 경찰은 내부 ATM만 활성화합니다.
    if (!HasAuthority()
        || PrimaryBailATM != nullptr
        || !IsCasinoGameplayAllowed(this)
        || bBailPaymentAvailable
        || bProcessingBailPayment
        || PaidBailAmount > 0.0f
        || !IsValid(PrisonDoor.Get())
        || !FMath::IsFinite(CurrentBailAmount)
        || CurrentBailAmount <= 0.0f)
    {
        return;
    }

    PaidBailAmount = 0.0f;
    bBailPaymentAvailable = true;

    OnPrisonLocked();
    NotifyBailStateChanged();
}

void ABailATM::BeginCasinoDay_Implementation()
{
    if (!HasAuthority() || PrimaryBailATM != nullptr)
    {
        return;
    }

    const auto* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();

    if (!GS)
    {
        return;
    }

    const float FirstDayAmount =
        FMath::IsFinite(BailAmount) && BailAmount > 0.0f
        ? BailAmount
        : 100.0f;

    const float DebtRatio = FMath::IsFinite(BailDebtRatio)
        ? FMath::Clamp(BailDebtRatio, 0.01f, 1.0f)
        : 0.5f;

    if (GS->LoopStatus.CurrentDay <= 1)
    {
        PreviousDayRequiredPayment = 0;
    }

    CurrentBailAmount = FirstDayAmount;

    if (GS->LoopStatus.CurrentDay > 1
        && PreviousDayRequiredPayment > 0)
    {
        CurrentBailAmount =
            static_cast<float>(PreviousDayRequiredPayment) * DebtRatio;
    }

    ResetPrison();
}

void ABailATM::EndCasinoDay_Implementation()
{
    if (!HasAuthority() || PrimaryBailATM != nullptr)
    {
        return;
    }

    const auto* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();

    if (GS)
    {
        // 실제 납부액이 아니라 그날의 전체 채무 목표를 기록합니다.
        PreviousDayRequiredPayment = FMath::Max(
            0, GS->LoopStatus.RequiredPayment);
    }

    ResetPrison();
}

void ABailATM::ResetPrison()
{
    if (!HasAuthority() || PrimaryBailATM != nullptr)
    {
        return;
    }

    // 이미 차감한 돈은 환급하지 않습니다.
    bBailPaymentAvailable = false;
    PaidBailAmount = 0.0f;

    if (IsValid(PrisonDoor.Get()))
    {
        OnPrisonReset();
    }

    NotifyBailStateChanged();
}

void ABailATM::NotifyBailStateChanged()
{
    if (HasAuthority())
    {
        ForceNetUpdate();
    }

    // 서버 호스트의 UI도 여기서 갱신합니다.
    OnBailStateChanged.Broadcast();
}

void ABailATM::OnRep_BailState()
{
    // 클라이언트 UI를 갱신합니다.
    OnBailStateChanged.Broadcast();
}

void ABailATM::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABailATM, bBailPaymentAvailable);
    DOREPLIFETIME(ABailATM, CurrentBailAmount);
    DOREPLIFETIME(ABailATM, PaidBailAmount);
}