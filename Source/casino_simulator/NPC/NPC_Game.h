// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/NPC_Base.h"
#include "NPC_Game.generated.h"

class UAnimMontage;

/**
 *  Base class for NPCs that host a minigame (e.g. the dice game). Holds the montages shared
 *  by any such minigame's start/play/end/success/fail beats.
 */
UCLASS(abstract)
class CASINO_SIMULATOR_API ANPC_Game : public ANPC_Base
{
	GENERATED_BODY()

protected:

	/** Played when a minigame this NPC hosts/joins (e.g. the dice game) starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> GameStartMontage;

	/** Played while the minigame is actively being played (e.g. a rolling/dealing loop) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> GamePlayMontage;

	/** Played once the minigame ends (win/lose/wrap-up reaction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> GameEndMontage;

	/** Played when the minigame resolves in this NPC's/player's favor (win reaction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> GameSuccessMontage;

	/** Played when the minigame resolves against this NPC's/player's favor (lose reaction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> GameFailMontage;
};
