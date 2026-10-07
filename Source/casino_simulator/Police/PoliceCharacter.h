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

	UFUNCTION(BlueprintImplementableEvent, Category = "Police|Presentation")
	void OnPoliceArrivalStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Police|Presentation")
	void OnPoliceEncounterStopped();

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "Police|Jail")
	AActor* GetPoliceJailDestination() const;

	// 감옥 이동 성공 후 경찰 BP에 알립니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Police|Jail")
	void OnPolicePlayerJailed();
};


