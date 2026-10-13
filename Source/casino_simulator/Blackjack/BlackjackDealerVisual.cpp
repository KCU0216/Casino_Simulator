#include "Blackjack/BlackjackDealerVisual.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Blackjack/BlackjackTableActor.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
	bool DealCardThroughBlueprint(AActor* CardActor, const FTransform& TargetTransform, float Duration)
	{
		UFunction* Function = CardActor->FindFunction(TEXT("DealTo"));
		FStructProperty* Target = Function ? FindFProperty<FStructProperty>(Function, TEXT("TargetTransform")) : nullptr;
		FNumericProperty* FlightDuration = Function ? FindFProperty<FNumericProperty>(Function, TEXT("Duration")) : nullptr;
		FBoolProperty* InterpolateRotation = Function ? FindFProperty<FBoolProperty>(Function, TEXT("isRot")) : nullptr;
		if (!Target || Target->Struct != TBaseStructure<FTransform>::Get() || !FlightDuration
			|| !FlightDuration->IsFloatingPoint()
			|| !Target->HasAnyPropertyFlags(CPF_Parm) || Target->HasAnyPropertyFlags(CPF_ReturnParm)
			|| !FlightDuration->HasAnyPropertyFlags(CPF_Parm) || FlightDuration->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			return false;
		}
		if (InterpolateRotation && (!InterpolateRotation->HasAnyPropertyFlags(CPF_Parm)
			|| InterpolateRotation->HasAnyPropertyFlags(CPF_ReturnParm)))
		{
			return false;
		}

		int32 ParameterCount = 0;
		for (TFieldIterator<FProperty> Property(Function); Property; ++Property)
		{
			if (Property->HasAnyPropertyFlags(CPF_Parm))
			{
				++ParameterCount;
				if (*Property != Target && *Property != FlightDuration && *Property != InterpolateRotation)
				{
					return false;
				}
			}
		}
		if (ParameterCount != 2 + (InterpolateRotation ? 1 : 0))
		{
			return false;
		}

		FStructOnScope Parameters(Function);
		Target->CopyCompleteValue(Target->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()), &TargetTransform);
		FlightDuration->SetFloatingPointPropertyValue(
			FlightDuration->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()), Duration);
		if (InterpolateRotation)
		{
			// The production card graphs use false: rotate to the target while flying.
			InterpolateRotation->SetPropertyValue_InContainer(Parameters.GetStructMemory(), false);
		}
		CardActor->ProcessEvent(Function, Parameters.GetStructMemory());
		return true;
	}
}

ABlackjackDealerVisual::ABlackjackDealerVisual()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh0"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetIsReplicated(false);
	// The authoritative table needs release notifies even when the dealer is not rendered.
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	ShuffleDeckMesh = Cube.Object;
	ShuffleLeftDeck = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShuffleLeftDeck"));
	ShuffleRightDeck = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShuffleRightDeck"));
	ShuffleLeftDeckBack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShuffleLeftDeckBack"));
	ShuffleRightDeckBack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShuffleRightDeckBack"));
	ShuffleLeftDeck->SetupAttachment(Mesh, TEXT("hand_l"));
	ShuffleRightDeck->SetupAttachment(Mesh, TEXT("hand_r"));
	ShuffleLeftDeck->SetRelativeTransform(ShuffleLeftDeckOffset);
	ShuffleRightDeck->SetRelativeTransform(ShuffleRightDeckOffset);
	ShuffleLeftDeckBack->SetupAttachment(ShuffleLeftDeck);
	ShuffleRightDeckBack->SetupAttachment(ShuffleRightDeck);
	for (UStaticMeshComponent* Deck : {ShuffleLeftDeck.Get(), ShuffleRightDeck.Get()})
	{
		Deck->SetStaticMesh(Cube.Object);
	}
	for (UStaticMeshComponent* Back : {ShuffleLeftDeckBack.Get(), ShuffleRightDeckBack.Get()})
	{
		Back->SetStaticMesh(Plane.Object);
		// Parent cube scales 100 cm shapes to the real deck dimensions.
		Back->SetRelativeLocation(FVector(0.0f, 0.0f, 50.1f));
	}
	for (UStaticMeshComponent* Prop : {ShuffleLeftDeck.Get(), ShuffleRightDeck.Get(), ShuffleLeftDeckBack.Get(), ShuffleRightDeckBack.Get()})
	{
		Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Prop->SetGenerateOverlapEvents(false);
		Prop->SetIsReplicated(false);
		Prop->SetVisibility(false);
		Prop->SetHiddenInGame(true);
	}
}

void ABlackjackDealerVisual::BeginPlay()
{
	Super::BeginPlay();
	CaptureBaseMeshRotation();
	ConfigureShuffleDecks();
	HideShuffleDecks();

	if (ABlackjackTableActor* Table = ResolveBlackjackTable())
	{
		BoundTable = Table;
		Table->OnDealerShuffle.AddUniqueDynamic(this, &ABlackjackDealerVisual::HandleDealerShuffle);
		Table->OnDealerPresentationReset.AddUniqueDynamic(this, &ABlackjackDealerVisual::HandleDealerPresentationReset);
	}
}

void ABlackjackDealerVisual::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ABlackjackTableActor* Table = BoundTable.Get())
	{
		Table->OnDealerShuffle.RemoveDynamic(this, &ABlackjackDealerVisual::HandleDealerShuffle);
		Table->OnDealerPresentationReset.RemoveDynamic(this, &ABlackjackDealerVisual::HandleDealerPresentationReset);
	}
	BoundTable.Reset();
	StopDealerAnimation(0.0f);
	Super::EndPlay(EndPlayReason);
}

float ABlackjackDealerVisual::PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
	UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	if (!IsValid(AnimMontage) || !IsValid(AnimInstance))
	{
		return 0.0f;
	}

	const float Duration = AnimInstance->Montage_Play(AnimMontage, InPlayRate);
	if (Duration > 0.0f && !StartSectionName.IsNone())
	{
		AnimInstance->Montage_JumpToSection(StartSectionName, AnimMontage);
	}
	return Duration;
}

float ABlackjackDealerVisual::PlayDealMontage()
{
	HideShuffleDecks();
	return PlayAnimMontage(DealMontage, FMath::Max(0.01f, DealPlayRate));
}

float ABlackjackDealerVisual::PlayShuffleMontage()
{
	CancelPresentedCard();
	HideShuffleDecks();
	RestoreBaseFacing(false);
	ConfigureShuffleDecks();
	const float Duration = PlayAnimMontage(ShuffleMontage);
	if (Duration > 0.0f && bShowShuffleDecks && GetWorld())
	{
		float ShowTime = 0.0f;
		float HideTime = 0.0f;
		GetShufflePropWindow(ShowTime, HideTime);
		if (ShowTime <= 0.0f) ShowShuffleDecks();
		else GetWorld()->GetTimerManager().SetTimer(ShuffleShowTimer, this, &ABlackjackDealerVisual::ShowShuffleDecks, ShowTime, false);
		GetWorld()->GetTimerManager().SetTimer(ShuffleHideTimer, this, &ABlackjackDealerVisual::HideShuffleDecks, FMath::Max(0.01f, HideTime), false);
		GetWorld()->GetTimerManager().SetTimer(ShuffleEndTimer, this, &ABlackjackDealerVisual::HideShuffleDecks,
			FMath::Max(0.01f, ShuffleMontage->GetPlayLength()), false);
	}
	return Duration;
}

void ABlackjackDealerVisual::PresentCard(AActor* CardActor, const FTransform& TargetTransform, float Duration, int32 SeatIndex)
{
	if (!IsValid(CardActor) || CardActor == this)
	{
		return;
	}

	// A new visual cannot overwrite a card still held by the previous montage.
	ReleasePresentedCard();
	HideShuffleDecks();
	FaceCardTarget(TargetTransform, SeatIndex);
	PresentedCard = CardActor;
	PresentedCardTarget = TargetTransform;
	PresentedCardFlightDuration = FMath::Max(0.0f, Duration);
	bPresentedCardPending = true;

	if (IsValid(Mesh))
	{
		FName Socket = DealSocketName;
		const bool bUseHandFallback = !Mesh->DoesSocketExist(Socket) && Mesh->DoesSocketExist(TEXT("hand_r"));
		if (bUseHandFallback)
		{
			Socket = TEXT("hand_r");
		}
		else if (!Mesh->DoesSocketExist(Socket))
		{
			Socket = NAME_None;
		}

		if (CardActor->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket)
			&& bUseHandFallback)
		{
			if (USceneComponent* CardRoot = CardActor->GetRootComponent())
			{
				FTransform Offset = CardHandOffset;
				Offset.SetScale3D(CardRoot->GetRelativeScale3D() * CardHandOffset.GetScale3D());
				CardActor->SetActorRelativeTransform(Offset);
			}
		}
	}

	const float MontageDuration = PlayDealMontage();
	if (MontageDuration <= 0.0f || !GetWorld())
	{
		ReleasePresentedCard();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(CardReleaseTimer, this,
		&ABlackjackDealerVisual::ReleasePresentedCard, GetDealReleaseDelay(), false);
}

void ABlackjackDealerVisual::ReleasePresentedCard()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CardReleaseTimer);
	}
	if (!bPresentedCardPending)
	{
		return;
	}

	AActor* CardActor = PresentedCard.Get();
	const FTransform Target = PresentedCardTarget;
	const float FlightDuration = PresentedCardFlightDuration;
	// Clear before invoking Blueprint, which can issue another presentation or a reset.
	bPresentedCardPending = false;
	PresentedCard.Reset();
	if (!IsValid(CardActor))
	{
		return;
	}

	CardActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (!DealCardThroughBlueprint(CardActor, Target, FlightDuration))
	{
		CardActor->SetActorTransform(Target);
	}
}

float ABlackjackDealerVisual::GetDealPresentationDuration(float FlightDuration) const
{
	const float MontageDuration = IsValid(DealMontage)
		? DealMontage->GetPlayLength() / FMath::Max(0.01f, DealPlayRate) : 0.0f;
	return MontageDuration + FMath::Max(0.0f, FlightDuration);
}

float ABlackjackDealerVisual::GetDealReleaseDelay() const
{
	const float Rate = FMath::Max(0.01f, DealPlayRate);
	const float Length = IsValid(DealMontage) ? DealMontage->GetPlayLength() : 0.0f;
	float ReleaseTime = Length * 0.65f;
	if (IsValid(DealMontage))
	{
		for (const FAnimNotifyEvent& Event : DealMontage->Notifies)
		{
			if (IsValid(Event.Notify)
				&& Event.Notify->GetClass()->GetName().Contains(TEXT("AN_Dealer_DealCardRelease")))
			{
				ReleaseTime = FMath::Clamp(Event.GetTriggerTime(), 0.0f, Length);
				break;
			}
		}
	}
	// Give an ordinary animation notify a chance to release before the fallback.
	return FMath::Max(0.01f, ReleaseTime / Rate + 0.05f);
}

void ABlackjackDealerVisual::CancelPresentedCard()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CardReleaseTimer);
	}
	if (AActor* CardActor = PresentedCard.Get())
	{
		CardActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	PresentedCard.Reset();
	bPresentedCardPending = false;
}

void ABlackjackDealerVisual::StopDealerAnimation(float BlendOutTime)
{
	CancelPresentedCard();
	HideShuffleDecks();
	RestoreBaseFacing(BlendOutTime <= 0.0f);
	if (UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Stop(FMath::Max(0.0f, BlendOutTime));
	}
}

void ABlackjackDealerVisual::CaptureBaseMeshRotation()
{
	if (!bBaseMeshRotationCaptured && IsValid(Mesh))
	{
		BaseMeshRotation = Mesh->GetRelativeRotation();
		bBaseMeshRotationCaptured = true;
	}
}

void ABlackjackDealerVisual::FaceCardTarget(const FTransform& TargetTransform, int32 SeatIndex)
{
	float YawOffset = 0.0f;
	if (bFaceDealtSeat && SeatIndex != INDEX_NONE)
	{
		const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(TargetTransform.GetLocation() - GetActorLocation());
		if (!LocalDirection.IsNearlyZero())
		{
			YawOffset = LocalDirection.Rotation().Yaw;
			if (SeatFacingYawOffsets.IsValidIndex(SeatIndex)) YawOffset += SeatFacingYawOffsets[SeatIndex];
		}
	}
	SetFacingOffset(YawOffset);
}

void ABlackjackDealerVisual::SetFacingOffset(float YawOffset)
{
	CaptureBaseMeshRotation();
	if (!IsValid(Mesh)) return;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FacingTimer);
	FacingStartYaw = Mesh->GetRelativeRotation().Yaw;
	FacingTargetYaw = FRotator::NormalizeAxis(BaseMeshRotation.Yaw + FMath::Clamp(YawOffset, -FMath::Abs(MaxSeatFacingYaw), FMath::Abs(MaxSeatFacingYaw)));
	if (!GetWorld() || SeatFacingBlendSeconds <= 0.0f)
	{
		FRotator Rotation = BaseMeshRotation;
		Rotation.Yaw = FacingTargetYaw;
		Mesh->SetRelativeRotation(Rotation);
		return;
	}
	FacingStartTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(FacingTimer, this, &ABlackjackDealerVisual::UpdateFacing,
		FMath::Min(1.0f / 60.0f, SeatFacingBlendSeconds), true);
}

void ABlackjackDealerVisual::UpdateFacing()
{
	if (!GetWorld() || !IsValid(Mesh)) return;
	const float Alpha = SeatFacingBlendSeconds <= 0.0f ? 1.0f
		: FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - FacingStartTime) / SeatFacingBlendSeconds), 0.0f, 1.0f);
	FRotator Rotation = BaseMeshRotation;
	Rotation.Yaw = FRotator::NormalizeAxis(FacingStartYaw + FMath::FindDeltaAngleDegrees(FacingStartYaw, FacingTargetYaw) * Alpha);
	Mesh->SetRelativeRotation(Rotation);
	if (Alpha >= 1.0f) GetWorld()->GetTimerManager().ClearTimer(FacingTimer);
}

void ABlackjackDealerVisual::RestoreBaseFacing(bool bImmediate)
{
	CaptureBaseMeshRotation();
	if (bImmediate)
	{
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(FacingTimer);
		if (IsValid(Mesh)) Mesh->SetRelativeRotation(BaseMeshRotation);
	}
	else SetFacingOffset(0.0f);
}

void ABlackjackDealerVisual::ConfigureShuffleDecks()
{
	if (!IsValid(Mesh)) return;
	for (UStaticMeshComponent* Deck : {ShuffleLeftDeck.Get(), ShuffleRightDeck.Get()})
	{
		if (!IsValid(Deck)) continue;
		if (IsValid(ShuffleDeckMesh)) Deck->SetStaticMesh(ShuffleDeckMesh);
		if (IsValid(ShuffleDeckMaterial)) Deck->SetMaterial(0, ShuffleDeckMaterial);
	}
	if (IsValid(ShuffleLeftDeck)) ShuffleLeftDeck->SetRelativeTransform(ShuffleLeftDeckOffset);
	if (IsValid(ShuffleRightDeck)) ShuffleRightDeck->SetRelativeTransform(ShuffleRightDeckOffset);
	for (UStaticMeshComponent* Back : {ShuffleLeftDeckBack.Get(), ShuffleRightDeckBack.Get()})
	{
		if (!IsValid(Back) || !IsValid(ShuffleCardBackMaterial)) continue;
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Back->GetMaterial(0));
		if (!Material || Material->Parent != ShuffleCardBackMaterial)
			Material = Back->CreateDynamicMaterialInstance(0, ShuffleCardBackMaterial);
		if (Material && IsValid(ShuffleBackTexture)) Material->SetTextureParameterValue(TEXT("BackTexture"), ShuffleBackTexture);
	}
}

void ABlackjackDealerVisual::GetShufflePropWindow(float& OutShowTime, float& OutHideTime) const
{
	const float Length = IsValid(ShuffleMontage) ? ShuffleMontage->GetPlayLength() : 0.0f;
	OutShowTime = Length * 0.1f;
	OutHideTime = Length * 0.9f;
	if (IsValid(ShuffleMontage))
	{
		for (const FSlotAnimationTrack& Track : ShuffleMontage->SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				const UAnimSequenceBase* Animation = Segment.GetAnimReference();
				if (!Animation) continue;
				if (Animation->GetName().Contains(TEXT("Cards_Take")))
					OutShowTime = Segment.StartPos + Segment.GetLength() * FMath::Clamp(ShufflePropTakeAttachFraction, 0.0f, 1.0f);
				if (Animation->GetName().Contains(TEXT("Cards_In_Box")))
					OutHideTime = Segment.StartPos + Segment.GetLength() * FMath::Clamp(ShufflePropInBoxDetachFraction, 0.0f, 1.0f);
			}
		}
	}
	OutShowTime = FMath::Clamp(OutShowTime, 0.0f, Length);
	OutHideTime = FMath::Clamp(OutHideTime, OutShowTime, Length);
}

void ABlackjackDealerVisual::ShowShuffleDecks()
{
	if (!bShowShuffleDecks) return;
	for (UStaticMeshComponent* Prop : {ShuffleLeftDeck.Get(), ShuffleRightDeck.Get(), ShuffleLeftDeckBack.Get(), ShuffleRightDeckBack.Get()})
	{
		if (!IsValid(Prop)) continue;
		Prop->SetVisibility(true);
		Prop->SetHiddenInGame(false);
	}
}

void ABlackjackDealerVisual::HideShuffleDecks()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ShuffleShowTimer);
		GetWorld()->GetTimerManager().ClearTimer(ShuffleHideTimer);
		GetWorld()->GetTimerManager().ClearTimer(ShuffleEndTimer);
	}
	for (UStaticMeshComponent* Prop : {ShuffleLeftDeck.Get(), ShuffleRightDeck.Get(), ShuffleLeftDeckBack.Get(), ShuffleRightDeckBack.Get()})
	{
		if (!IsValid(Prop)) continue;
		Prop->SetVisibility(false);
		Prop->SetHiddenInGame(true);
	}
}

FTransform ABlackjackDealerVisual::GetDealSocketTransform() const
{
	if (!IsValid(Mesh))
	{
		return GetActorTransform();
	}
	if (Mesh->DoesSocketExist(DealSocketName))
	{
		return Mesh->GetSocketTransform(DealSocketName, RTS_World);
	}
	if (Mesh->DoesSocketExist(TEXT("hand_r")))
	{
		return CardHandOffset * Mesh->GetSocketTransform(TEXT("hand_r"), RTS_World);
	}
	return Mesh->GetComponentTransform();
}

ABlackjackTableActor* ABlackjackDealerVisual::ResolveBlackjackTable() const
{
	TArray<AActor*> Parents = { GetParentActor(), GetAttachParentActor(), GetOwner() };
	TSet<AActor*> Visited;
	for (int32 Index = 0; Index < Parents.Num(); ++Index)
	{
		AActor* Parent = Parents[Index];
		if (!IsValid(Parent) || Parent == this || Visited.Contains(Parent))
		{
			continue;
		}
		if (ABlackjackTableActor* Table = Cast<ABlackjackTableActor>(Parent))
		{
			return Table;
		}
		Visited.Add(Parent);
		Parents.Add(Parent->GetParentActor());
		Parents.Add(Parent->GetAttachParentActor());
		Parents.Add(Parent->GetOwner());
	}
	return nullptr;
}

void ABlackjackDealerVisual::HandleDealerShuffle()
{
	PlayShuffleMontage();
}

void ABlackjackDealerVisual::HandleDealerPresentationReset()
{
	StopDealerAnimation();
}
