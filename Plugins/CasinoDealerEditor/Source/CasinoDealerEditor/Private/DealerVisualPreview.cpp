#include "DealerVisualPreview.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimMontage.h"
#include "Blackjack/BlackjackDealerVisual.h"
#include "Blackjack/BlackjackTableActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCasinoDealerPreview, Log, All);

namespace
{
const FString PreviewPackagePath = TEXT("/Game/Blackjack/BluePrint/Animation/Dealer/Preview/L_DealerPresentationPreview");
const FName PreviewTableTag(TEXT("CasinoDealerPreviewTable"));
const FName PreviewCameraTag(TEXT("CasinoDealerPreviewCamera"));

struct FPreviewWorldState
{
	FDelegateHandle BeginPlayHandle;
	TArray<FTimerHandle> Timers;
};

TMap<TWeakObjectPtr<UWorld>, FPreviewWorldState> PreviewWorlds;
FDelegateHandle WorldInitializationHandle;
FDelegateHandle WorldCleanupHandle;

bool IsPreviewPackage(const UWorld* World)
{
	return World && UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == PreviewPackagePath;
}

bool IsRunningPreview(const UWorld* World)
{
	return IsPreviewPackage(World) && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game);
}

ABlackjackTableActor* FindPreviewTable(UWorld* World)
{
	for (TActorIterator<ABlackjackTableActor> It(World); It; ++It)
		if (It->ActorHasTag(PreviewTableTag)) return *It;
	return nullptr;
}

ABlackjackDealerVisual* FindPreviewDealer(ABlackjackTableActor* Table)
{
	TArray<AActor*> Children;
	Table->GetAllChildActors(Children, true);
	for (AActor* Child : Children)
		if (ABlackjackDealerVisual* Dealer = Cast<ABlackjackDealerVisual>(Child)) return Dealer;
	return nullptr;
}

void Schedule(UWorld* World, float Delay, TFunction<void()> Action)
{
	if (!IsRunningPreview(World)) return;
	FPreviewWorldState* State = PreviewWorlds.Find(World);
	if (!State) return;
	FTimerManager& Timers = World->GetTimerManager();
	State->Timers.RemoveAll([&Timers](const FTimerHandle& Handle) { return !Timers.TimerExists(Handle); });
	FTimerHandle Handle;
	Timers.SetTimer(Handle, FTimerDelegate::CreateLambda(MoveTemp(Action)), FMath::Max(0.01f, Delay), false);
	State->Timers.Add(Handle);
}

void StartCycle(TWeakObjectPtr<UWorld> WeakWorld)
{
	UWorld* World = WeakWorld.Get();
	if (!IsRunningPreview(World)) return;
	ABlackjackTableActor* Table = FindPreviewTable(World);
	ABlackjackDealerVisual* Dealer = Table ? FindPreviewDealer(Table) : nullptr;
	if (!IsValid(Table) || !IsValid(Dealer))
	{
		UE_LOG(LogCasinoDealerPreview, Error, TEXT("Preview map is missing its production table/dealer."));
		return;
	}
	Table->ResetRound();
	const bool bShuffleEnabled = Table->IsDealerShuffleAnimationEnabled();
	if (bShuffleEnabled)
	{
		Table->OnDealerShuffle.Broadcast();
		UE_LOG(LogCasinoDealerPreview, Display, TEXT("Dealer preview phase: optional shuffle"));
	}
	else UE_LOG(LogCasinoDealerPreview, Display, TEXT("Dealer preview phase: dealing (shuffle disabled)"));
	const float ShuffleLength = bShuffleEnabled && Dealer->ShuffleMontage ? Dealer->ShuffleMontage->GetPlayLength() : 0.0f;
	const float CardInterval = FMath::Max(0.25f, Dealer->GetDealPresentationDuration(0.25f) + 0.15f);
	const float FirstDealTime = ShuffleLength + 0.4f;
	TWeakObjectPtr<ABlackjackTableActor> WeakTable(Table);
	for (int32 SeatIndex = 0; SeatIndex < 4; ++SeatIndex)
	{
		Schedule(World, FirstDealTime + CardInterval * SeatIndex, [WeakWorld, WeakTable, SeatIndex]()
		{
			if (!IsRunningPreview(WeakWorld.Get()) || !WeakTable.IsValid()) return;
			const FBlackjackCard Card(static_cast<EBlackjackSuit>(SeatIndex), static_cast<EBlackjackRank>(8 + SeatIndex), true);
			WeakTable->OnPlayerHandCardDealt.Broadcast(SeatIndex, 0, 0, Card);
			UE_LOG(LogCasinoDealerPreview, Display, TEXT("Dealer preview phase: seat %d"), SeatIndex);
		});
	}
	Schedule(World, FirstDealTime + CardInterval * 4.0f, [WeakWorld, WeakTable]()
	{
		if (!IsRunningPreview(WeakWorld.Get()) || !WeakTable.IsValid()) return;
		WeakTable->OnDealerCardDealt.Broadcast(INDEX_NONE, FBlackjackCard(EBlackjackSuit::Hearts, EBlackjackRank::Ace, true));
		UE_LOG(LogCasinoDealerPreview, Display, TEXT("Dealer preview phase: dealer card / base facing"));
	});
	Schedule(World, FirstDealTime + CardInterval * 5.0f + 2.0f, [WeakWorld]() { StartCycle(WeakWorld); });
}

void BeginPreview(TWeakObjectPtr<UWorld> WeakWorld)
{
	UWorld* World = WeakWorld.Get();
	if (!IsRunningPreview(World)) return;
	if (APlayerController* Controller = World->GetFirstPlayerController())
	{
		for (TActorIterator<ACameraActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(PreviewCameraTag))
			{
				Controller->SetViewTarget(*It);
				break;
			}
		}
		if (APawn* Pawn = Controller->GetPawn()) Pawn->SetActorHiddenInGame(true);
		Controller->bShowMouseCursor = false;
	}
	Schedule(World, 0.5f, [WeakWorld]() { StartCycle(WeakWorld); });
}

void OnWorldInitialized(UWorld* World, const UWorld::InitializationValues Values)
{
	if (!IsRunningPreview(World) || PreviewWorlds.Contains(World)) return;
	FPreviewWorldState& State = PreviewWorlds.Add(World);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	State.BeginPlayHandle = World->OnWorldBeginPlay.AddLambda([WeakWorld]() { BeginPreview(WeakWorld); });
	if (World->HasBegunPlay()) BeginPreview(WeakWorld);
}

void RemovePreviewWorld(UWorld* World)
{
	if (FPreviewWorldState* State = PreviewWorlds.Find(World))
	{
		World->OnWorldBeginPlay.Remove(State->BeginPlayHandle);
		for (FTimerHandle& Handle : State->Timers) World->GetTimerManager().ClearTimer(Handle);
		PreviewWorlds.Remove(World);
	}
}

void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	RemovePreviewWorld(World);
}
}

void RegisterDealerVisualPreviewCallbacks()
{
	if (!WorldInitializationHandle.IsValid())
		WorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddStatic(&OnWorldInitialized);
	if (!WorldCleanupHandle.IsValid())
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&OnWorldCleanup);
}

void UnregisterDealerVisualPreviewCallbacks()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializationHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldInitializationHandle.Reset();
	WorldCleanupHandle.Reset();
	const TArray<TWeakObjectPtr<UWorld>> Worlds = [&]()
	{
		TArray<TWeakObjectPtr<UWorld>> Keys;
		PreviewWorlds.GetKeys(Keys);
		return Keys;
	}();
	for (const TWeakObjectPtr<UWorld>& World : Worlds)
		if (World.IsValid()) RemovePreviewWorld(World.Get());
	PreviewWorlds.Reset();
}

bool CreateDealerVisualPreview(FString& OutError)
{
	OutError.Reset();
	// Only this named generated package can be created; existing authored maps are never edited.
	if (FPackageName::DoesPackageExist(PreviewPackagePath))
	{
		OutError = TEXT("The generated dealer preview map already exists; open it without regenerating it.");
		return false;
	}
	UPackage* Package = CreatePackage(*PreviewPackagePath);
	if (!Package || FindObject<UWorld>(Package, TEXT("L_DealerPresentationPreview")))
	{
		OutError = TEXT("An unsaved preview world already occupies the output package.");
		return false;
	}
	UClass* TableClass = LoadClass<ABlackjackTableActor>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/BP_BlackjackTable.BP_BlackjackTable_C"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TableClass || !Cube)
	{
		OutError = TEXT("The production blackjack table or engine floor mesh could not be loaded.");
		return false;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("L_DealerPresentationPreview"), Package, false);
	if (!World)
	{
		OutError = TEXT("An isolated editor preview world could not be created.");
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	World->SetFlags(RF_Public | RF_Standalone);
	Package->SetPackageFlags(PKG_ContainsMap);
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->GetWorldSettings()->bEnableWorldBoundsChecks = false;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABlackjackTableActor* Table = World->SpawnActor<ABlackjackTableActor>(TableClass, FVector::ZeroVector,
		FRotator::ZeroRotator, SpawnParameters);
	ACameraActor* Camera = World->SpawnActor<ACameraActor>();
	AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>();
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>();
	APointLight* Key = World->SpawnActor<APointLight>();
	APointLight* Fill = World->SpawnActor<APointLight>();
	if (!Table || !Camera || !Floor || !Sun || !Key || !Fill)
	{
		OutError = TEXT("A required preview scene actor could not be created.");
		return false;
	}
	Table->Tags.Add(PreviewTableTag);
	Table->SetActorLabel(TEXT("Production Blackjack Table - Automatic Dealer Demo"));
	Camera->Tags.Add(PreviewCameraTag);
	Camera->SetActorLabel(TEXT("Dealer Preview Camera"));
	const FVector CameraLocation(-350.0f, -120.0f, 185.0f);
	Camera->SetActorLocationAndRotation(CameraLocation, (FVector(40.0f, 0.0f, 95.0f) - CameraLocation).Rotation());
	Camera->GetCameraComponent()->FieldOfView = 50.0f;
	Camera->GetCameraComponent()->AspectRatio = 16.0f / 9.0f;
	Camera->GetCameraComponent()->bConstrainAspectRatio = true;
	FByteProperty* Activation = FindFProperty<FByteProperty>(Camera->GetClass(), TEXT("AutoActivateForPlayer"));
	if (!Activation)
	{
		OutError = TEXT("The native camera auto-activation property could not be resolved.");
		return false;
	}
	Activation->SetPropertyValue_InContainer(Camera, static_cast<uint8>(EAutoReceiveInput::Player0));
	Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Camera->GetCameraComponent()->PostProcessSettings.AutoExposureMinBrightness = 1.0f;
	Camera->GetCameraComponent()->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;
	Floor->SetActorLabel(TEXT("Preview Floor"));
	Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Floor->SetActorLocation(FVector(0.0f, 0.0f, -10.0f));
	Floor->SetActorScale3D(FVector(20.0f, 20.0f, 0.1f));
	Sun->SetActorLabel(TEXT("Preview Directional Light"));
	Sun->SetActorRotation(FRotator(-50.0f, -30.0f, 0.0f));
	Sun->GetLightComponent()->SetIntensity(2.0f);
	Key->SetActorLabel(TEXT("Preview Warm Key Light"));
	Key->SetActorLocation(FVector(-150.0f, -180.0f, 280.0f));
	Key->PointLightComponent->SetIntensity(18000.0f);
	Key->PointLightComponent->SetAttenuationRadius(1000.0f);
	Key->PointLightComponent->SetLightColor(FLinearColor(1.0f, 0.92f, 0.80f));
	Fill->SetActorLabel(TEXT("Preview Cool Fill Light"));
	Fill->SetActorLocation(FVector(250.0f, 200.0f, 230.0f));
	Fill->PointLightComponent->SetIntensity(10000.0f);
	Fill->PointLightComponent->SetAttenuationRadius(1000.0f);
	Fill->PointLightComponent->SetLightColor(FLinearColor(0.82f, 0.90f, 1.0f));
	FAssetRegistryModule::AssetCreated(World);
	Package->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(PreviewPackagePath, FPackageName::GetMapPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs SaveArguments;
	SaveArguments.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArguments.SaveFlags = SAVE_NoError;
	if (Package->GetName() != PreviewPackagePath || !UPackage::SavePackage(Package, World, *Filename, SaveArguments))
	{
		OutError = TEXT("The whitelisted dealer preview map could not be saved.");
		return false;
	}
	UE_LOG(LogCasinoDealerPreview, Display, TEXT("Created isolated dealer preview map: %s"), *PreviewPackagePath);
	return true;
}
