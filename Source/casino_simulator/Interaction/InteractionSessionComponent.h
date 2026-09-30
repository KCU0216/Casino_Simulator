#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionSessionComponent.generated.h"

class Acasino_simulatorCharacter;

DECLARE_MULTICAST_DELEGATE_OneParam(FInteractionSessionUserEvent, Acasino_simulatorCharacter*);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UInteractionSessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionSessionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static bool CanRestoreMovement(const Acasino_simulatorCharacter* Character);
	static void RestoreMovementAfterUse(Acasino_simulatorCharacter* Character);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Interaction Session")
	bool TryJoin(Acasino_simulatorCharacter* User);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Interaction Session")
	bool TryLeave(Acasino_simulatorCharacter* User);

	UFUNCTION(BlueprintPure, Category = "Interaction Session")
	bool ContainsUser(const Acasino_simulatorCharacter* User) const;

	UFUNCTION(BlueprintPure, Category = "Interaction Session")
	bool HasCapacity() const;

	UFUNCTION(BlueprintPure, Category = "Interaction Session")
	bool IsSessionActive() const { return !Users.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Interaction Session")
	int32 GetUserCount() const { return Users.Num(); }

	const TArray<TObjectPtr<Acasino_simulatorCharacter>>& GetUsers() const { return Users; }

	FInteractionSessionUserEvent OnUserJoined;
	FInteractionSessionUserEvent OnUserLeft;

private:
	/** One for exclusive use, zero for unlimited use, or any positive concurrent-user limit. */
	UPROPERTY(EditAnywhere, Replicated, Category = "Interaction Session", meta = (ClampMin = "0"))
	int32 MaxUsers = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Users)
	TArray<TObjectPtr<Acasino_simulatorCharacter>> Users;

	TArray<TWeakObjectPtr<Acasino_simulatorCharacter>> NotifiedUsers;

	UFUNCTION()
	void OnRep_Users();

	void RefreshNotifications();
	void ForceOwnerNetUpdate() const;
};
