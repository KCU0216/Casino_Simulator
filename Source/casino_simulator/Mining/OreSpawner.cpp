// Fill out your copyright notice in the Description page of Project Settings.


#include "Mining/OreSpawner.h"
#include "Mining/OreBase.h"

// Sets default values
AOreSpawner::AOreSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AOreSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SpawnOre();
	}
	
}

// Called every frame
void AOreSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AOreSpawner::SpawnOre()
{
	if (!HasAuthority() || !OreClass)
	{
		return;
	}

	if (!OreClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameter;
	SpawnParameter.Owner = this;
	SpawnParameter.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;


	SpawnedOre = GetWorld()->SpawnActor<AOreBase>(OreClass, GetActorTransform(), SpawnParameter);

	if (SpawnedOre)
	{
		SpawnedOre->OnOreDepleted.AddDynamic(this, &AOreSpawner::OnOreDepleted);
	}
	
}

void AOreSpawner::OnOreDepleted()
{

	SpawnedOre = nullptr;

	GetWorldTimerManager().SetTimer(
		HandleRespawn,
		this,
		&AOreSpawner::SpawnOre,
		RespawnTime,
		false
	);
}

