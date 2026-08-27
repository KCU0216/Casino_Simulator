// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dice.generated.h"

class UStaticMeshComponent;

/** Broadcast with the final pip count (1-6) once a roll has settled: on the server the instant it decides, and on each client once ResultFace replicates in. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDiceRolled, int32, Face);

/**
 * A physically-simulated die whose outcome is still server-authoritative and guaranteed.
 *
 * Roll() (server-only) applies a real angular impulse so it spins in place naturally (no throw/hop -
 * it stays put and just rotates). Once its velocity
 * drops below a "settling" threshold, physics is switched off and the rotation is blended over
 * SettleDuration seconds onto the exact orientation that puts DesiredFace's pip facing up (plus a
 * random spin around the up axis so it doesn't look identical every time). Only the server runs
 * this logic; clients just see the resulting transform through normal actor movement replication,
 * and read the authoritative result off ResultFace/OnDiceRolled.
 *
 * FaceUpRotations must be calibrated per-mesh in the editor (see comment above the property) before
 * this will land on the correct face.
 */
UCLASS()
class CASINO_SIMULATOR_API ADice : public AActor
{
	GENERATED_BODY()

public:
	ADice();

	/**
	 * Starts a roll. Server-only (no-ops on clients).
	 * @param DesiredFace Pip count 1-6 to guarantee as the result, or 0 to pick uniformly at random.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dice")
	void Roll(int32 DesiredFace = 0);

	/** True while a roll is in flight (tumbling or settling) and hasn't produced a result yet. */
	UFUNCTION(BlueprintPure, Category = "Dice")
	bool IsRolling() const { return PendingFace != 0; }

	/** Fired with the final pip count: on the server the moment it's decided, and on each client via OnRep once it replicates in. Bind UI/SFX here instead of polling ResultFace. */
	UPROPERTY(BlueprintAssignable, Category = "Dice")
	FOnDiceRolled OnDiceRolled;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> DiceMesh;

	/**
	 * Local-space rotation that puts each face's pip directly facing +Z (world up), keyed 1-6.
	 * Calibrate in-editor: temporarily rotate a DiceMesh instance by hand until a given pip faces
	 * straight up, then copy that Rotator in here for that face number. Must be filled in (defaults
	 * to identity for every face, which is almost certainly wrong for a real die mesh).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Dice")
	TMap<int32, FRotator> FaceUpRotations;

	/** Linear speed (cm/s) below which the die is considered to have stopped tumbling and settling can begin. */
	UPROPERTY(EditDefaultsOnly, Category = "Dice")
	float SettleLinearSpeedThreshold = 25.0f;

	/** Angular speed (deg/s) below which the die is considered to have stopped tumbling and settling can begin. */
	UPROPERTY(EditDefaultsOnly, Category = "Dice")
	float SettleAngularSpeedThreshold = 200.0f;

	/** How long (seconds) the corrective blend from wherever it stopped to the exact target face-up rotation takes. */
	UPROPERTY(EditDefaultsOnly, Category = "Dice")
	float SettleDuration = 0.25f;

	/** Angular impulse strength range applied on Roll(), in deg/s. */
	UPROPERTY(EditDefaultsOnly, Category = "Dice")
	FVector2D AngularImpulseRange = FVector2D(500.0f, 1000.0f);

	/** Authoritative result of the last completed roll (0 = no result yet / currently rolling). */
	UPROPERTY(ReplicatedUsing = OnRep_ResultFace, BlueprintReadOnly, Category = "Dice")
	int32 ResultFace = 3;

	UFUNCTION()
	void OnRep_ResultFace();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Face we're rolling towards; 0 once settled/idle. Server-only state (not replicated - clients only ever see the final ResultFace). */
	int32 PendingFace = 0;

	bool bSettling = false;
	FQuat SettleStartRotation = FQuat::Identity;
	FQuat SettleTargetRotation = FQuat::Identity;
	float SettleElapsed = 0.0f;

	/** Looks up FaceUpRotations, warning once and falling back to identity if a face is missing. */
	FRotator GetFaceUpRotation(int32 Face) const;
};
