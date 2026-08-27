// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiceGame.generated.h"

class ADice;
class UTextRenderComponent;
class UStaticMeshComponent;

UCLASS()
class CASINO_SIMULATOR_API ADiceGame : public AActor
{
	GENERATED_BODY()

public:
	ADiceGame();

	/** Shows/hides both dice and rolls them to the given result value. */
	UFUNCTION(BlueprintCallable, Category = "Dice Game")
	void SetDice(bool bVisible, int32 ResultValue);

protected:
	/** Dice class to spawn as the first die (e.g. BP_Dice). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice Game")
	TSubclassOf<ADice> BP_Dice_1;

	/** Dice class to spawn as the second die (e.g. BP_Dice). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice Game")
	TSubclassOf<ADice> BP_Dice_2;

	/** Instance spawned from BP_Dice_1. */
	UPROPERTY(BlueprintReadOnly, Category = "Dice Game")
	TObjectPtr<ADice> SpawnedDice1;

	/** Instance spawned from BP_Dice_2. */
	UPROPERTY(BlueprintReadOnly, Category = "Dice Game")
	TObjectPtr<ADice> SpawnedDice2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice Game")
	FVector StartPos;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice Game")
	FVector DistancePos;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Dice Game")
	FVector DiceScale;

	/** Displays the dice result above the table. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dice Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextRenderComponent> ResultText;

	/** Visual mesh for the dice game (e.g. tray/table). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dice Game", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	virtual void BeginPlay() override;

private:
	/** Spawns one die from the given class at the given transform, or returns nullptr if the class isn't set. */
	ADice* SpawnDice(TSubclassOf<ADice> DiceClass, const FTransform& SpawnTransform) const;

	/** Reveals ResultText with the given value; called after ResultTextRevealDelay once dice start rolling. */
	void ShowResultText(int32 ResultValue);

	/** Delay (seconds) before ResultText appears after SetDice(true, ...) is called, so it shows up after the dice roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice Game", meta = (AllowPrivateAccess = "true"))
	float ResultTextRevealDelay = 0.4f;

	FTimerHandle ResultTextTimerHandle;
};
