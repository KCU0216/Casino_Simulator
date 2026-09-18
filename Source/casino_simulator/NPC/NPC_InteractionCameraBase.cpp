// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/NPC_InteractionCameraBase.h"
#include "Camera/CameraComponent.h"
#include "casino_simulatorCharacter.h"
#include "Interaction/WorldInteractionDetectorComponent.h"
#include "Components/CapsuleComponent.h"

ANPC_InteractionCameraBase::ANPC_InteractionCameraBase()
{
	InteractionCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera"));
	InteractionCameraComponent->SetupAttachment(GetCapsuleComponent());
	InteractionCameraComponent->SetRelativeLocation(FVector(180.0f, -180.0f, 90.0f));
	InteractionCameraComponent->SetRelativeRotation(FRotator(0.0f, 135.0f, 0.0f));
	InteractionCameraComponent->bAutoActivate = true;
}

AActor* ANPC_InteractionCameraBase::GetInteractionCameraTarget() const
{
	return InteractionCameraTarget ? InteractionCameraTarget.Get() : const_cast<ANPC_InteractionCameraBase*>(this);
}

void ANPC_InteractionCameraBase::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Super::OnInteractionSphereBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	
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

void ANPC_InteractionCameraBase::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Super::OnInteractionSphereEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
}
