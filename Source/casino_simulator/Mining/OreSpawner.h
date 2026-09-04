// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OreSpawner.generated.h"

class AOreBase;
UCLASS()
class CASINO_SIMULATOR_API AOreSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOreSpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void SpawnOre();
	UPROPERTY(EditAnywhere, Category = "OreClass")
	TSubclassOf<AOreBase> OreClass;

	UPROPERTY(VisibleAnywhere)
	AOreBase* SpawnedOre;

	UPROPERTY(VisibleAnywhere, Category = "Respawn")
	float RespawnTime = 30.f;

	FTimerHandle HandleRespawn;

	UFUNCTION()
	void OnOreDepleted();

};
