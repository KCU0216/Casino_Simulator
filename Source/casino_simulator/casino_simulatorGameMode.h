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

	UFUNCTION(BlueprintCallable, Category = "Thief")
	void StealMoney(APawn* TargetPlayer, AActor* ThiefActor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thief")
	int St_Money_Max = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thief")
	int St_Money_Min = 100;
};



