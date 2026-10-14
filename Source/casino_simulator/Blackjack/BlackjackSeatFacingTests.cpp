// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blackjack/BlackjackPlayerComponent.h"
#include "Blackjack/BlackjackTableActor.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulatorCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackSeatFacingTest,
	"Casino.Blackjack.SeatFacing.Local",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackSeatFacingTest::RunTest(const FString& Parameters)
{
	// The native character is abstract; use the shipped concrete subclass.
	UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
		TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("Concrete seated character class"), CharacterClass)) { return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Isolated seat-facing world"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACasinoLoopGameState* State = World->SpawnActor<ACasinoLoopGameState>();
	ABlackjackTableActor* Table = World->SpawnActor<ABlackjackTableActor>(
		ABlackjackTableActor::StaticClass(), FVector(500.0f, -300.0f, 0.0f), FRotator(8.0f, 123.0f, -6.0f), Spawn);
	Acasino_simulatorCharacter* Character = World->SpawnActor<Acasino_simulatorCharacter>(
		CharacterClass, FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator, Spawn);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Playing game state"), State) || !TestNotNull(TEXT("Rotated table"), Table)
		|| !TestNotNull(TEXT("Concrete seated character"), Character) || !TestNotNull(TEXT("Native controller"), Controller))
	{
		return false;
	}
	World->SetGameState(State);
	State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
	Table->DispatchBeginPlay();
	Controller->Possess(Character);
	if (!TestTrue(TEXT("Test controller possesses the seated character"), Controller->GetPawn() == Character)) { return false; }
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Character, Character);
	ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);

	UBlackjackPlayerComponent* Component = Character->GetBlackjackPlayerComponent();
	FStructProperty* SessionProperty = FindFProperty<FStructProperty>(Component->GetClass(), TEXT("SeatSession"));
	UFunction* SeatNotify = Component->FindFunction(TEXT("OnRep_BlackjackSeatMode"));
	if (!TestNotNull(TEXT("Replicated seat snapshot"), SessionProperty)
		|| !TestNotNull(TEXT("Seat RepNotify"), SeatNotify)) { return false; }
	FBlackjackSeatSession* Session = SessionProperty->ContainerPtrToValuePtr<FBlackjackSeatSession>(Component);

	// A real controller's UpdateRotation also runs on the server when a late
	// movement packet supplies the player's old look direction. No camera or
	// Blueprint asset is needed to exercise the resulting Pawn::FaceRotation.
	for (int32 SeatIndex = 0; SeatIndex < 4; ++SeatIndex)
	{
		USceneComponent* SeatPoint = Table->GetSeatPoint(SeatIndex);
		if (!TestNotNull(TEXT("Configured seat point"), SeatPoint)) { return false; }
		const FRotator ExpectedFacing(0.0f, SeatPoint->GetComponentRotation().Yaw, 0.0f);
		const FRotator OldLook(27.0f, ExpectedFacing.Yaw + 70.0f + SeatIndex * 15.0f, 0.0f);
		const bool bOriginalControllerYaw = SeatIndex % 2 == 0;
		Character->bUseControllerRotationYaw = bOriginalControllerYaw;
		Character->SetActorRotation(FRotator(0.0f, OldLook.Yaw, 0.0f));
		Controller->SetControlRotation(OldLook);
		if (!TestTrue(FString::Printf(TEXT("Seat %d can be claimed"), SeatIndex), Table->TryClaimSeat(Character, SeatIndex)))
		{
			return false;
		}
		Component->EnterBlackjackSeatMode(Table, SeatIndex);
		TestFalse(FString::Printf(TEXT("Seat %d disables controller body yaw"), SeatIndex), Character->bUseControllerRotationYaw);
		TestTrue(FString::Printf(TEXT("Seat %d uses upright world seat yaw"), SeatIndex),
			Character->GetActorRotation().Equals(ExpectedFacing, 0.01f));
		TestTrue(FString::Printf(TEXT("Seat %d aligns control rotation"), SeatIndex),
			Controller->GetControlRotation().Equals(ExpectedFacing, 0.01f));

		// Repeating the same seat event must not save the already-disabled yaw
		// flag as the original value that will be restored when the player leaves.
		Component->EnterBlackjackSeatMode(Table, SeatIndex);
		Component->ProcessEvent(SeatNotify, nullptr);
		Component->ProcessEvent(SeatNotify, nullptr);
		Controller->SetControlRotation(OldLook);
		Controller->UpdateRotation(1.0f / 60.0f);
		TestTrue(FString::Printf(TEXT("Seat %d ignores late pre-seat look yaw"), SeatIndex),
			Character->GetActorRotation().Equals(ExpectedFacing, 0.01f));

		// Exercise the exact replicated removal/reassignment path. The table's
		// authority-side occupant deliberately stays intact until CompleteExit.
		*Session = FBlackjackSeatSession();
		Component->ProcessEvent(SeatNotify, nullptr);
		Component->ProcessEvent(SeatNotify, nullptr);
		TestEqual(FString::Printf(TEXT("Seat %d replicated removal restores original yaw flag"), SeatIndex),
			static_cast<bool>(Character->bUseControllerRotationYaw), bOriginalControllerYaw);
		TestTrue(FString::Printf(TEXT("Seat %d replicated removal aligns the exit view"), SeatIndex),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Controller->GetControlRotation().Yaw, Character->GetActorRotation().Yaw)) < 0.01f);
		Controller->UpdateRotation(1.0f / 60.0f);
		TestTrue(FString::Printf(TEXT("Seat %d replicated removal cannot snap back to old look yaw"), SeatIndex),
			Character->GetActorRotation().Equals(ExpectedFacing, 0.01f));

		Controller->SetControlRotation(OldLook);
		Session->Table = Table;
		Session->SeatIndex = SeatIndex;
		Component->ProcessEvent(SeatNotify, nullptr);
		Component->ProcessEvent(SeatNotify, nullptr);
		TestFalse(FString::Printf(TEXT("Seat %d replicated assignment disables body yaw"), SeatIndex),
			Character->bUseControllerRotationYaw);
		TestTrue(FString::Printf(TEXT("Seat %d replicated assignment applies seat yaw"), SeatIndex),
			Character->GetActorRotation().Equals(ExpectedFacing, 0.01f));
		TestTrue(FString::Printf(TEXT("Seat %d replicated assignment aligns control rotation"), SeatIndex),
			Controller->GetControlRotation().Equals(ExpectedFacing, 0.01f));
		Controller->SetControlRotation(OldLook);
		Controller->UpdateRotation(1.0f / 60.0f);
		TestTrue(FString::Printf(TEXT("Seat %d replicated assignment remains fixed after controller update"), SeatIndex),
			Character->GetActorRotation().Equals(ExpectedFacing, 0.01f));

		Component->CompleteExitBlackjackSeat();
		TestFalse(FString::Printf(TEXT("Seat %d authoritative exit clears seated state"), SeatIndex), Component->IsInBlackjackSeat());
		TestEqual(FString::Printf(TEXT("Seat %d authoritative exit releases occupancy"), SeatIndex),
			Table->GetSeatIndexForPlayer(Character), INDEX_NONE);
		TestEqual(FString::Printf(TEXT("Seat %d authoritative exit preserves original yaw flag"), SeatIndex),
			static_cast<bool>(Character->bUseControllerRotationYaw), bOriginalControllerYaw);
		TestTrue(FString::Printf(TEXT("Seat %d exit aligns controller to body"), SeatIndex),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Controller->GetControlRotation().Yaw, Character->GetActorRotation().Yaw)) < 0.01f);
		Controller->UpdateRotation(1.0f / 60.0f);
		TestTrue(FString::Printf(TEXT("Seat %d authoritative exit cannot snap back to old look yaw"), SeatIndex),
			Character->GetActorRotation().Equals(ExpectedFacing, 0.01f));
	}
	return true;
}

#endif
