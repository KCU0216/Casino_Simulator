// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MachineInteractionComponent.generated.h"

class Acasino_simulatorCharacter;

DECLARE_DELEGATE_OneParam(FMachineInteractionPlayerEvent, Acasino_simulatorCharacter*);

/**
 * Shared RPC scaffolding for the "use machine" half of AWorldInteractableBase/ANPC_Base's Interact()
 * flow: the Server_RequestUseMachine -> Multicast_MachineUseStarted round trip plus the
 * movement-disable line, which used to be copy-pasted identically in both classes.
 *
 * Deliberately doesn't own any "who's using me" state - InteractingPlayer/OverlappingPlayer and their
 * Handle*() reactions stay exactly where they were (on AWorldInteractableBase/ANPC_Base and their own
 * subclasses - e.g. ASeatedMachineBase overrides the Handle* virtuals, AThreeCardPokerTableActor reads
 * InteractingPlayer directly). The owner instead binds OnRequestUseMachine/OnUseStarted (typically in
 * its constructor) to its own methods; binding a pointer-to-virtual-member-function still dispatches
 * through the vtable, so existing subclass overrides keep firing exactly as before.
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UMachineInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMachineInteractionComponent();

	/** Fired server-only, before the multicast goes out - mirrors the old
	 * Server_RequestUseMachine_Implementation's call to HandleMachineRequestUseMachine. */
	FMachineInteractionPlayerEvent OnRequestUseMachine;

	/** Fired on every machine (server + all clients) once Multicast_MachineUseStarted lands - mirrors
	 * the old Multicast_MachineUseStarted_Implementation body. */
	FMachineInteractionPlayerEvent OnUseStarted;

	/** Entry point mirroring the old Interact() body: calls the request flow directly when the owner
	 * already has authority (Interact() is only ever reached with authority already - see
	 * IWorldInteractable's class comment), otherwise routes through the Server RPC. */
	void RequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter);

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MachineUseStarted(Acasino_simulatorCharacter* RequestingCharacter);
};
