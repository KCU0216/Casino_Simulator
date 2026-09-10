// Copyright Epic Games, Inc. All Rights Reserved.

#include "Police/PoliceCharacter.h"

#include "Police/PoliceAIController.h"

APoliceCharacter::APoliceCharacter()
{
	AIControllerClass = APoliceAIController::StaticClass();
}
