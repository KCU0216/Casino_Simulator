// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackSeatInteractionActor.h"

#include "Blackjack/BlackjackPlayerComponent.h"
#include "Blackjack/BlackjackTableActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "casino_simulatorCharacter.h"

ABlackjackSeatInteractionActor::ABlackjackSeatInteractionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	InteractionPromptText = FText::FromString(TEXT("E Sit"));

	SeatCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SeatCamera"));
	SeatCamera->SetupAttachment(SceneRoot);
	SeatCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	SeatCamera->SetRelativeRotation(FRotator(-20.0f, 0.0f, 0.0f));
	SeatCamera->SetFieldOfView(70.0f);
	SeatCamera->bUsePawnControlRotation = false;
}

void ABlackjackSeatInteractionActor::BeginPlay()
{
	Super::BeginPlay();

	const bool bHasExplicitTableReference = IsValid(BlackjackTable);
	if (!BlackjackTable)
	{
		BlackjackTable = ResolveBlackjackTable();
	}

	SyncSeatIndexFromNearestSeatPoint();
	if (bHasExplicitTableReference && IsValid(BlackjackTable))
	{
		BlackjackTable->RegisterSeatCameraTarget(this);
	}
}

void ABlackjackSeatInteractionActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (IsValid(SeatCamera))
	{
		SeatCamera->GetCameraView(DeltaTime, OutResult);
		return;
	}
	Super::CalcCamera(DeltaTime, OutResult);
}

void ABlackjackSeatInteractionActor::Interact(Acasino_simulatorCharacter* InteractingCharacter)
{
	if (!InteractingCharacter)
	{
		BP_OnSeatClaimFailed(InteractingCharacter, EBlackjackSeatClaimResult::InvalidPlayer);
		return;
	}

	ABlackjackTableActor* Table = ResolveBlackjackTable();
	if (!Table)
	{
		BP_OnSeatClaimFailed(InteractingCharacter, EBlackjackSeatClaimResult::InvalidSeat);
		return;
	}

	const EBlackjackSeatClaimResult Result = Table->GetSeatClaimResult(InteractingCharacter, SeatIndex);
	if (Result != EBlackjackSeatClaimResult::Accepted)
	{
		BP_OnSeatClaimFailed(InteractingCharacter, Result);
		return;
	}

	if (!Table->TryClaimSeat(InteractingCharacter, SeatIndex))
	{
		BP_OnSeatClaimFailed(InteractingCharacter, EBlackjackSeatClaimResult::RequestFailed);
		return;
	}

	if (UBlackjackPlayerComponent* BlackjackPlayerComponent = InteractingCharacter->GetBlackjackPlayerComponent())
	{
		BlackjackPlayerComponent->EnterBlackjackSeatMode(Table, SeatIndex);
	}

	BP_OnSeatClaimSucceeded(InteractingCharacter, Table, SeatIndex);
}

bool ABlackjackSeatInteractionActor::CanInteract(Acasino_simulatorCharacter* InteractingCharacter) const
{
	if (!Super::CanInteract(InteractingCharacter))
	{
		return false;
	}

	ABlackjackTableActor* Table = ResolveBlackjackTable();
	return Table && Table->CanClaimSeat(InteractingCharacter, SeatIndex);
}

ABlackjackTableActor* ABlackjackSeatInteractionActor::GetBlackjackTable() const
{
	return ResolveBlackjackTable();
}

EBlackjackSeatClaimResult ABlackjackSeatInteractionActor::GetCurrentClaimResult(Acasino_simulatorCharacter* InteractingCharacter) const
{
	if (!InteractingCharacter)
	{
		return EBlackjackSeatClaimResult::InvalidPlayer;
	}

	ABlackjackTableActor* Table = ResolveBlackjackTable();
	if (!Table)
	{
		return EBlackjackSeatClaimResult::InvalidSeat;
	}

	return Table->GetSeatClaimResult(InteractingCharacter, SeatIndex);
}

ABlackjackTableActor* ABlackjackSeatInteractionActor::ResolveBlackjackTable() const
{
	if (BlackjackTable)
	{
		return BlackjackTable;
	}

	TArray<AActor*> ParentActors = { GetOwner(), GetAttachParentActor(), GetParentActor() };
	TSet<AActor*> VisitedActors;
	for (int32 Index = 0; Index < ParentActors.Num(); ++Index)
	{
		AActor* ParentActor = ParentActors[Index];
		if (!IsValid(ParentActor) || ParentActor == this || VisitedActors.Contains(ParentActor))
		{
			continue;
		}
		if (ABlackjackTableActor* ParentTable = Cast<ABlackjackTableActor>(ParentActor))
		{
			return ParentTable;
		}
		VisitedActors.Add(ParentActor);
		ParentActors.Add(ParentActor->GetOwner());
		ParentActors.Add(ParentActor->GetAttachParentActor());
		ParentActors.Add(ParentActor->GetParentActor());
	}

	return nullptr;
}

void ABlackjackSeatInteractionActor::SyncSeatIndexFromNearestSeatPoint()
{
	ABlackjackTableActor* Table = ResolveBlackjackTable();
	if (!Table)
	{
		return;
	}

	const FVector SeatActorLocation = GetActorLocation();
	int32 BestSeatIndex = INDEX_NONE;
	double BestDistanceSq = TNumericLimits<double>::Max();

	for (int32 Index = 0; Index < 4; ++Index)
	{
		USceneComponent* SeatPoint = Table->GetSeatPoint(Index);
		if (!SeatPoint)
		{
			continue;
		}

		const double DistanceSq = FVector::DistSquared(SeatActorLocation, SeatPoint->GetComponentLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestSeatIndex = Index;
		}
	}

	if (BestSeatIndex != INDEX_NONE)
	{
		SeatIndex = BestSeatIndex;
		BlackjackTable = Table;
	}
}
