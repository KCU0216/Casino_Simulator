// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/NPC_InteractionCameraBase.h"
#include "Camera/CameraComponent.h"
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
