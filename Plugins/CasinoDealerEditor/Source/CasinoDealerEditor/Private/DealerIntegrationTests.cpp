// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "AnimGraphNode_AnimDynamics.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Blackjack/BlackjackDealerVisual.h"
#include "Blackjack/BlackjackTableActor.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
UObject* ObjectValue(UObject* Object, FName Name)
{
	FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
	return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
}

TArray<AActor*> VisualCards(UObject* Table, FName Name = TEXT("AllVisualCards"))
{
	TArray<AActor*> Cards;
	FArrayProperty* Property = FindFProperty<FArrayProperty>(Table->GetClass(), Name);
	FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
	if (!Inner) return Cards;
	FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Table));
	for (int32 Index = 0; Index < Array.Num(); ++Index)
		Cards.Add(Cast<AActor>(Inner->GetObjectPropertyValue(Array.GetRawPtr(Index))));
	return Cards;
}

int32 LiveCardCount(UWorld* World, UClass* CardClass)
{
	int32 Count = 0;
	for (TActorIterator<AActor> It(World, CardClass); It; ++It)
		if (IsValid(*It)) ++Count;
	return Count;
}

AActor* PlayerVisualAt(UObject* Table, int32 SeatIndex, int32 HandIndex, int32 CardIndex)
{
	UFunction* KeyFunction = Table->FindFunction(TEXT("MakePlayerCardKey"));
	FMapProperty* MapProperty = FindFProperty<FMapProperty>(Table->GetClass(), TEXT("PlayerCardVisualMap"));
	FObjectPropertyBase* ValueProperty = MapProperty ? CastField<FObjectPropertyBase>(MapProperty->ValueProp) : nullptr;
	FStrProperty* KeyProperty = KeyFunction ? FindFProperty<FStrProperty>(KeyFunction, TEXT("Key")) : nullptr;
	if (!KeyFunction || !KeyProperty || !ValueProperty || !CastField<FStrProperty>(MapProperty->KeyProp)) return nullptr;
	FStructOnScope Parameters(KeyFunction);
	for (const TPair<FName, int32>& Input : {TPair<FName, int32>(TEXT("SeatIndex"), SeatIndex),
		TPair<FName, int32>(TEXT("HandIndex"), HandIndex), TPair<FName, int32>(TEXT("CardIndex"), CardIndex)})
	{
		FIntProperty* Property = FindFProperty<FIntProperty>(KeyFunction, Input.Key);
		if (!Property) return nullptr;
		Property->SetPropertyValue_InContainer(Parameters.GetStructMemory(), Input.Value);
	}
	Table->ProcessEvent(KeyFunction, Parameters.GetStructMemory());
	const FString Key = KeyProperty->GetPropertyValue_InContainer(Parameters.GetStructMemory());
	FScriptMapHelper Map(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(Table));
	const int32 Index = Map.FindMapIndexWithKey(&Key);
	return Index != INDEX_NONE ? Cast<AActor>(ValueProperty->GetObjectPropertyValue(Map.GetValuePtr(Index))) : nullptr;
}

bool CardTarget(UObject* Table, FName FunctionName, int32 SeatIndex, int32 HandIndex, int32 CardIndex, FTransform& OutTarget)
{
	UFunction* Function = Table->FindFunction(FunctionName);
	if (!Function) return false;
	FStructOnScope Parameters(Function);
	for (const TPair<FName, int32>& Input : {TPair<FName, int32>(TEXT("SeatIndex"), SeatIndex),
		TPair<FName, int32>(TEXT("HandIndex"), HandIndex), TPair<FName, int32>(TEXT("CardIndex"), CardIndex)})
		if (FIntProperty* Property = FindFProperty<FIntProperty>(Function, Input.Key))
			Property->SetPropertyValue_InContainer(Parameters.GetStructMemory(), Input.Value);
	FStructProperty* Target = FindFProperty<FStructProperty>(Function, TEXT("TargetTransform"));
	FBoolProperty* Valid = FindFProperty<FBoolProperty>(Function, TEXT("bValid"));
	if (!Target || Target->Struct != TBaseStructure<FTransform>::Get() || !Valid) return false;
	Table->ProcessEvent(Function, Parameters.GetStructMemory());
	Target->CopyCompleteValue(&OutTarget, Target->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()));
	return Valid->GetPropertyValue_InContainer(Parameters.GetStructMemory());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackDealerAssetIntegrationTest,
	"Casino.Blackjack.DealerAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackDealerAssetIntegrationTest::RunTest(const FString& Parameters)
{
	UBlueprint* DealerBP = LoadObject<UBlueprint>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/BP_NPC_BlackjackDealer.BP_NPC_BlackjackDealer"));
	UBlueprint* TableBP = LoadObject<UBlueprint>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/BP_BlackjackTable.BP_BlackjackTable"));
	UBlueprint* CardBP = LoadObject<UBlueprint>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/Card/BP_BlackjackCard.BP_BlackjackCard"));
	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/Animation/Dealer/ABP_BlackjackDealer_WBG.ABP_BlackjackDealer_WBG"));
	if (!TestNotNull(TEXT("Shipped dealer BP"), DealerBP) || !TestNotNull(TEXT("Shipped table BP"), TableBP)
		|| !TestNotNull(TEXT("Shipped card BP"), CardBP) || !TestNotNull(TEXT("Retargeted dealer AnimBP"), AnimBP)
		|| !TestNotNull(TEXT("Dealer generated class"), DealerBP->GeneratedClass.Get())
		|| !TestNotNull(TEXT("Table generated class"), TableBP->GeneratedClass.Get())
		|| !TestNotNull(TEXT("Card generated class"), CardBP->GeneratedClass.Get())
		|| !TestNotNull(TEXT("AnimBP generated class"), AnimBP->GeneratedClass.Get())) return false;
	TestTrue(TEXT("Dealer has plain visual parent"), DealerBP->ParentClass == ABlackjackDealerVisual::StaticClass());
	TestFalse(TEXT("Dealer is no longer a Character NPC"), DealerBP->GeneratedClass->IsChildOf(ACharacter::StaticClass()));
	ABlackjackDealerVisual* DealerDefaults = Cast<ABlackjackDealerVisual>(DealerBP->GeneratedClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Plain dealer defaults"), DealerDefaults)
		|| !TestNotNull(TEXT("Dealer mesh"), DealerDefaults->Mesh.Get())
		|| !TestNotNull(TEXT("Dealer skeletal asset"), DealerDefaults->Mesh->GetSkeletalMeshAsset())
		|| !TestNotNull(TEXT("Retargeted deal montage"), DealerDefaults->DealMontage.Get())
		|| !TestNotNull(TEXT("Retargeted shuffle montage"), DealerDefaults->ShuffleMontage.Get())) return false;
	USkeleton* Skeleton = DealerDefaults->Mesh->GetSkeletalMeshAsset()->GetSkeleton();
	TestTrue(TEXT("Dealer AnimBP uses target skeleton"), AnimBP->TargetSkeleton == Skeleton);
	TestTrue(TEXT("Dealer deal montage uses target skeleton"), DealerDefaults->DealMontage->GetSkeleton() == Skeleton);
	TestTrue(TEXT("Dealer shuffle montage uses target skeleton"), DealerDefaults->ShuffleMontage->GetSkeleton() == Skeleton);
	TestTrue(TEXT("Mesh runs the retargeted AnimBP"), DealerDefaults->Mesh->GetAnimClass() == AnimBP->GeneratedClass);
	TArray<UActorComponent*> DealerComponents;
	DealerDefaults->GetComponents(DealerComponents);
	TestEqual(TEXT("Dealer retains only visual components, no interaction/ASC/movement components"), DealerComponents.Num(), 6);
	TArray<UAnimGraphNode_AnimDynamics*> Dynamics;
	FBlueprintEditorUtils::GetAllNodesOfClass(AnimBP, Dynamics);
	TestEqual(TEXT("Dealer graph has four secondary motion nodes"), Dynamics.Num(), 4);
	for (FName Bone : {FName(TEXT("breast_l")), FName(TEXT("breast_r")), FName(TEXT("butt_l")), FName(TEXT("butt_r"))})
		TestTrue(FString::Printf(TEXT("Secondary motion targets %s"), *Bone.ToString()),
			Dynamics.ContainsByPredicate([Bone](const UAnimGraphNode_AnimDynamics* Node) { return Node->Node.BoundBone.BoneName == Bone; }));
	UObject* TableDefaults = TableBP->GeneratedClass->GetDefaultObject();
	FBoolProperty* Animated = FindFProperty<FBoolProperty>(TableBP->GeneratedClass, TEXT("bAnimateDealer"));
	FBoolProperty* AnimatedShuffle = FindFProperty<FBoolProperty>(TableBP->GeneratedClass, TEXT("bAnimateDealerShuffle"));
	FFloatProperty* Interval = FindFProperty<FFloatProperty>(TableBP->GeneratedClass, TEXT("DealerCardIntervalSeconds"));
	FFloatProperty* Shuffle = FindFProperty<FFloatProperty>(TableBP->GeneratedClass, TEXT("DealerShuffleSeconds"));
	if (!TestNotNull(TEXT("Table animation setting"), Animated) || !TestNotNull(TEXT("Table shuffle animation setting"), AnimatedShuffle)
		|| !TestNotNull(TEXT("Table deal interval"), Interval)
		|| !TestNotNull(TEXT("Table shuffle interval"), Shuffle)) return false;
	TestTrue(TEXT("Shipped table enables animated pacing"), Animated->GetPropertyValue_InContainer(TableDefaults));
	TestTrue(TEXT("Table deal interval covers montage and card flight"),
		Interval->GetPropertyValue_InContainer(TableDefaults) >= DealerDefaults->GetDealPresentationDuration(0.25f));
	TestFalse(TEXT("Shipped table skips cosmetic shuffle"), AnimatedShuffle->GetPropertyValue_InContainer(TableDefaults));
	TestEqual(TEXT("Shipped table has no wait before dealing"), Shuffle->GetPropertyValue_InContainer(TableDefaults), 0.0f);
	AActor* CardDefaults = Cast<AActor>(CardBP->GeneratedClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Card actor defaults"), CardDefaults)) return false;
	TestFalse(TEXT("Cosmetic card actors are not replicated a second time"), CardDefaults->GetIsReplicated());

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<uint64> FrameCounterGuard(GFrameCounter, GFrameCounter);
	TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABlackjackTableActor* Table = World->SpawnActor<ABlackjackTableActor>(TableBP->GeneratedClass,
		FVector(400, 200, 0), FRotator(0, 15, 0), SpawnParameters);
	if (!TestNotNull(TEXT("Spawned production table"), Table)) return false;
	Table->DispatchBeginPlay();
	ABlackjackDealerVisual* Dealer = Cast<ABlackjackDealerVisual>(ObjectValue(Table, TEXT("DealerActor")));
	if (!TestNotNull(TEXT("Production BeginPlay resolves plain dealer child"), Dealer)) return false;
	TestTrue(TEXT("Spawned child uses configured target AnimBP"), Dealer->Mesh->GetAnimClass() == AnimBP->GeneratedClass);
	TestTrue(TEXT("Spawned child inherits retargeted deal montage"), Dealer->DealMontage == DealerDefaults->DealMontage);
	TestTrue(TEXT("Spawned child inherits retargeted shuffle montage"), Dealer->ShuffleMontage == DealerDefaults->ShuffleMontage);
	TArray<UActorComponent*> SpawnedDealerComponents;
	Dealer->GetComponents(SpawnedDealerComponents);
	TestEqual(TEXT("Spawned dealer has no old NPC components"), SpawnedDealerComponents.Num(), 6);
	const FTransform DealerActorBaseline = Dealer->GetActorTransform();
	const FRotator DealerMeshBaseline = Dealer->Mesh->GetRelativeRotation();
	AddInfo(FString::Printf(TEXT("Spawned dealer class=%s AnimClass=%s Deal=%s Shuffle=%s Rate=%.3f DealLength=%.3f"),
		*Dealer->GetClass()->GetPathName(), *GetPathNameSafe(Dealer->Mesh->GetAnimClass()),
		*GetPathNameSafe(Dealer->DealMontage.Get()), *GetPathNameSafe(Dealer->ShuffleMontage.Get()),
		Dealer->DealPlayRate, Dealer->DealMontage ? Dealer->DealMontage->GetPlayLength() : 0.0f));
	if (!Dealer->HasActorBegunPlay()) Dealer->DispatchBeginPlay();
	Dealer->Mesh->InitAnim(true);
	if (!TestNotNull(TEXT("Dealer animation instance initialized without viewport"), Dealer->Mesh->GetAnimInstance())) return false;
	AddInfo(FString::Printf(TEXT("Spawned dealer Instance=%s InstanceSkeleton=%s"),
		*Dealer->Mesh->GetAnimInstance()->GetClass()->GetPathName(),
		*GetPathNameSafe(Dealer->Mesh->GetAnimInstance()->CurrentSkeleton.Get())));
	TestTrue(TEXT("Hand-aware production event is bound"), Table->OnPlayerHandCardDealt.IsBound());
	TestTrue(TEXT("Production split remap event is bound"), Table->OnPlayerHandsSplit.IsBound());
	TestTrue(TEXT("Dealer production event is bound"), Table->OnDealerCardDealt.IsBound());
	TestTrue(TEXT("Hole reveal production event is bound"), Table->OnDealerHoleCardRevealed.IsBound());
	TestTrue(TEXT("Table visual reset production event is bound"), Table->OnDealerTableVisualReset.IsBound());
	auto AdvanceVisuals = [&](float Seconds)
	{
		while (Seconds > 0.0f)
		{
			const float Step = FMath::Min(0.01f, Seconds);
			++GFrameCounter;
			World->TimeSeconds += Step;
			World->GetTimerManager().Tick(Step);
			for (AActor* Card : VisualCards(Table))
			{
				if (!IsValid(Card)) continue;
				if (!Card->HasActorBegunPlay()) Card->DispatchBeginPlay();
				TInlineComponentArray<UTimelineComponent*> Timelines(Card);
				for (UTimelineComponent* Timeline : Timelines) Timeline->TickComponent(Step, LEVELTICK_All, nullptr);
			}
			Seconds -= Step;
		}
	};
	FTransform FirstTarget;
	if (!TestTrue(TEXT("Production destination for seat 0 hand 0"),
		CardTarget(Table, TEXT("GetPlayerHandCardTargetTransform"), 0, 0, 0, FirstTarget))) return false;
	const FBlackjackCard FaceCard(EBlackjackSuit::Spades, EBlackjackRank::Ten, true);
	Table->OnPlayerHandCardDealt.Broadcast(0, 0, 0, FaceCard);
	TArray<AActor*> Cards = VisualCards(Table);
	if (!TestEqual(TEXT("One player event creates exactly one visual"), Cards.Num(), 1)
		|| !TestNotNull(TEXT("First card visual"), Cards[0])) return false;
	AActor* FirstCard = Cards[0];
	TestEqual(TEXT("World has one cosmetic card"), LiveCardCount(World, CardBP->GeneratedClass), 1);
	TestTrue(TEXT("Production card is held on dealer mesh before release"), FirstCard->GetRootComponent()->GetAttachParent() == Dealer->Mesh);
	TestTrue(TEXT("Production card starts actual deal montage"), Dealer->Mesh->GetAnimInstance()->Montage_IsPlaying(Dealer->DealMontage));
	UObject* ExpectedBack = ObjectValue(FirstCard, TEXT("BackTexture"));
	if (!TestNotNull(TEXT("Face-up card has a configured card back"), ExpectedBack)) return false;
	Dealer->ReleasePresentedCard();
	TestFalse(TEXT("Released card flies rather than teleporting to its destination"), FirstCard->GetActorLocation().Equals(FirstTarget.GetLocation(), 0.01f));
	AdvanceVisuals(0.1f);
	TestFalse(TEXT("Card remains in flight before the configured duration"), FirstCard->GetActorLocation().Equals(FirstTarget.GetLocation(), 0.01f));
	AdvanceVisuals(0.25f);
	TestFalse(TEXT("Released card is detached from dealer"), FirstCard->GetRootComponent()->GetAttachParent() == Dealer->Mesh);
	TestTrue(TEXT("First released card reaches explicit seat destination"), FirstCard->GetActorLocation().Equals(FirstTarget.GetLocation(), 0.01f));

	FTransform SplitTarget;
	if (!TestTrue(TEXT("Production destination for seat 3 split hand and explicit index"),
		CardTarget(Table, TEXT("GetPlayerHandCardTargetTransform"), 3, 1, 2, SplitTarget))) return false;
	Table->OnPlayerHandCardDealt.Broadcast(3, 1, 2, FBlackjackCard(EBlackjackSuit::Hearts, EBlackjackRank::Seven, true));
	Cards = VisualCards(Table);
	if (!TestEqual(TEXT("Different seat/hand creates only one additional card"), Cards.Num(), 2)) return false;
	AActor* SplitCard = Cards.Last();
	if (!TestNotNull(TEXT("Second seat card visual"), SplitCard)) return false;
	TestTrue(TEXT("Second seat card starts held by dealer"), SplitCard->GetRootComponent()->GetAttachParent() == Dealer->Mesh);
	// No animation update/notifies: the native fallback still releases the held card.
	AdvanceVisuals(Dealer->GetDealPresentationDuration(0.25f) + 0.4f);
	TestTrue(TEXT("Player card presentation turns the dealer mesh toward its seat"),
		FMath::Abs(FMath::FindDeltaAngleDegrees(DealerMeshBaseline.Yaw, Dealer->Mesh->GetRelativeRotation().Yaw)) > 1.0f);
	TestTrue(TEXT("Facing preserves dealer world placement"), Dealer->GetActorTransform().Equals(DealerActorBaseline));
	TestFalse(TEXT("Fallback timer releases the second card"), SplitCard->GetRootComponent()->GetAttachParent() == Dealer->Mesh);
	TestTrue(TEXT("Split card reaches its own indexed destination"), SplitCard->GetActorLocation().Equals(SplitTarget.GetLocation(), 0.01f));
	TestFalse(TEXT("Seat and split destinations differ"), FirstTarget.GetLocation().Equals(SplitTarget.GetLocation(), 0.01f));

	Table->OnDealerCardDealt.Broadcast(INDEX_NONE, FaceCard);
	Dealer->ReleasePresentedCard();
	AdvanceVisuals(0.35f);
	TestTrue(TEXT("Dealer own-card presentation returns to base facing"),
		FMath::Abs(FMath::FindDeltaAngleDegrees(DealerMeshBaseline.Yaw, Dealer->Mesh->GetRelativeRotation().Yaw)) < 0.01f);
	Table->OnDealerCardDealt.Broadcast(INDEX_NONE, FBlackjackCard(EBlackjackSuit::Clubs, EBlackjackRank::None, false));
	TArray<AActor*> DealerCards = VisualCards(Table, TEXT("DealerVisualCards"));
	if (!TestEqual(TEXT("Dealer up card and redacted hole create two visuals"), DealerCards.Num(), 2)) return false;
	AActor* Hole = DealerCards[1];
	if (!TestNotNull(TEXT("Redacted hole visual"), Hole)) return false;
	FBoolProperty* FaceUp = FindFProperty<FBoolProperty>(Hole->GetClass(), TEXT("bFaceUp"));
	if (!TestNotNull(TEXT("Hole visual face state"), FaceUp)) return false;
	TestFalse(TEXT("Redacted hole remains facedown"), FaceUp->GetPropertyValue_InContainer(Hole));
	TestTrue(TEXT("Redacted hole has the deck's real back texture"), ObjectValue(Hole, TEXT("BackTexture")) == ExpectedBack);
	TestNull(TEXT("Redacted hole does not contain a secret front texture"), ObjectValue(Hole, TEXT("FrontTexture")));
	Dealer->ReleasePresentedCard();
	AdvanceVisuals(0.35f);
	Table->OnDealerHoleCardRevealed.Broadcast(INDEX_NONE, FBlackjackCard(EBlackjackSuit::Diamonds, EBlackjackRank::Nine, true));
	TestEqual(TEXT("Reveal reuses hole visual rather than spawning another"), LiveCardCount(World, CardBP->GeneratedClass), 4);
	TestTrue(TEXT("Reveal turns the existing hole face up"), FaceUp->GetPropertyValue_InContainer(Hole));
	TestNotNull(TEXT("Revealed hole receives its actual front texture"), ObjectValue(Hole, TEXT("FrontTexture")));
	Table->OnDealerPresentationReset.Broadcast();
	TestEqual(TEXT("Cancelling a gesture preserves previous table cards"), LiveCardCount(World, CardBP->GeneratedClass), 4);
	Table->ResetRound();
	TestEqual(TEXT("Real round reset clears all card actors"), LiveCardCount(World, CardBP->GeneratedClass), 0);
	TestEqual(TEXT("Real round reset clears all visual references"), VisualCards(Table).Num(), 0);
	TestEqual(TEXT("Real round reset clears dealer card references"), VisualCards(Table, TEXT("DealerVisualCards")).Num(), 0);
	FMapProperty* PlayerMap = FindFProperty<FMapProperty>(Table->GetClass(), TEXT("PlayerCardVisualMap"));
	if (TestNotNull(TEXT("Player visual map"), PlayerMap))
	{
		FScriptMapHelper Map(PlayerMap, PlayerMap->ContainerPtrToValuePtr<void>(Table));
		TestEqual(TEXT("Real round reset clears player visual keys"), Map.Num(), 0);
	}
	AdvanceVisuals(Dealer->GetDealPresentationDuration(0.25f) + 0.4f);
	TestEqual(TEXT("Cancelled visual fallback cannot restore old cards"), LiveCardCount(World, CardBP->GeneratedClass), 0);

	// Timed splitting broadcasts one original card in each hand before replacement
	// draws. The second old card must move/remap even though neither hand has index 1 yet.
	const FBlackjackCard PairFirst(EBlackjackSuit::Spades, EBlackjackRank::Eight, true);
	const FBlackjackCard PairSecond(EBlackjackSuit::Hearts, EBlackjackRank::Eight, true);
	Table->OnPlayerHandCardDealt.Broadcast(0, 0, 0, PairFirst);
	Dealer->ReleasePresentedCard();
	AdvanceVisuals(0.35f);
	Table->OnPlayerHandCardDealt.Broadcast(0, 0, 1, PairSecond);
	Dealer->ReleasePresentedCard();
	AdvanceVisuals(0.35f);
	AActor* OriginalFirst = PlayerVisualAt(Table, 0, 0, 0);
	AActor* OriginalSecond = PlayerVisualAt(Table, 0, 0, 1);
	if (!TestNotNull(TEXT("Original first split card"), OriginalFirst)
		|| !TestNotNull(TEXT("Original second split card"), OriginalSecond)) return false;
	FBlackjackSeatState SplitSnapshot;
	SplitSnapshot.SeatIndex = 0;
	SplitSnapshot.BetAmount = 100;
	SplitSnapshot.bHasSplitThisRound = true;
	SplitSnapshot.Hands.SetNum(2);
	SplitSnapshot.Hands[0].Cards = {PairFirst};
	SplitSnapshot.Hands[1].Cards = {PairSecond};
	for (FBlackjackHand& Hand : SplitSnapshot.Hands)
	{
		Hand.BetAmount = 100;
		Hand.bFromSplit = true;
	}
	FTransform MovedTarget;
	if (!TestTrue(TEXT("Original second card destination after split"),
		CardTarget(Table, TEXT("GetPlayerHandCardTargetTransform"), 0, 1, 0, MovedTarget))) return false;
	Table->OnPlayerHandsSplit.Broadcast(0, SplitSnapshot);
	TestTrue(TEXT("Split preserves first original card mapping"), PlayerVisualAt(Table, 0, 0, 0) == OriginalFirst);
	TestNull(TEXT("Split removes old second-card key before replacement"), PlayerVisualAt(Table, 0, 0, 1));
	TestTrue(TEXT("Split remaps existing second card to new hand"), PlayerVisualAt(Table, 0, 1, 0) == OriginalSecond);
	TestEqual(TEXT("Original-only split does not spawn replacement visuals early"), LiveCardCount(World, CardBP->GeneratedClass), 2);
	auto LogSplitMotion = [&](const TCHAR* Stage)
	{
		FStructProperty* TargetProperty = FindFProperty<FStructProperty>(OriginalSecond->GetClass(), TEXT("DealTargetTransform"));
		FTransform StoredTarget = FTransform::Identity;
		if (TargetProperty && TargetProperty->Struct == TBaseStructure<FTransform>::Get())
			TargetProperty->CopyCompleteValue(&StoredTarget, TargetProperty->ContainerPtrToValuePtr<void>(OriginalSecond));
		FNumericProperty* DurationProperty = FindFProperty<FNumericProperty>(OriginalSecond->GetClass(), TEXT("DealDuration"));
		const double StoredDuration = DurationProperty && DurationProperty->IsFloatingPoint()
			? DurationProperty->GetFloatingPointPropertyValue(DurationProperty->ContainerPtrToValuePtr<void>(OriginalSecond)) : -1.0;
		AddInfo(FString::Printf(TEXT("Split motion %s: actual=%s expected=%s StoredDealTarget=%s StoredDuration=%.3f"),
			Stage, *OriginalSecond->GetActorLocation().ToString(), *MovedTarget.GetLocation().ToString(),
			*StoredTarget.GetLocation().ToString(), StoredDuration));
		TInlineComponentArray<UTimelineComponent*> Timelines(OriginalSecond);
		for (UTimelineComponent* Timeline : Timelines)
			AddInfo(FString::Printf(TEXT("Split timeline %s: playing=%d position=%.3f length=%.3f rate=%.3f"),
				Stage, Timeline->IsPlaying(), Timeline->GetPlaybackPosition(), Timeline->GetTimelineLength(), Timeline->GetPlayRate()));
	};
	LogSplitMotion(TEXT("before advance"));
	AdvanceVisuals(0.4f);
	LogSplitMotion(TEXT("after .4 seconds"));
	TestTrue(TEXT("Original second card moves to split hand destination"), OriginalSecond->GetActorLocation().Equals(MovedTarget.GetLocation(), 0.01f));
	Table->OnPlayerHandCardDealt.Broadcast(0, 0, 1, FBlackjackCard(EBlackjackSuit::Clubs, EBlackjackRank::Two, true));
	Dealer->ReleasePresentedCard();
	AdvanceVisuals(0.35f);
	Table->OnPlayerHandCardDealt.Broadcast(0, 1, 1, FBlackjackCard(EBlackjackSuit::Diamonds, EBlackjackRank::Three, true));
	Dealer->ReleasePresentedCard();
	AdvanceVisuals(0.35f);
	TestEqual(TEXT("Two replacement events add exactly two visuals"), LiveCardCount(World, CardBP->GeneratedClass), 4);
	TestEqual(TEXT("Split leaves exactly four tracked cards"), VisualCards(Table).Num(), 4);
	TestTrue(TEXT("Second original still occupies new-hand first-card key"), PlayerVisualAt(Table, 0, 1, 0) == OriginalSecond);
	TestTrue(TEXT("First-hand replacement has its own new visual"), IsValid(PlayerVisualAt(Table, 0, 0, 1))
		&& PlayerVisualAt(Table, 0, 0, 1) != OriginalSecond);
	TestNotNull(TEXT("Second-hand replacement has its own new visual"), PlayerVisualAt(Table, 0, 1, 1));
	Table->ResetRound();
	TestEqual(TEXT("Split cleanup removes all four visuals"), LiveCardCount(World, CardBP->GeneratedClass), 0);
	for (UStaticMeshComponent* Prop : {Dealer->ShuffleLeftDeck.Get(), Dealer->ShuffleRightDeck.Get(),
		Dealer->ShuffleLeftDeckBack.Get(), Dealer->ShuffleRightDeckBack.Get()})
	{
		if (!TestNotNull(TEXT("Shuffle visual component"), Prop)) return false;
		TestTrue(TEXT("Shuffle prop is cosmetic without collision"), Prop->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Shuffle props do not replicate independently"), Prop->GetIsReplicated());
	}
	TestNotNull(TEXT("Shuffle uses a real card back texture"), Dealer->ShuffleBackTexture.Get());
	TestNotNull(TEXT("Shuffle uses existing card back material"), Dealer->ShuffleCardBackMaterial.Get());
	Table->OnDealerShuffle.Broadcast();
	AdvanceVisuals(Dealer->ShuffleMontage->GetPlayLength() * 0.5f);
	TestFalse(TEXT("Left half-deck is shown during shuffle"), Dealer->ShuffleLeftDeck->bHiddenInGame);
	TestFalse(TEXT("Right half-deck is shown during shuffle"), Dealer->ShuffleRightDeck->bHiddenInGame);
	TestFalse(TEXT("Top card backs are shown during shuffle"), Dealer->ShuffleLeftDeckBack->bHiddenInGame);
	TestTrue(TEXT("Left deck follows dealer's left hand"), Dealer->ShuffleLeftDeck->GetAttachSocketName() == TEXT("hand_l"));
	TestTrue(TEXT("Right deck follows dealer's right hand"), Dealer->ShuffleRightDeck->GetAttachSocketName() == TEXT("hand_r"));
	AdvanceVisuals(Dealer->ShuffleMontage->GetPlayLength());
	TestTrue(TEXT("Deck props disappear after shuffle completes"), Dealer->ShuffleLeftDeck->bHiddenInGame && Dealer->ShuffleRightDeck->bHiddenInGame);
	Table->OnDealerShuffle.Broadcast();
	Table->OnDealerPresentationReset.Broadcast();
	AdvanceVisuals(Dealer->ShuffleMontage->GetPlayLength() + 0.5f);
	TestTrue(TEXT("Cancelled shuffle cannot show props later"), Dealer->ShuffleLeftDeck->bHiddenInGame && Dealer->ShuffleRightDeckBack->bHiddenInGame);
	return true;
}

#endif
