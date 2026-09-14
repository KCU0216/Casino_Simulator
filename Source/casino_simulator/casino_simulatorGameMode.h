// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "casino_simulatorGameMode.generated.h"


class APawn;
class AACtor;


/**
 *  Simple GameMode for a first person game
 */
UCLASS(abstract)
class Acasino_simulatorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	Acasino_simulatorGameMode();

	UFUNCTION(BlueprintCallable, Category = "Police")
	void ArrestPlayer(APawn* TargetPlayer, AActor* PoliceActor, AActor* JailPoint);

	UFUNCTION(BlueprintCallable, Category = "Police|Jail")
	bool PayBail(APawn* Player, float BailAmount);

};



