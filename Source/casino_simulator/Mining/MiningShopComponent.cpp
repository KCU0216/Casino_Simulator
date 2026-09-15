// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mining/MiningShopComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerState.h"

UMiningShopComponent::UMiningShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMiningShopComponent::RequestBuyPowerUpgrade(Acasino_simulatorCharacter* Buyer)
{
	RequestBuyUpgrade(Buyer, EMiningShopUpgradeType::Power);
}

void UMiningShopComponent::RequestBuySpeedUpgrade(Acasino_simulatorCharacter* Buyer)
{
	RequestBuyUpgrade(Buyer, EMiningShopUpgradeType::Speed);
}

void UMiningShopComponent::RequestBuyUpgrade(Acasino_simulatorCharacter* Buyer, EMiningShopUpgradeType UpgradeType)
{
	if (!Buyer)
	{
		FailPurchase(UpgradeType, TEXT("Buyer is invalid."));
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ProcessUpgradePurchase(Buyer, UpgradeType);
		return;
	}

	Buyer->ServerBuyMiningShopUpgrade(this, UpgradeType);
}

bool UMiningShopComponent::ProcessUpgradePurchase(Acasino_simulatorCharacter* Buyer, EMiningShopUpgradeType UpgradeType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (!Buyer)
	{
		FailPurchase(UpgradeType, TEXT("Buyer is invalid."));
		return false;
	}

	Acasino_simulatorPlayerState* CasinoPlayerState = Buyer->GetPlayerState<Acasino_simulatorPlayerState>();
	if (!CasinoPlayerState)
	{
		Buyer->ClientMiningShopPurchaseFailed(this, UpgradeType, TEXT("PlayerState is invalid."));
		return false;
	}

	const int32 TotalPrice = GetUpgradePrice(Buyer, UpgradeType);

	if (TotalPrice <= 0)
	{
		Buyer->ClientMiningShopPurchaseFailed(this, UpgradeType, TEXT("Invalid upgrade price."));
		return false;
	}

	if (!GetUpgradeEffectClass(UpgradeType))
	{
		Buyer->ClientMiningShopPurchaseFailed(this, UpgradeType, TEXT("Upgrade effect is not set."));
		return false;
	}

	if (!Buyer->TrySpendCurrency(static_cast<float>(TotalPrice)))
	{
		Buyer->ClientMiningShopPurchaseFailed(this, UpgradeType, TEXT("Not enough money."));
		return false;
	}

	if (!ApplyUpgradeEffect(Buyer, UpgradeType))
	{
		Buyer->AddCurrency(static_cast<float>(TotalPrice));
		Buyer->ClientMiningShopPurchaseFailed(this, UpgradeType, TEXT("Could not apply upgrade effect."));
		return false;
	}

	if (UpgradeType == EMiningShopUpgradeType::Power)
	{
		CasinoPlayerState->AddMiningPowerUpgradeLevel(1);
	}
	else
	{
		CasinoPlayerState->AddMiningSpeedUpgradeLevel(1);
	}

	Buyer->ClientMiningShopPurchaseCompleted(this, UpgradeType, TotalPrice);
	return true;
}

int32 UMiningShopComponent::GetUpgradePrice(Acasino_simulatorCharacter* Buyer, EMiningShopUpgradeType UpgradeType) const
{
	const Acasino_simulatorPlayerState* CasinoPlayerState = Buyer ? Buyer->GetPlayerState<Acasino_simulatorPlayerState>() : nullptr;
	if (!CasinoPlayerState)
	{
		return 0;
	}

	if (UpgradeType == EMiningShopUpgradeType::Power)
	{
		return PowerUpgradeBasePrice + (CasinoPlayerState->GetMiningPowerUpgradeLevel() * PowerUpgradePriceIncrease);
	}

	return SpeedUpgradeBasePrice + (CasinoPlayerState->GetMiningSpeedUpgradeLevel() * SpeedUpgradePriceIncrease);
}

void UMiningShopComponent::HandlePurchaseCompletedFromServer(EMiningShopUpgradeType UpgradeType, int32 TotalPrice)
{
	OnPurchaseCompleted.Broadcast(UpgradeType, TotalPrice);
}

void UMiningShopComponent::HandlePurchaseFailedFromServer(EMiningShopUpgradeType UpgradeType, const FString& Reason)
{
	OnPurchaseFailed.Broadcast(UpgradeType, Reason);
}

bool UMiningShopComponent::ApplyUpgradeEffect(Acasino_simulatorCharacter* Buyer, EMiningShopUpgradeType UpgradeType) const
{
	if (!Buyer)
	{
		return false;
	}

	TSubclassOf<UGameplayEffect> EffectClass = GetUpgradeEffectClass(UpgradeType);
	if (!EffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent = Buyer->GetAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}

TSubclassOf<UGameplayEffect> UMiningShopComponent::GetUpgradeEffectClass(EMiningShopUpgradeType UpgradeType) const
{
	return UpgradeType == EMiningShopUpgradeType::Power ? PowerUpgradeEffectClass : SpeedUpgradeEffectClass;
}

void UMiningShopComponent::FailPurchase(EMiningShopUpgradeType UpgradeType, const FString& Reason)
{
	OnPurchaseFailed.Broadcast(UpgradeType, Reason);
}
