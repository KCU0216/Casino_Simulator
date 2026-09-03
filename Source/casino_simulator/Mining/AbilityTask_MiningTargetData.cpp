#include "Mining/AbilityTask_MiningTargetData.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Mining/OreBase.h"

UAbilityTask_WaitMiningTargetData* UAbilityTask_WaitMiningTargetData::WaitMiningTargetData(UGameplayAbility* OwningAbility, float MaxRange)
{
	// 블루프린트 노드가 호출하는 task 생성 함수.
	UAbilityTask_WaitMiningTargetData* Task = NewAbilityTask<UAbilityTask_WaitMiningTargetData>(OwningAbility);
	Task->MaximumRange = MaxRange;
	return Task;
}

void UAbilityTask_WaitMiningTargetData::Activate()
{
	// Target Data는 서버에서만 수신하고 판정한다.
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 이 GA 활성화의 Target Data 수신 함수를 등록한다.
	ASC->AbilityTargetDataSetDelegate(GetAbilitySpecHandle(), GetActivationPredictionKey())
		.AddUObject(this, &UAbilityTask_WaitMiningTargetData::OnTargetDataReceived);
	// delegate 등록 전에 이미 도착한 데이터도 즉시 처리한다.
	ASC->CallReplicatedTargetDataDelegatesIfSet(GetAbilitySpecHandle(), GetActivationPredictionKey());
	if (IsForRemoteClient())
	{
		SetWaitingOnRemotePlayerData();
	}
}

void UAbilityTask_WaitMiningTargetData::OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag)
{
	// 수신한 Target Data는 한 번 소비해 중복 처리를 막는다.
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		ASC->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	}

	// 클라이언트가 보낸 Hit Result에서 광석과 최대 거리를 검증한다.
	AActor* HitActor = nullptr;
	if (Data.Num() > 0 && Data.Get(0) != nullptr)
	{
		if (const FHitResult* Hit = Data.Get(0)->GetHitResult())
		{
			AActor* Avatar = GetAvatarActor();
			if (Avatar && Hit->GetActor() && FVector::DistSquared(Avatar->GetActorLocation(), Hit->ImpactPoint) <= FMath::Square(MaximumRange))
			{
				HitActor = Cast<AOreBase>(Hit->GetActor());
			}
		}
	}

	// 검증된 광석만 블루프린트의 On Valid Hit으로 전달한다.
	if (HitActor && ShouldBroadcastAbilityTaskDelegates())
	{
		OnValidHit.Broadcast(HitActor);
	}

	// Keep listening until the ability ends: a looping mining montage can send multiple hit frames.
}

UAbilityTask_SendMiningTargetData* UAbilityTask_SendMiningTargetData::SendMiningTargetData(UGameplayAbility* OwningAbility, float TraceRange)
{
	// 블루프린트 노드가 호출하는 task 생성 함수.
	UAbilityTask_SendMiningTargetData* Task = NewAbilityTask<UAbilityTask_SendMiningTargetData>(OwningAbility);
	Task->Range = TraceRange;
	return Task;
}

void UAbilityTask_SendMiningTargetData::Activate()
{
	// 로컬 조작 중인 Pawn만 카메라 기준 trace를 만든다.
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	AActor* Avatar = GetAvatarActor();
	if (!ASC || !Avatar || !IsLocallyControlled())
	{
		EndTask();
		return;
	}

	// 로컬 카메라 시점에서 광석 후보를 찾는다.
	FVector ViewLocation;
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MiningTargetData), false, Avatar);
	GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * Range, ECC_Visibility, QueryParams);

	// Hit Result 하나를 GAS가 전송할 Target Data 형식으로 감싼다.
	FGameplayAbilityTargetDataHandle Data;
	Data.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));

	FScopedPredictionWindow PredictionWindow(ASC, IsPredictingClient());
	if (IsPredictingClient())
	{
		// 원격 클라이언트는 Target Data를 서버 ASC로 보낸다.
		ASC->CallServerSetReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey(), Data, FGameplayTag(), ASC->ScopedPredictionKey);
	}
	else if (ASC->IsOwnerActorAuthoritative())
	{
		// 리슨 서버 호스트는 같은 프로세스의 서버 delegate를 바로 호출한다.
		ASC->AbilityTargetDataSetDelegate(GetAbilitySpecHandle(), GetActivationPredictionKey()).Broadcast(Data, FGameplayTag());
	}

	EndTask();
}
