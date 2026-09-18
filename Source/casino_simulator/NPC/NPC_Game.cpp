// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC_Game.h"
#include "casino_simulatorCharacter.h"
#include "Interaction/WorldInteractionDetectorComponent.h"


void ANPC_Game::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Super::OnInteractionSphereBeginOverlap(OverlappedComponent,  OtherActor,  OtherComp,  OtherBodyIndex, bFromSweep, SweepResult);

	/*Acasino_simulatorCharacter* PlayerCharacter = Cast<Acasino_simulatorCharacter>(OtherActor);

	if (OverlappingPlayer == nullptr && PlayerCharacter != nullptr)
	{
		OverlappingPlayer = PlayerCharacter;

		if (UWorldInteractionDetectorComponent* Detector = PlayerCharacter->GetWorldInteractionDetector())
		{
			Detector->RegisterCandidate(this);
		}
	}*/
}

void ANPC_Game::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Super::OnInteractionSphereEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
}
