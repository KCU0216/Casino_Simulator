#pragma once

#include "CoreMinimal.h"
#include "Interaction/WorldInteractableBase.h"
#include "SeatedMachineBase.generated.h"

class Acasino_simulatorCharacter;
class UCameraComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESeatedMachineUseResult : uint8
{
	Accepted UMETA(DisplayName = "Accepted"),
	AlreadyOccupied UMETA(DisplayName = "Already Occupied"),
	InvalidUser UMETA(DisplayName = "Invalid User")
};

/**
 * Base actor for machines that a single player operates while seated.
 * Child classes own the actual game rules; this class only owns occupancy,
 * seat/camera points, and the server-authoritative use/release flow.
 */
UCLASS(Abstract, Blueprintable)
class CASINO_SIMULATOR_API ASeatedMachineBase : public AWorldInteractableBase
{
	GENERATED_BODY()

public:
	ASeatedMachineBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	virtual void Interact(Acasino_simulatorCharacter* RequestingCharacter) override;

	
	UFUNCTION(BlueprintCallable, Category = "Machine|Interaction")
	void RequestReleaseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	UFUNCTION(BlueprintCallable, Category = "Machine|Interaction")
	void HandleMachinePrimaryInput(Acasino_simulatorCharacter* RequestingCharacter);

	UFUNCTION(BlueprintCallable, Category = "Machine|Interaction")
	void SetCanExitMachine(bool bCanExit);

	UFUNCTION(BlueprintPure, Category = "Machine|Interaction")
	bool CanExitMachine() const { return bCanExitMachine; }

	
	UFUNCTION(BlueprintPure, Category = "Machine|State")
	bool IsOccupied() const { return CurrentUser != nullptr; }

	
	UFUNCTION(BlueprintPure, Category = "Machine|State")
	bool CanOperate(Acasino_simulatorCharacter* RequestingCharacter) const;

	virtual bool CanInteract(Acasino_simulatorCharacter* RequestingCharacter) const override;

	UFUNCTION(BlueprintPure, Category = "Machine|State")
	Acasino_simulatorCharacter* GetCurrentUser() const { return CurrentUser; }

	UFUNCTION(BlueprintPure, Category = "Machine|Seat")
	USceneComponent* GetSeatPoint() const { return SeatPoint; }

	UFUNCTION(BlueprintPure, Category = "Machine|Seat")
	USceneComponent* GetCameraPoint() const { return CameraPoint; }

protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<UStaticMeshComponent> ChairMesh;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<USceneComponent> SeatPoint;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<USceneComponent> CameraPoint;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine|Components")
	TObjectPtr<UCameraComponent> MachineCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine|Seat")
	bool bMoveUserToSeatOnUse = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine|Seat")
	bool bDisableUserMovementOnUse = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine|Seat", meta = (ClampMin = "0.0"))
	float MachineCameraBlendTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Machine|Seat", meta = (ClampMin = "0.0"))
	float ReleaseCameraBlendTime = 0.25f;

	
	UPROPERTY(ReplicatedUsing = OnRep_CurrentUser, BlueprintReadOnly, Category = "Machine|State")
	TObjectPtr<Acasino_simulatorCharacter> CurrentUser;


	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Machine|State")
	bool bCanOperate = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Machine|State")
	bool bCanExitMachine = true;


	UFUNCTION(Server, Reliable)
	void Server_RequestUseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	
	UFUNCTION(Server, Reliable)
	void Server_ReleaseMachine(Acasino_simulatorCharacter* RequestingCharacter);

	UFUNCTION(Server, Reliable)
	void Server_HandleMachinePrimaryInput(Acasino_simulatorCharacter* RequestingCharacter);

	UFUNCTION(Server, Reliable)
	void Server_SetCanExitMachine(bool bCanExit);

	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MachineUseStarted(Acasino_simulatorCharacter* RequestingCharacter);

	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_MachineReleased(Acasino_simulatorCharacter* ReleasingCharacter);
	

	UFUNCTION()
	void OnRep_CurrentUser();

	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineReady(Acasino_simulatorCharacter* RequestingCharacter);
	virtual void OnMachineReady_Implementation(Acasino_simulatorCharacter* RequestingCharacter);


	
	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineReleased(Acasino_simulatorCharacter* ReleasingCharacter);
	virtual void OnMachineReleased_Implementation(Acasino_simulatorCharacter* ReleasingCharacter);

	
	UFUNCTION(BlueprintNativeEvent, Category = "Machine|State")
	void OnMachineUseRejected(Acasino_simulatorCharacter* RequestingCharacter, ESeatedMachineUseResult Result);
	virtual void OnMachineUseRejected_Implementation(Acasino_simulatorCharacter* RequestingCharacter, ESeatedMachineUseResult Result);

	UFUNCTION(BlueprintNativeEvent, Category = "Machine|Input")
	void OnMachinePrimaryInput(Acasino_simulatorCharacter* RequestingCharacter);
	virtual void OnMachinePrimaryInput_Implementation(Acasino_simulatorCharacter* RequestingCharacter);

	UFUNCTION(BlueprintNativeEvent, Category = "Machine|Interaction")
	void OnMachineExitRejected(Acasino_simulatorCharacter* RequestingCharacter);
	virtual void OnMachineExitRejected_Implementation(Acasino_simulatorCharacter* RequestingCharacter);

private:

	ESeatedMachineUseResult CanAcceptUser(Acasino_simulatorCharacter* RequestingCharacter) const;

	void EnterMachineUseView(Acasino_simulatorCharacter* RequestingCharacter);
	void ExitMachineUseView(Acasino_simulatorCharacter* ReleasingCharacter);
};
