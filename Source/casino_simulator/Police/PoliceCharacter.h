// Copyright Epic Games, Inc. All Rights Reserved.
// ACharacter를 상속받는다.
// 캡슐 충돌, 스켈레탈 메시, CharacterMovementComponent를 기본으로 가진다.
#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBaseCharacter.h"
#include "PoliceCharacter.generated.h"

UCLASS(Blueprintable)
class CASINO_SIMULATOR_API APoliceCharacter : public AEnemyBaseCharacter
{
	GENERATED_BODY()

public:
	APoliceCharacter();
};


