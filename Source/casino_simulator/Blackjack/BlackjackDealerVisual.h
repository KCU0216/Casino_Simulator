// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "BlackjackDealerVisual.generated.h"

class ABlackjackTableActor;
class UAnimMontage;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UTexture2D;

/** Cosmetic dealer for a blackjack table; it has no NPC interaction or player state. */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API ABlackjackDealerVisual : public AActor
{
	GENERATED_BODY()

public:
	ABlackjackDealerVisual();

	/** Keeps the inherited Character mesh name so existing dealer BP templates can migrate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	TObjectPtr<UStaticMeshComponent> ShuffleLeftDeck;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	TObjectPtr<UStaticMeshComponent> ShuffleRightDeck;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	TObjectPtr<UStaticMeshComponent> ShuffleLeftDeckBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	TObjectPtr<UStaticMeshComponent> ShuffleRightDeckBack;

	UFUNCTION(BlueprintPure, Category="Blackjack|Dealer")
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

	/** Same inputs and return value as Character::PlayAnimMontage for existing BP calls. */
	UFUNCTION(BlueprintCallable, Category="Blackjack|Dealer")
	float PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate = 1.0f, FName StartSectionName = NAME_None);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Dealer")
	float PlayDealMontage();

	UFUNCTION(BlueprintCallable, Category="Blackjack|Dealer")
	float PlayShuffleMontage();

	/** Cosmetic only: neither the release notify nor its fallback timer changes the hand. */
	UFUNCTION(BlueprintCallable, Category="Blackjack|Dealer")
	void PresentCard(AActor* CardActor, const FTransform& TargetTransform, float Duration = 0.25f, int32 SeatIndex = -1);

	UFUNCTION(BlueprintCallable, Category="Blackjack|Dealer")
	void ReleasePresentedCard();

	UFUNCTION(BlueprintPure, Category="Blackjack|Dealer")
	float GetDealPresentationDuration(float FlightDuration = 0.25f) const;

	UFUNCTION(BlueprintCallable, Category="Blackjack|Dealer")
	void StopDealerAnimation(float BlendOutTime = 0.15f);

	/** Uses the configured card socket, with the right hand as a fallback. */
	UFUNCTION(BlueprintPure, Category="Blackjack|Dealer")
	FTransform GetDealSocketTransform() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer")
	TObjectPtr<UAnimMontage> DealMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer")
	TObjectPtr<UAnimMontage> ShuffleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer", meta=(ClampMin="0.01"))
	float DealPlayRate = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer")
	FName DealSocketName = TEXT("Socket_CardDeal_R");

	/** Local adjustment when the target skeleton has hand_r but no card socket yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer")
	FTransform CardHandOffset = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Facing")
	bool bFaceDealtSeat = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Facing", meta=(ClampMin="0.0", Units="s"))
	float SeatFacingBlendSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Facing", meta=(ClampMin="0.0", ClampMax="180.0", Units="deg"))
	float MaxSeatFacingYaw = 60.0f;

	/** Small corrections for each seat, added to its target bearing before clamping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Facing")
	TArray<float> SeatFacingYawOffsets = {0.0f, 0.0f, 0.0f, 0.0f};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	bool bShowShuffleDecks = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	TObjectPtr<UStaticMesh> ShuffleDeckMesh;

	/** Paper edge/body material; top planes use the separate card back material. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	TObjectPtr<UMaterialInterface> ShuffleDeckMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	TObjectPtr<UMaterialInterface> ShuffleCardBackMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	TObjectPtr<UTexture2D> ShuffleBackTexture;

	/** Engine cube is 100 cm: these scales create a 6.3 x 8.8 x 0.7 cm half deck. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	FTransform ShuffleLeftDeckOffset = FTransform(FRotator::ZeroRotator, FVector(4.0f, -2.0f, 0.0f), FVector(0.063f, 0.088f, 0.007f));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle")
	FTransform ShuffleRightDeckOffset = FTransform(FRotator::ZeroRotator, FVector(4.0f, 2.0f, 0.0f), FVector(0.063f, 0.088f, 0.007f));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ShufflePropTakeAttachFraction = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blackjack|Dealer|Shuffle", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ShufflePropInBoxDetachFraction = 0.25f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

private:
	ABlackjackTableActor* ResolveBlackjackTable() const;
	void CancelPresentedCard();
	float GetDealReleaseDelay() const;
	void CaptureBaseMeshRotation();
	void FaceCardTarget(const FTransform& TargetTransform, int32 SeatIndex);
	void SetFacingOffset(float YawOffset);
	void UpdateFacing();
	void RestoreBaseFacing(bool bImmediate);
	void ConfigureShuffleDecks();
	void GetShufflePropWindow(float& OutShowTime, float& OutHideTime) const;
	void ShowShuffleDecks();
	void HideShuffleDecks();

	UFUNCTION()
	void HandleDealerShuffle();

	UFUNCTION()
	void HandleDealerPresentationReset();

	TWeakObjectPtr<ABlackjackTableActor> BoundTable;
	TWeakObjectPtr<AActor> PresentedCard;
	FTransform PresentedCardTarget = FTransform::Identity;
	float PresentedCardFlightDuration = 0.25f;
	bool bPresentedCardPending = false;
	FTimerHandle CardReleaseTimer;
	FTimerHandle FacingTimer;
	FTimerHandle ShuffleShowTimer;
	FTimerHandle ShuffleHideTimer;
	FTimerHandle ShuffleEndTimer;
	FRotator BaseMeshRotation = FRotator::ZeroRotator;
	bool bBaseMeshRotationCaptured = false;
	float FacingStartYaw = 0.0f;
	float FacingTargetYaw = 0.0f;
	double FacingStartTime = 0.0;
};
