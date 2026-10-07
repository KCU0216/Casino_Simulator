// Fill out your copyright notice in the Description page of Project Settings.

#include "Police/BailATM.h"
#include "casino_simulatorPlayerController.h"
#include "casino_simulatorCharacter.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"

ABailATM::ABailATM()
{
	bReplicates = true;


	if (InteractionSphere)
	{
		InteractionSphere->InitSphereRadius(150.0f);
	}
}

bool ABailATM::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	return bBailPaymentAvailable && IsValid(InteractingCharacter) && Super::CanInteract(InteractingCharacter);
}

void ABailATM::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	// 결제는 서버에서만 처리합니다.
	if (!HasAuthority()
		|| !IsCasinoGameplayAllowed(this)
		|| !CanInteract(InteractingCharacter)
		|| !IsValid(PrisonDoor.Get())
		|| !FMath::IsFinite(BailAmount)
		|| BailAmount <= 0.0f)
	{
		return;
	}

	// 결제 처리 중에는 추가 결제를 받지 않습니다.
	bBailPaymentAvailable = false;

	if (!InteractingCharacter->TrySpendCurrency(BailAmount))
	{
		bBailPaymentAvailable = true;

		if (auto* PC = Cast<Acasino_simulatorPlayerController>(
			InteractingCharacter->GetController()))
		{
			PC->ClientBailPaymentFailed(this);
		}

		return;
	}
	ForceNetUpdate();
	OnBailPaid();

}

void ABailATM::ActivateBailPayment()
{
	if (!HasAuthority()
		|| !IsCasinoGameplayAllowed(this)
		|| bBailPaymentAvailable
		|| !IsValid(PrisonDoor.Get()))
	{
		return;
	}

	OnPrisonLocked();

	bBailPaymentAvailable = true;
	ForceNetUpdate();
}


void ABailATM::BeginCasinoDay_Implementation()
{
	ResetPrison();
}

void ABailATM::EndCasinoDay_Implementation()
{
	ResetPrison();
}

void ABailATM::ResetPrison()
{
	if (!HasAuthority())
	{
		return;
	}

	bBailPaymentAvailable = false;
	ForceNetUpdate();

	if (IsValid(PrisonDoor.Get()))
	{
		OnPrisonReset();
	}
}

void ABailATM::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABailATM, bBailPaymentAvailable);
}

