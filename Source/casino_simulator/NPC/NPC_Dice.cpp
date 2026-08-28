// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/NPC_Dice.h"
#include "Components/StaticMeshComponent.h"
#include "DiceGame.h"
#include "casino_simulatorCharacter.h"
#include "Net/UnrealNetwork.h"

ANPC_Dice::ANPC_Dice()
{
	// Decorative prop only - no collision, just follows the character mesh (e.g. a hand socket).
	CupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CupMesh"));
	CupMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
	CupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANPC_Dice::BeginPlay()
{
	Super::BeginPlay();

	// Captured before any interaction can retarget Owner, so EndInteraction has something to restore.
	DefaultOwner = GetOwner();

	// Only the server spawns the dice table; it replicates down to clients via DiceGameInstance
	// (marked Replicated) once ADiceGame itself is a replicated actor.
	if (!HasAuthority() || !DiceGameClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FTransform SpawnTransform = FTransform(GetActorRotation(), GetActorLocation() + DiceGameOffset);
	DiceGameInstance = GetWorld()->SpawnActor<ADiceGame>(DiceGameClass, SpawnTransform, SpawnParams);
	DiceGameInstance->SetOwner(this);
}

void ANPC_Dice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANPC_Dice, DiceGameInstance);
}

void ANPC_Dice::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	SetInteractingPlayer(InteractingCharacter);
	Super::Interact(InteractingCharacter);
}

void ANPC_Dice::EndInteraction()
{
	SetInteractingPlayer(nullptr);
}

void ANPC_Dice::SetInteractingPlayer(Acasino_simulatorCharacter* Player)
{
	if (!HasAuthority())
	{
		return;
	}

	InteractingPlayer = Player;

	// Owning the NPC while a specific player is using it gives ROLE_AutonomousProxy to that
	// player's client only, which is required for that client to call a Server RPC declared
	// directly on this NPC. Reverts to DefaultOwner once nobody's interacting.
	SetOwner(Player ? static_cast<AActor*>(Player) : DefaultOwner.Get());
}

bool ANPC_Dice::PlaceBet(Acasino_simulatorCharacter* Player, int32 Select, int32 Betting)
{
	if (!Player || Betting <= 0)
	{
		return false;
	}

	if (HasAuthority())
	{
		return ExecutePlaceBet(Player, Select, Betting);
	}

	// This NPC isn't owned by any player's connection, so a Server RPC declared on it would just
	// be dropped if called from a client. Route through Player's own Character instead, which IS
	// owned by the calling client's connection.
	Player->ServerPlaceDiceBet(this, Select, Betting);
	return true;
}

bool ANPC_Dice::ExecutePlaceBet(Acasino_simulatorCharacter* Player, int32 Select, int32 Betting)
{
	if (!HasAuthority() || !Player || Betting <= 0)
	{
		return false;
	}

	// Only the player currently interacting with this NPC may place a bet on it.
	if (InteractingPlayer.Get() != Player)
	{
		return false;
	}

	if (!Player->TrySpendCurrency(static_cast<float>(Betting)))
	{
		return false;
	}

	SetBetValue(Select, Betting);
	return true;
}

void ANPC_Dice::SetBetValue(int32 Select, int32 Betting)
{
	SelectedValue = Select;
	BettingAmount = Betting;
}

bool ANPC_Dice::ShowResult(int32 ResultValue)
{
	bool bResult = ResultValue % 2 == SelectedValue;
	if (bResult)
	{
		if (Acasino_simulatorCharacter* Player = InteractingPlayer.Get())
		{
			Player->AddCurrency(static_cast<float>(BettingAmount * 2));
		}
	}
	else
	{

	}
	return bResult;
}
