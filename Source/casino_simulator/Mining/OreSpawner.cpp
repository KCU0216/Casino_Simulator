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
	if (!HasAuthority())
	{
		return;
	}

	const int32 OreIndex = GetRandomOreIndex();
	if (!OreClasses.IsValidIndex(OreIndex) || !OreClasses[OreIndex])
	{
		return;
	}

	FActorSpawnParameters SpawnParameter;
	SpawnParameter.Owner = this;
	SpawnParameter.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;


	SpawnedOre = GetWorld()->SpawnActor<AOreBase>(
		OreClasses[OreIndex],
		GetActorTransform(),
		SpawnParameter);

	if (SpawnedOre)
	{
		SpawnedOre->OnOreDepleted.AddDynamic(this, &AOreSpawner::OnOreDepleted);
	}
	
}

int32 AOreSpawner::GetRandomOreIndex() const
{
	const int32 Roll = FMath::RandRange(1, 100);

	if (Roll <= 70)
	{
		return 0;
	}

	if (Roll <= 90)
	{
		return 1;
	}

	return 2;
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

