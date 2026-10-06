// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blackjack/BlackjackPlayerComponent.h"
#include "Blackjack/BlackjackSeatInteractionActor.h"
#include "Blackjack/BlackjackTableActor.h"
#include "Blackjack/BlackjackTableInteractionActor.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UI/CasinoUIManagerComponent.h"
#include "UObject/UnrealType.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackSeatedInteractionTest,
	"Casino.Blackjack.SeatedInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackSeatedInteractionTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
		TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("Playable character class"), CharacterClass)) { return false; }

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
	TGuardValue<uint64> FrameCounterGuard(GFrameCounter, GFrameCounter);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABlackjackTableActor* Table = World->SpawnActor<ABlackjackTableActor>();
	ABlackjackTableActor* OtherTable = World->SpawnActor<ABlackjackTableActor>();
	if (!TestNotNull(TEXT("Table"), Table) || !TestNotNull(TEXT("Other table"), OtherTable)) { return false; }
	Table->DispatchBeginPlay();
	OtherTable->DispatchBeginPlay();
	FIntProperty* AllowedSeat = FindFProperty<FIntProperty>(
		ABlackjackTableInteractionActor::StaticClass(), TEXT("AllowedSeatIndex"));
	FObjectPropertyBase* ChipTable = FindFProperty<FObjectPropertyBase>(
		ABlackjackTableInteractionActor::StaticClass(), TEXT("BlackjackTable"));
	FEnumProperty* ActionProperty = FindFProperty<FEnumProperty>(
		ABlackjackTableInteractionActor::StaticClass(), TEXT("InteractionAction"));
	FEnumProperty* RoundProperty = FindFProperty<FEnumProperty>(Table->GetClass(), TEXT("RoundState"));
	if (!TestNotNull(TEXT("Seat restriction"), AllowedSeat) || !TestNotNull(TEXT("Chip table reference"), ChipTable)
		|| !TestNotNull(TEXT("Chip action"), ActionProperty) || !TestNotNull(TEXT("Round state"), RoundProperty))
	{
		return false;
	}
	ABlackjackTableInteractionActor* SharedChip = World->SpawnActor<ABlackjackTableInteractionActor>();
	ABlackjackTableInteractionActor* ForeignChip = World->SpawnActor<ABlackjackTableInteractionActor>();
	if (!TestNotNull(TEXT("Shared betting chip"), SharedChip) || !TestNotNull(TEXT("Foreign chip"), ForeignChip)) { return false; }
	SharedChip->AttachToActor(Table, FAttachmentTransformRules::KeepWorldTransform);
	ForeignChip->AttachToActor(OtherTable, FAttachmentTransformRules::KeepWorldTransform);
	SharedChip->DispatchBeginPlay();
	ForeignChip->DispatchBeginPlay();

	Acasino_simulatorCharacter* Players[4];
	ABlackjackSeatInteractionActor* SeatTargets[4];
	ABlackjackTableInteractionActor* OwnChips[4];
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Players[Index] = World->SpawnActor<Acasino_simulatorCharacter>(CharacterClass,
			FVector(Index * 30.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Spawn);
		SeatTargets[Index] = World->SpawnActor<ABlackjackSeatInteractionActor>(
			ABlackjackSeatInteractionActor::StaticClass(), Table->GetSeatPoint(Index)->GetComponentLocation(),
			Table->GetSeatPoint(Index)->GetComponentRotation(), Spawn);
		OwnChips[Index] = World->SpawnActor<ABlackjackTableInteractionActor>();
		if (!TestNotNull(TEXT("Player"), Players[Index]) || !TestNotNull(TEXT("Seat target"), SeatTargets[Index])
			|| !TestNotNull(TEXT("Own-seat chip"), OwnChips[Index])) { return false; }
		auto* ASC = Players[Index]->GetAbilitySystemComponent();
		ASC->InitAbilityActorInfo(Players[Index], Players[Index]);
		ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
		ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 10000.0f);
		SeatTargets[Index]->AttachToActor(Table, FAttachmentTransformRules::KeepWorldTransform);
		SeatTargets[Index]->DispatchBeginPlay();
		OwnChips[Index]->AttachToActor(Table, FAttachmentTransformRules::KeepWorldTransform);
		AllowedSeat->SetPropertyValue_InContainer(OwnChips[Index], Index);
		OwnChips[Index]->DispatchBeginPlay();
		TestNull(TEXT("Unseated player has no betting target"), Table->GetBettingInteractionTarget(Players[Index]));
		SeatTargets[Index]->Interact(Players[Index]);
		TestEqual(TEXT("Seat interaction assigns the intended index"),
			Players[Index]->GetBlackjackPlayerComponent()->GetCurrentSeatIndex(), Index);
		TestTrue(TEXT("Own chip is preferred over the earlier shared chip"),
			Table->GetBettingInteractionTarget(Players[Index]) == OwnChips[Index]);
		TestNull(TEXT("Another table never supplies the seated player's betting target"),
			OtherTable->GetBettingInteractionTarget(Players[Index]));
		TestFalse(TEXT("Another table's chip cannot interact"), ForeignChip->CanInteract(Players[Index]));

		UCameraComponent* Camera = SeatTargets[Index]->GetSeatCamera();
		if (!TestNotNull(TEXT("Seat camera"), Camera)) { return false; }
		Camera->SetRelativeLocationAndRotation(FVector(10.0f + Index, 20.0f * Index, 120.0f + Index),
			FRotator(-20.0f - Index, 5.0f * Index, 0.0f));
		Camera->SetFieldOfView(60.0f + Index);
		FMinimalViewInfo View;
		SeatTargets[Index]->CalcCamera(0.0f, View);
		TestTrue(TEXT("Every seat resolves its own view target"), Table->GetSeatCameraTarget(Index) == SeatTargets[Index]);
		TestTrue(TEXT("Seat view uses its configured camera location"), View.Location.Equals(Camera->GetComponentLocation()));
		TestTrue(TEXT("Seat view uses its configured camera rotation"), View.Rotation.Equals(Camera->GetComponentRotation()));
		TestEqual(TEXT("Seat view preserves configured FOV"), View.FOV, 60.0f + Index);
		TestFalse(TEXT("Seat camera does not follow pawn look input"), Camera->bUsePawnControlRotation);
		if (Index > 0)
		{
			TestTrue(TEXT("Neighbor seats use distinct camera components"), Camera != SeatTargets[Index - 1]->GetSeatCamera());
			TestTrue(TEXT("Neighbor seats use distinct view actors"), Table->GetSeatCameraTarget(Index) != Table->GetSeatCameraTarget(Index - 1));
		}
	}
	TestNull(TEXT("Invalid camera seat is rejected"), Table->GetSeatCameraTarget(INDEX_NONE));
	TestNull(TEXT("Out-of-range camera seat is rejected"), Table->GetSeatCameraTarget(4));
	TestNull(TEXT("Another table does not borrow an attached seat camera"), OtherTable->GetSeatCameraTarget(0));

	// Losing a seat-specific target must fall back to the shared OpenBetting target,
	// never an unrelated action or a same-table actor found by a world-wide scan.
	ActionProperty->GetUnderlyingProperty()->SetIntPropertyValue(
		ActionProperty->ContainerPtrToValuePtr<void>(OwnChips[0]), static_cast<uint64>(EBlackjackTableInteractionAction::Hit));
	TestTrue(TEXT("A non-betting action cannot replace the keypad target"),
		Table->GetBettingInteractionTarget(Players[0]) == SharedChip);
	ActionProperty->GetUnderlyingProperty()->SetIntPropertyValue(
		ActionProperty->ContainerPtrToValuePtr<void>(OwnChips[0]), static_cast<uint64>(EBlackjackTableInteractionAction::OpenBetting));
	OwnChips[0]->SetActorLocation(FVector(10000.0f, 0.0f, 0.0f));
	TestTrue(TEXT("An out-of-range own target falls back to the usable shared chip"),
		Table->GetBettingInteractionTarget(Players[0]) == SharedChip);
	OwnChips[0]->SetActorLocation(FVector::ZeroVector);
	ABlackjackTableInteractionActor* UnregisteredChip = World->SpawnActor<ABlackjackTableInteractionActor>();
	if (!TestNotNull(TEXT("Unregistered chip"), UnregisteredChip)) { return false; }
	ChipTable->SetObjectPropertyValue_InContainer(UnregisteredChip, Table);
	SharedChip->Destroy();
	OwnChips[0]->Destroy();
	TestNull(TEXT("Unregistered and unattached actors are not discovered by world scans"),
		Table->GetBettingInteractionTarget(Players[0]));
	UnregisteredChip->DispatchBeginPlay();
	TestTrue(TEXT("An explicit table reference registers a usable level-placed chip"),
		Table->GetBettingInteractionTarget(Players[0]) == UnregisteredChip);

	const float StartingBalance = Players[0]->GetCurrency();
	UnregisteredChip->Interact(Players[0]);
	TestEqual(TEXT("Opening the keypad does not charge currency"), Players[0]->GetCurrency(), StartingBalance);
	TestFalse(TEXT("Opening the keypad does not start the betting countdown"), Table->IsBettingCountdownActive());
	TestTrue(TEXT("First bet is accepted"), Players[0]->GetBlackjackPlayerComponent()->PlaceBet(100));
	TestEqual(TEXT("First bet charges exactly once"), Players[0]->GetCurrency(), StartingBalance - 100.0f);
	TestEqual(TEXT("First bet starts the existing 15-second countdown"), Table->GetBettingRemainingTime(), 15.0f);
	TestNull(TEXT("An already-bet player cannot resolve a keypad target"), Table->GetBettingInteractionTarget(Players[0]));
	TestFalse(TEXT("Duplicate bet is rejected"), Players[0]->GetBlackjackPlayerComponent()->PlaceBet(100));
	TestEqual(TEXT("Duplicate bet does not charge again"), Players[0]->GetCurrency(), StartingBalance - 100.0f);
	++GFrameCounter;
	World->TimeSeconds += 5.0;
	World->GetTimerManager().Tick(5.0f);
	OwnChips[1]->Interact(Players[1]);
	TestEqual(TEXT("Opening another keypad preserves the original deadline"), Table->GetBettingRemainingTime(), 10.0f);
	TestEqual(TEXT("Another keypad does not charge its player"), Players[1]->GetCurrency(), 10000.0f);

	// Let world time pass without ticking the timer to exercise the expired-deadline
	// guard itself, before its timer closes betting or starts the round.
	World->TimeSeconds += 11.0;
	TestTrue(TEXT("Deadline fixture has not yet run its betting timer"), Table->IsBettingWindowOpen());
	TestNull(TEXT("Expired betting deadline blocks seat keypad lookup"), Table->GetBettingInteractionTarget(Players[1]));
	OwnChips[1]->Interact(Players[1]);
	TestEqual(TEXT("A rejected expired interaction preserves currency"), Players[1]->GetCurrency(), 10000.0f);
	Table->ResetRound();
	RoundProperty->GetUnderlyingProperty()->SetIntPropertyValue(
		RoundProperty->ContainerPtrToValuePtr<void>(Table), static_cast<uint64>(EBlackjackRoundState::Insurance));
	TestNull(TEXT("Insurance phase cannot open the normal betting keypad"), Table->GetBettingInteractionTarget(Players[1]));
	Table->ResetRound();
	ACasinoLoopGameState* State = World->SpawnActor<ACasinoLoopGameState>();
	if (!TestNotNull(TEXT("Loop game state"), State)) { return false; }
	World->SetGameState(State);
	State->LoopStatus.Phase = ECasinoLoopPhase::Settling;
	TestNull(TEXT("Day-end payment phase cannot resolve a gameplay betting target"), Table->GetBettingInteractionTarget(Players[1]));
	State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
	TestTrue(TEXT("Playing phase preserves the normal own-seat betting target"),
		Table->GetBettingInteractionTarget(Players[1]) == OwnChips[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackSeatViewLifecycleTest,
	"Casino.Blackjack.SeatViewLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackSeatViewLifecycleTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
		TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
	if (!TestNotNull(TEXT("Playable character class"), CharacterClass)
		|| !TestNotNull(TEXT("Playable controller class"), ControllerClass)) { return false; }

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACasinoLoopGameState* State = World->SpawnActor<ACasinoLoopGameState>();
	ABlackjackTableActor* Table = World->SpawnActor<ABlackjackTableActor>();
	if (!TestNotNull(TEXT("Loop game state"), State) || !TestNotNull(TEXT("Table"), Table)) { return false; }
	World->SetGameState(State);
	State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
	Table->DispatchBeginPlay();

	Acasino_simulatorCharacter* Players[2];
	Acasino_simulatorPlayerController* Controllers[2];
	ABlackjackSeatInteractionActor* Seats[2];
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Players[Index] = World->SpawnActor<Acasino_simulatorCharacter>(CharacterClass,
			FVector(Index * 50.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Spawn);
		Controllers[Index] = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass,
			FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
		Seats[Index] = World->SpawnActor<ABlackjackSeatInteractionActor>(
			ABlackjackSeatInteractionActor::StaticClass(), Table->GetSeatPoint(Index)->GetComponentLocation(),
			Table->GetSeatPoint(Index)->GetComponentRotation(), Spawn);
		if (!TestNotNull(TEXT("Player"), Players[Index]) || !TestNotNull(TEXT("Controller"), Controllers[Index])
			|| !TestNotNull(TEXT("Seat"), Seats[Index])) { return false; }
		auto* ASC = Players[Index]->GetAbilitySystemComponent();
		ASC->InitAbilityActorInfo(Players[Index], Players[Index]);
		ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
		ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 10000.0f);
		Controllers[Index]->Player = NewObject<ULocalPlayer>(GEngine);
		Controllers[Index]->Player->PlayerController = Controllers[Index];
		Controllers[Index]->SetAsLocalPlayerController();
		Controllers[Index]->UIScreen = ECasinoUIScreen::Playing;
		Controllers[Index]->UIManager->UIScreen = ECasinoUIScreen::Playing;
		// Isolated worlds skip controller PostInitializeComponents; explicitly create
		// the same camera manager the normal engine initialization would create.
		Controllers[Index]->SpawnPlayerCameraManager();
		if (!TestNotNull(TEXT("Local player camera manager"), Controllers[Index]->PlayerCameraManager.Get())) { return false; }
		Controllers[Index]->Possess(Players[Index]);
		Controllers[Index]->SetViewTarget(Players[Index]);
		Players[Index]->GetMesh()->SetVisibility(Index != 0, true);
		Players[Index]->GetFirstPersonMesh()->SetVisibility(true, true);
		Seats[Index]->AttachToActor(Table, FAttachmentTransformRules::KeepWorldTransform);
		Seats[Index]->DispatchBeginPlay();
		Seats[Index]->Interact(Players[Index]);
		TestTrue(TEXT("Seat camera starts immediately on successful seating"),
			Players[Index]->GetBlackjackPlayerComponent()->HasLocalSeatView());
		TestTrue(TEXT("Local player uses the assigned seat view actor"), Controllers[Index]->GetViewTarget() == Seats[Index]);
		TestTrue(TEXT("Seating immediately locks look input"), Controllers[Index]->IsLookInputIgnored());
		TestFalse(TEXT("Seating alone does not open an interaction UI"), Controllers[Index]->IsInteractionUIOpen());
		TestFalse(TEXT("Seating alone does not show the mouse cursor"), Controllers[Index]->bShowMouseCursor);
		TestFalse(TEXT("Seating hides the player's first-person body"), Players[Index]->GetFirstPersonMesh()->IsVisible());
		TestFalse(TEXT("Seating hides the player's world body in its local view"), Players[Index]->GetMesh()->IsVisible());
		TestTrue(TEXT("Seating disables walking"), Players[Index]->GetCharacterMovement()->MovementMode == MOVE_None);
	}
	TestTrue(TEXT("Two local players keep distinct view targets"), Controllers[0]->GetViewTarget() != Controllers[1]->GetViewTarget());
	auto* Component = Players[0]->GetBlackjackPlayerComponent();
	for (int32 Refresh = 0; Refresh < 3; ++Refresh)
	{
		TestTrue(TEXT("Repeated seated-view refresh succeeds"), Component->RefreshLocalSeatView());
	}
	TestTrue(TEXT("Another player's seat cannot replace the first player's camera"), Controllers[0]->GetViewTarget() == Seats[0]);
	FStructProperty* SessionProperty = FindFProperty<FStructProperty>(Component->GetClass(), TEXT("SeatSession"));
	UFunction* SeatNotify = Component->FindFunction(TEXT("OnRep_BlackjackSeatMode"));
	if (!TestNotNull(TEXT("Replicated seat snapshot"), SessionProperty)
		|| !TestNotNull(TEXT("Seat RepNotify"), SeatNotify)) { return false; }
	FBlackjackSeatSession* Session = SessionProperty->ContainerPtrToValuePtr<FBlackjackSeatSession>(Component);
	// Exercise client presentation through the exact RepNotify path without
	// changing the authoritative table occupant or starting a multiplayer session.
	*Session = FBlackjackSeatSession();
	Component->ProcessEvent(SeatNotify, nullptr);
	Component->ProcessEvent(SeatNotify, nullptr);
	TestFalse(TEXT("A replicated seat removal releases the camera"), Component->HasLocalSeatView());
	TestFalse(TEXT("Duplicate replicated removal does not retain a look lock"), Controllers[0]->IsLookInputIgnored());
	Session->Table = Table;
	Session->SeatIndex = 0;
	Component->ProcessEvent(SeatNotify, nullptr);
	Component->ProcessEvent(SeatNotify, nullptr);
	TestTrue(TEXT("A replicated seat assignment starts its camera"), Component->HasLocalSeatView());
	TestTrue(TEXT("Replicated seat assignment uses its own seat target"), Controllers[0]->GetViewTarget() == Seats[0]);

	// An existing betting widget can pass its old shared camera target. The seat
	// retains its own view throughout UI open and close without releasing occupancy.
	Controllers[0]->EnterInteractionUIMode(Seats[1], 0.0f);
	TestTrue(TEXT("Interaction UI opens while seated"), Controllers[0]->IsInteractionUIOpen());
	TestTrue(TEXT("Interaction UI shows its mouse cursor"), Controllers[0]->bShowMouseCursor);
	TestTrue(TEXT("Interaction UI preserves this player's seat camera"), Controllers[0]->GetViewTarget() == Seats[0]);
	Controllers[0]->ExitInteractionUIMode(0.0f);
	TestFalse(TEXT("Closing betting UI clears only the UI-open state"), Controllers[0]->IsInteractionUIOpen());
	TestFalse(TEXT("Closing betting UI hides its mouse cursor"), Controllers[0]->bShowMouseCursor);
	TestTrue(TEXT("Closing betting UI preserves the seated camera"), Controllers[0]->GetViewTarget() == Seats[0]);
	TestTrue(TEXT("Closing betting UI preserves the seated look lock"), Controllers[0]->IsLookInputIgnored());
	TestFalse(TEXT("Closing betting UI keeps the first-person body hidden"), Players[0]->GetFirstPersonMesh()->IsVisible());
	TestTrue(TEXT("Closing betting UI does not release the table seat"), Table->GetSeatIndexForPlayer(Players[0]) == 0);
	TestTrue(TEXT("The neighboring local player's camera is unchanged"), Controllers[1]->GetViewTarget() == Seats[1]);
	Controllers[0]->EnterInteractionUIMode(Seats[1], 0.0f);
	TestTrue(TEXT("Interaction UI is open before managed cleanup"), Controllers[0]->IsInteractionUIOpen());
	Controllers[0]->UIManager->ClearInteractionWidgets();
	TestFalse(TEXT("Managed widget cleanup clears the active interaction UI"), Controllers[0]->IsInteractionUIOpen());
	TestTrue(TEXT("Managed widget cleanup preserves the seat camera"), Controllers[0]->GetViewTarget() == Seats[0]);
	TestTrue(TEXT("Managed cleanup removes only UI-owned look locks"), Controllers[0]->IsLookInputIgnored());
	TestFalse(TEXT("Managed widget cleanup cannot expose the seated body"), Players[0]->GetFirstPersonMesh()->IsVisible());

	Component->CompleteExitBlackjackSeat();
	TestFalse(TEXT("Completed exit releases the local seat view"), Component->HasLocalSeatView());
	TestTrue(TEXT("Completed exit restores the pawn view"), Controllers[0]->GetViewTarget() == Players[0]);
	TestFalse(TEXT("Exit balances look locks after UI and repeated refreshes"), Controllers[0]->IsLookInputIgnored());
	TestFalse(TEXT("Completed exit restores move input"), Controllers[0]->IsMoveInputIgnored());
	TestTrue(TEXT("Completed exit restores walking"), Players[0]->GetCharacterMovement()->MovementMode == MOVE_Walking);
	TestTrue(TEXT("Completed exit restores the previously visible first-person mesh"), Players[0]->GetFirstPersonMesh()->IsVisible());
	TestFalse(TEXT("Completed exit preserves a previously hidden world mesh"), Players[0]->GetMesh()->IsVisible());
	TestEqual(TEXT("Completed exit releases table occupancy"), Table->GetSeatIndexForPlayer(Players[0]), INDEX_NONE);
	TestTrue(TEXT("One player's exit keeps the neighboring player seated"), Players[1]->GetBlackjackPlayerComponent()->HasLocalSeatView());
	TestTrue(TEXT("One player's exit cannot restore the neighboring camera"), Controllers[1]->GetViewTarget() == Seats[1]);
	Players[1]->GetBlackjackPlayerComponent()->CompleteExitBlackjackSeat();
	TestTrue(TEXT("Neighbor exit restores its original world mesh visibility"), Players[1]->GetMesh()->IsVisible());
	TestFalse(TEXT("Neighbor exit releases its look lock"), Controllers[1]->IsLookInputIgnored());

	TestTrue(TEXT("A player can claim a seat before its presentation actor arrives"), Table->TryClaimSeat(Players[0], 2));
	Component->EnterBlackjackSeatMode(Table, 2);
	TestFalse(TEXT("Missing seat camera does not report an applied local view"), Component->HasLocalSeatView());
	TestTrue(TEXT("Missing camera preserves the seated gameplay state"), Component->IsInBlackjackSeat());
	ABlackjackSeatInteractionActor* LateSeat = World->SpawnActor<ABlackjackSeatInteractionActor>(
		ABlackjackSeatInteractionActor::StaticClass(), Table->GetSeatPoint(2)->GetComponentLocation(),
		Table->GetSeatPoint(2)->GetComponentRotation(), Spawn);
	if (!TestNotNull(TEXT("Late seat camera actor"), LateSeat)) { return false; }
	LateSeat->AttachToActor(Table, FAttachmentTransformRules::KeepWorldTransform);
	LateSeat->DispatchBeginPlay();
	TestTrue(TEXT("The view refresh retries a late camera successfully"), Component->RefreshLocalSeatView());
	TestTrue(TEXT("The retried view targets the newly available seat camera"), Controllers[0]->GetViewTarget() == LateSeat);
	Component->CompleteExitBlackjackSeat();
	TestFalse(TEXT("Exit after a late camera balances the look lock"), Controllers[0]->IsLookInputIgnored());

	Seats[0]->Interact(Players[0]);
	Controllers[0]->UIManager->ApplyUIScreenInput();
	TestTrue(TEXT("Playing input reset reapplies the active seated view"), Component->HasLocalSeatView());
	TestTrue(TEXT("Playing input reset preserves the player's seat camera"), Controllers[0]->GetViewTarget() == Seats[0]);
	TestTrue(TEXT("Playing input reset reapplies the seated look lock"), Controllers[0]->IsLookInputIgnored());
	// Payment owns a new look lock after discarding all prior input counts. A late
	// seat removal must not consume that new lock or restore the gameplay camera.
	Controllers[0]->SetViewTarget(Seats[1]);
	Controllers[0]->UIScreen = ECasinoUIScreen::Payment;
	Controllers[0]->UIManager->UIScreen = ECasinoUIScreen::Payment;
	State->LoopStatus.Phase = ECasinoLoopPhase::Settling;
	Controllers[0]->UIManager->ApplyUIScreenInput();
	TestFalse(TEXT("Payment input reset releases the seated view"), Component->HasLocalSeatView());
	TestTrue(TEXT("Payment input reset retains its own look lock"), Controllers[0]->IsLookInputIgnored());
	TestTrue(TEXT("Payment input reset retains its own move lock"), Controllers[0]->IsMoveInputIgnored());
	TestTrue(TEXT("Payment input reset preserves the higher-priority view"), Controllers[0]->GetViewTarget() == Seats[1]);
	*Session = FBlackjackSeatSession();
	Component->ProcessEvent(SeatNotify, nullptr);
	TestTrue(TEXT("Late replicated seat removal preserves the payment look lock"), Controllers[0]->IsLookInputIgnored());
	TestTrue(TEXT("Late replicated seat removal preserves the payment move lock"), Controllers[0]->IsMoveInputIgnored());
	TestTrue(TEXT("Late replicated seat removal preserves the payment camera"), Controllers[0]->GetViewTarget() == Seats[1]);
	Table->EndCasinoDay_Implementation();
	State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
	Controllers[0]->UIScreen = ECasinoUIScreen::Playing;
	Controllers[0]->UIManager->UIScreen = ECasinoUIScreen::Playing;
	Controllers[0]->UIManager->ApplyUIScreenInput();
	TestFalse(TEXT("Returning to Playing removes the payment look lock"), Controllers[0]->IsLookInputIgnored());

	Seats[0]->Interact(Players[0]);
	if (!TestTrue(TEXT("Player is seated for cinematic lock fixture"), Component->HasLocalSeatView())) { return false; }
	Controllers[0]->BeginPoliceArrival();
	if (!TestTrue(TEXT("Public police arrival starts cinematic state"), Controllers[0]->IsPoliceCinematicActive())) { return false; }
	Controllers[0]->SetIgnoreLookInput(true);
	Controllers[0]->SetViewTarget(Seats[1]);
	Component->CompleteExitBlackjackSeat();
	TestFalse(TEXT("Exiting during a cinematic releases the seat view"), Component->HasLocalSeatView());
	TestTrue(TEXT("Seat exit preserves the cinematic's separate look lock"), Controllers[0]->IsLookInputIgnored());
	TestTrue(TEXT("Seat exit cannot overwrite the cinematic view"), Controllers[0]->GetViewTarget() == Seats[1]);
	// Check counted ownership before the stop event can reset input from Blueprint.
	Controllers[0]->SetIgnoreLookInput(false);
	TestFalse(TEXT("Removing the cinematic lock leaves no leaked seat look count"), Controllers[0]->IsLookInputIgnored());
	Controllers[0]->EndPoliceCinematic();
	TestFalse(TEXT("Public cinematic completion clears cinematic state"), Controllers[0]->IsPoliceCinematicActive());
	return true;
}

#endif
