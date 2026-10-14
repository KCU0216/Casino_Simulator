#include "BlackjackCardSoundSetup.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Audio.h"
#include "AudioDecompress.h"
#include "Blackjack/BlackjackBlueprintLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Factories/Factory.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "K2Node_Timeline.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/FeedbackContext.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Sound/CasinoSoundTable.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace BlackjackCardSoundSetup
{
const FString CardPackage = TEXT("/Game/Blackjack/BluePrint/Card/BP_BlackjackCard");
const FString TablePackage = TEXT("/Game/1_BluePrint/MyCasinoSoundTable");
const FString WavePackage = TEXT("/Game/6_Sound/BlackjackCardPlace");
const FString AttenuationPackage = TEXT("/Game/6_Sound/BlackjackCardAttenuation");
const FString Recording = TEXT("C:/Users/user1/Downloads/card_table_slap.wav");
const FName SoundId(TEXT("BlackjackCardPlace"));
const FName SoundNodeName(TEXT("CasinoCardPlacementSound"));
const FName SelfNodeName(TEXT("CasinoCardPlacementSelf"));

struct FDiagnosticCheckpoint
{
	SIZE_T Ensures = FDebug::GetNumEnsureFailures();
	int32 Errors = GWarn ? GWarn->GetNumErrors() : 0;

	bool Check(const TCHAR* Stage, FString& OutError) const
	{
		if (FDebug::GetNumEnsureFailures() > Ensures || (GWarn && GWarn->GetNumErrors() > Errors))
		{
			OutError = FString::Printf(TEXT("Card sound %s reported an engine ensure or error; no further assets will be saved."), Stage);
			return false;
		}
		return true;
	}
};

bool PrepareDecoder(FString& OutError)
{
	// -NoSound commandlets skip audio device initialization, which normally registers BINKA's decoder.
	// SoundFactory still creates the SoundWave's runtime data while importing, so it requires that factory.
	if (!FModuleManager::Get().LoadModule(TEXT("BinkAudioDecoder"))
		|| !IAudioInfoFactoryRegistry::Get().Find(FName(TEXT("BINKA"))))
	{
		OutError = TEXT("BINKA decoder is unavailable; card sound setup refused to import or load its SoundWave.");
		return false;
	}
	return true;
}

FString AssetPath(const FString& PackageName)
{
	return PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName);
}

bool SaveKnownAsset(UObject* Asset, FString& OutError)
{
	const FDiagnosticCheckpoint Checkpoint;
	UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
	if (!Package || (Package->GetName() != CardPackage && Package->GetName() != TablePackage
		&& Package->GetName() != WavePackage && Package->GetName() != AttenuationPackage))
	{
		OutError = TEXT("Card sound setup refused to save an asset outside its four explicit packages.");
		return false;
	}
	if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset); Blueprint && Blueprint->Status != BS_UpToDate)
	{
		OutError = TEXT("Card sound setup refused to save an uncompiled card Blueprint.");
		return false;
	}
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Package, Asset, *File, Args))
	{
		OutError = FString::Printf(TEXT("Unable to save card sound asset %s."), *Package->GetName());
		return false;
	}
	return Checkpoint.Check(TEXT("asset save"), OutError);
}

bool FindCompletionHook(UBlueprint* Blueprint, UK2Node_Timeline*& OutTimeline, UK2Node_CallFunction*& OutSound, FString& OutError)
{
	OutTimeline = nullptr;
	OutSound = nullptr;
	TArray<UK2Node_Timeline*> Timelines;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Timelines);
	for (UK2Node_Timeline* Timeline : Timelines)
	{
		if (Timeline->TimelineName != TEXT("TL_Deal")) continue;
		if (OutTimeline)
		{
			OutError = TEXT("Card Blueprint contains multiple TL_Deal nodes; no completion hook can be selected safely.");
			return false;
		}
		OutTimeline = Timeline;
	}
	if (!OutTimeline || !OutTimeline->FindPin(TEXT("Finished")))
	{
		OutError = TEXT("Card Blueprint's original TL_Deal completion output is missing.");
		return false;
	}
	TArray<UK2Node_CallFunction*> Calls;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Calls);
	for (UK2Node_CallFunction* Call : Calls)
	{
		if (Call->FunctionReference.GetMemberName() != TEXT("PlayCardPlacementSound") && Call->GetFName() != SoundNodeName) continue;
		if (OutSound || Call->GetFName() != SoundNodeName
			|| Call->FunctionReference.GetMemberName() != TEXT("PlayCardPlacementSound")
			|| Call->FunctionReference.GetMemberParentClass() != UBlackjackBlueprintLibrary::StaticClass()
			|| Call->GetGraph() != OutTimeline->GetGraph())
		{
			OutError = TEXT("Card Blueprint contains a conflicting or duplicate card sound hook.");
			return false;
		}
		OutSound = Call;
	}
	return true;
}

bool CheckCompletionHook(UK2Node_Timeline* Timeline, UK2Node_CallFunction* Sound, FString& OutError)
{
	UEdGraphPin* Finished = Timeline ? Timeline->FindPin(TEXT("Finished")) : nullptr;
	UEdGraphPin* Execute = Sound ? Sound->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr;
	UEdGraphPin* CardInput = Sound ? Sound->FindPin(TEXT("CardActor")) : nullptr;
	UK2Node_Self* Self = CardInput && CardInput->LinkedTo.Num() == 1 ? Cast<UK2Node_Self>(CardInput->LinkedTo[0]->GetOwningNode()) : nullptr;
	if (!Finished || !Execute || Finished->LinkedTo.Num() != 1 || Finished->LinkedTo[0] != Execute
		|| Execute->LinkedTo.Num() != 1 || Execute->LinkedTo[0] != Finished
		|| !Self || Self->GetFName() != SelfNodeName || Self->GetGraph() != Timeline->GetGraph())
	{
		OutError = TEXT("Card placement sound is not connected once to TL_Deal Finished and the local card Self.");
		return false;
	}
	return true;
}

bool AddCompletionHook(UBlueprint* Blueprint, FString& OutError)
{
	UK2Node_Timeline* Timeline = nullptr;
	UK2Node_CallFunction* Sound = nullptr;
	if (!FindCompletionHook(Blueprint, Timeline, Sound, OutError)) return false;
	if (Sound) return CheckCompletionHook(Timeline, Sound, OutError);
	UEdGraph* Graph = Timeline->GetGraph();
	if (FindObject<UObject>(Graph, *SoundNodeName.ToString()) || FindObject<UObject>(Graph, *SelfNodeName.ToString()))
	{
		OutError = TEXT("The card sound graph node names are already occupied by unrelated nodes.");
		return false;
	}
	UEdGraphPin* Finished = Timeline->FindPin(TEXT("Finished"));
	const TArray<UEdGraphPin*> PreviousCompletion = Finished->LinkedTo;
	const UEdGraphSchema* Schema = Graph->GetSchema();
	Sound = NewObject<UK2Node_CallFunction>(Graph, SoundNodeName);
	Graph->AddNode(Sound, false, false);
	Sound->CreateNewGuid();
	Sound->NodePosX = Timeline->NodePosX + 400;
	Sound->NodePosY = Timeline->NodePosY + 240;
	Sound->FunctionReference.SetExternalMember(TEXT("PlayCardPlacementSound"), UBlackjackBlueprintLibrary::StaticClass());
	Sound->AllocateDefaultPins();
	Sound->NodeComment = TEXT("Play a local spatial card slap only after the card has landed; no extra multicast.");
	UK2Node_Self* Self = NewObject<UK2Node_Self>(Graph, SelfNodeName);
	Graph->AddNode(Self, false, false);
	Self->CreateNewGuid();
	Self->NodePosX = Sound->NodePosX - 180;
	Self->NodePosY = Sound->NodePosY + 100;
	Self->AllocateDefaultPins();
	if (!Schema->TryCreateConnection(Self->FindPin(UEdGraphSchema_K2::PN_Self), Sound->FindPin(TEXT("CardActor"))))
	{
		OutError = TEXT("Unable to connect local card Self to the placement sound.");
		return false;
	}
	Finished->BreakAllPinLinks();
	if (!Schema->TryCreateConnection(Finished, Sound->FindPin(UEdGraphSchema_K2::PN_Execute)))
	{
		OutError = TEXT("Unable to connect the card flight completion to its placement sound.");
		return false;
	}
	// Keep any previously supplied completion behavior after the local feedback.
	for (UEdGraphPin* Next : PreviousCompletion)
	{
		if (!Schema->TryCreateConnection(Sound->FindPin(UEdGraphSchema_K2::PN_Then), Next))
		{
			OutError = TEXT("Unable to preserve the card's original completion behavior.");
			return false;
		}
	}
	if (!CheckCompletionHook(Timeline, Sound, OutError)) return false;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	if (Results.NumErrors || Blueprint->Status != BS_UpToDate)
	{
		OutError = FString::Printf(TEXT("Card sound Blueprint compile failed: %d errors, status %d."), Results.NumErrors, static_cast<int32>(Blueprint->Status));
		return false;
	}
	return true;
}

bool BackUpTargets(UBlueprint* Blueprint, FString& OutBackup, FString& OutError)
{
	OutBackup = FPaths::ProjectSavedDir() / TEXT("CasinoDealerSetup")
		/ FString::Printf(TEXT("CardSoundBackup_%s_%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	if (!IFileManager::Get().MakeDirectory(*OutBackup, true))
	{
		OutError = TEXT("Unable to create the card sound backup directory.");
		return false;
	}
	for (const FString& PackageName : {CardPackage, TablePackage, WavePackage, AttenuationPackage})
	{
		const FString Original = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*Original)) continue;
		const FString Copy = OutBackup / PackageName.RightChop(6) + FPackageName::GetAssetPackageExtension();
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy), true)
			|| IFileManager::Get().Copy(*Copy, *Original, false) != COPY_OK)
		{
			OutError = FString::Printf(TEXT("Unable to back up %s before card sound setup."), *PackageName);
			return false;
		}
	}
	UK2Node_Timeline* Timeline = nullptr;
	UK2Node_CallFunction* Sound = nullptr;
	if (!FindCompletionHook(Blueprint, Timeline, Sound, OutError)) return false;
	FString GraphText;
	TSet<UObject*> Nodes;
	for (UEdGraphNode* Node : Timeline->GetGraph()->Nodes) Nodes.Add(Node);
	FEdGraphUtilities::ExportNodesToText(Nodes, GraphText);
	if (!FFileHelper::SaveStringToFile(GraphText, *(OutBackup / TEXT("Card_EventGraph_Before.txt"))))
	{
		OutError = TEXT("Unable to write the card graph backup before sound setup.");
		return false;
	}
	return true;
}

USoundWave* ImportRecording(const FString& SourceCopy, FString& OutError)
{
	if (FPackageName::DoesPackageExist(WavePackage))
	{
		if (USoundWave* Existing = LoadObject<USoundWave>(nullptr, *AssetPath(WavePackage))) return Existing;
		OutError = TEXT("BlackjackCardPlace package already exists without the expected SoundWave.");
		return nullptr;
	}
	// Use the WAV factory explicitly so an automated import cannot select a different pipeline or create a Cue.
	if (!FModuleManager::Get().LoadModule(TEXT("AudioEditor")))
	{
		OutError = TEXT("AudioEditor module is unavailable for importing the existing card recording.");
		return nullptr;
	}
	UClass* FactoryClass = LoadClass<UFactory>(nullptr, TEXT("/Script/AudioEditor.SoundFactory"));
	UFactory* Factory = FactoryClass ? NewObject<UFactory>(GetTransientPackage(), FactoryClass) : nullptr;
	if (!Factory)
	{
		OutError = TEXT("WAV import factory is unavailable.");
		return nullptr;
	}
	if (FBoolProperty* AutoCue = FindFProperty<FBoolProperty>(FactoryClass, TEXT("bAutoCreateCue"))) AutoCue->SetPropertyValue_InContainer(Factory, false);
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = SourceCopy;
	Task->DestinationPath = TEXT("/Game/6_Sound");
	Task->DestinationName = TEXT("BlackjackCardPlace");
	Task->bAutomated = true;
	Task->bAsync = false;
	Task->bSave = false;
	Task->bReplaceExisting = false;
	Task->Factory = Factory;
	const FDiagnosticCheckpoint ImportCheckpoint;
	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks({Task});
	if (!ImportCheckpoint.Check(TEXT("WAV import"), OutError)) return nullptr;
	USoundWave* Wave = nullptr;
	for (UObject* Imported : Task->GetObjects())
	{
		USoundWave* Candidate = Cast<USoundWave>(Imported);
		if (!Candidate || Imported->GetOutermost()->GetName() != WavePackage || Wave)
		{
			OutError = TEXT("WAV import returned an unexpected or additional asset; no card sound assets were saved.");
			return nullptr;
		}
		Wave = Candidate;
	}
	if (!Wave) OutError = TEXT("Unable to import the existing card_table_slap.wav recording.");
	return Wave;
}
}

bool ValidateBlackjackCardPlacementSound(FString& OutError)
{
	using namespace BlackjackCardSoundSetup;
	const FDiagnosticCheckpoint Checkpoint;
	if (!PrepareDecoder(OutError)) return false;
	UBlueprint* Card = LoadObject<UBlueprint>(nullptr, *AssetPath(CardPackage));
	UCasinoSoundTable* Table = LoadObject<UCasinoSoundTable>(nullptr, *AssetPath(TablePackage));
	USoundWave* Wave = LoadObject<USoundWave>(nullptr, *AssetPath(WavePackage));
	USoundAttenuation* Attenuation = LoadObject<USoundAttenuation>(nullptr, *AssetPath(AttenuationPackage));
	const FCasinoSoundEntry* Entry = Table ? Table->FindEntry(SoundId) : nullptr;
	if (!Card || Card->Status != BS_UpToDate || !Table || !Wave || !Attenuation || !Entry
		|| Entry->Sound != Wave || Entry->Attenuation != Attenuation
		|| !FMath::IsNearlyEqual(Entry->VolumeMultiplier, 0.65f) || !FMath::IsNearlyEqual(Entry->PitchMultiplier, 1.0f)
		|| Wave->NumChannels != 1 || Wave->bLooping || !FMath::IsNearlyEqual(Wave->GetDuration(), 0.42f, 0.002f)
		|| !Attenuation->Attenuation.bAttenuate || !Attenuation->Attenuation.bSpatialize
		|| Attenuation->Attenuation.AttenuationShape != EAttenuationShape::Sphere
		|| !FMath::IsNearlyEqual(Attenuation->Attenuation.AttenuationShapeExtents.X, 150.0f)
		|| !FMath::IsNearlyEqual(Attenuation->Attenuation.FalloffDistance, 650.0f))
	{
		OutError = TEXT("Card landing sound mapping, recording properties or spatial attenuation are not configured as expected.");
		return false;
	}
	UK2Node_Timeline* Timeline = nullptr;
	UK2Node_CallFunction* Sound = nullptr;
	return FindCompletionHook(Card, Timeline, Sound, OutError) && CheckCompletionHook(Timeline, Sound, OutError)
		&& Checkpoint.Check(TEXT("asset validation"), OutError);
}

bool SetupBlackjackCardPlacementSound(FString& OutError)
{
	using namespace BlackjackCardSoundSetup;
	const FDiagnosticCheckpoint SetupCheckpoint;
	if (!PrepareDecoder(OutError)) return false;
	const FString SourceCopy = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("6_Sound/Source/CardTableSlap.wav"));
	if (!IFileManager::Get().FileExists(*Recording)
		|| (IFileManager::Get().FileExists(*SourceCopy) && FMD5Hash::HashFile(*Recording) != FMD5Hash::HashFile(*SourceCopy)))
	{
		OutError = TEXT("The requested card_table_slap.wav source is missing, or its preserved Content source already contains a different recording.");
		return false;
	}
	UBlueprint* Card = LoadObject<UBlueprint>(nullptr, *AssetPath(CardPackage));
	UCasinoSoundTable* Table = LoadObject<UCasinoSoundTable>(nullptr, *AssetPath(TablePackage));
	if (!Card || !Table)
	{
		OutError = TEXT("Existing blackjack card Blueprint or MyCasinoSoundTable is missing.");
		return false;
	}
	if (const FCasinoSoundEntry* Existing = Table->FindEntry(SoundId); Existing
		&& (!Existing->Sound || Existing->Sound->GetPathName() != AssetPath(WavePackage)))
	{
		OutError = TEXT("BlackjackCardPlace is already registered to a different sound; no existing mapping was overwritten.");
		return false;
	}
	UK2Node_Timeline* Timeline = nullptr;
	UK2Node_CallFunction* Sound = nullptr;
	if (!FindCompletionHook(Card, Timeline, Sound, OutError) || (Sound && !CheckCompletionHook(Timeline, Sound, OutError))) return false;
	FString ExistingValidation;
	if (IFileManager::Get().FileExists(*SourceCopy) && ValidateBlackjackCardPlacementSound(ExistingValidation))
	{
		if (!SetupCheckpoint.Check(TEXT("existing setup validation"), OutError)) return false;
		UE_LOG(LogTemp, Display, TEXT("Blackjack card landing sound is already configured and valid; no assets changed."));
		return true;
	}
	const TMap<FName, FCasinoSoundEntry> PreviousEntries = Table->Sounds;
	FString Backup;
	if (!BackUpTargets(Card, Backup, OutError)) return false;
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(SourceCopy), true)
		|| (!IFileManager::Get().FileExists(*SourceCopy) && IFileManager::Get().Copy(*SourceCopy, *Recording, false) != COPY_OK))
	{
		OutError = TEXT("Unable to preserve the requested recording under Content/6_Sound/Source.");
		return false;
	}
	USoundWave* Wave = ImportRecording(SourceCopy, OutError);
	if (!Wave || Wave->NumChannels != 1 || !FMath::IsNearlyEqual(Wave->GetDuration(), 0.42f, 0.002f))
	{
		if (OutError.IsEmpty()) OutError = TEXT("Imported card recording has unexpected channels or duration.");
		return false;
	}
	USoundAttenuation* Attenuation = FPackageName::DoesPackageExist(AttenuationPackage)
		? LoadObject<USoundAttenuation>(nullptr, *AssetPath(AttenuationPackage)) : nullptr;
	if (!Attenuation)
	{
		if (FPackageName::DoesPackageExist(AttenuationPackage))
		{
			OutError = TEXT("BlackjackCardAttenuation package contains a different asset type.");
			return false;
		}
		UPackage* Package = CreatePackage(*AttenuationPackage);
		Attenuation = NewObject<USoundAttenuation>(Package, TEXT("BlackjackCardAttenuation"), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Attenuation);
	}
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(150.0f, 0.0f, 0.0f);
	Attenuation->Attenuation.FalloffDistance = 650.0f;
	Wave->bLooping = false;
	FCasinoSoundEntry Entry;
	Entry.Sound = Wave;
	Entry.Attenuation = Attenuation;
	Entry.VolumeMultiplier = 0.65f;
	Entry.PitchMultiplier = 1.0f;
	Table->Sounds.Add(SoundId, Entry);
	if (!AddCompletionHook(Card, OutError)) return false;
	for (const TPair<FName, FCasinoSoundEntry>& Previous : PreviousEntries)
	{
		if (Previous.Key == SoundId) continue;
		const FCasinoSoundEntry* Current = Table->FindEntry(Previous.Key);
		if (!Current || Current->Sound != Previous.Value.Sound || Current->Attenuation != Previous.Value.Attenuation
			|| Current->VolumeMultiplier != Previous.Value.VolumeMultiplier || Current->PitchMultiplier != Previous.Value.PitchMultiplier)
		{
			OutError = TEXT("An unrelated sound table entry changed; card sound setup refused to save.");
			return false;
		}
	}
	if (!ValidateBlackjackCardPlacementSound(OutError) || !SetupCheckpoint.Check(TEXT("setup before save"), OutError)) return false;
	if (!SaveKnownAsset(Wave, OutError) || !SaveKnownAsset(Attenuation, OutError)
		|| !SaveKnownAsset(Table, OutError) || !SaveKnownAsset(Card, OutError)) return false;
	UE_LOG(LogTemp, Display, TEXT("Blackjack card landing sound configured from card_table_slap.wav; previous assets backed up to %s."), *Backup);
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackCardPlacementSoundAssetTest,
	"Casino.Blackjack.CardPlacementSound.Assets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackCardPlacementSoundAssetTest::RunTest(const FString& Parameters)
{
	FString Error;
	if (!TestTrue(TEXT("Landing hookup and spatial sound mapping are valid"), ValidateBlackjackCardPlacementSound(Error)))
	{
		AddError(Error);
		return false;
	}
	TArray<uint8> SourceBytes;
	const FString Source = FPaths::ProjectContentDir() / TEXT("6_Sound/Source/CardTableSlap.wav");
	if (!TestTrue(TEXT("Original recording preserved for reimport"), FFileHelper::LoadFileToArray(SourceBytes, *Source))) return false;
	FWaveModInfo Header;
	if (!TestTrue(TEXT("Preserved source is a valid WAV"), Header.ReadWaveInfo(SourceBytes.GetData(), SourceBytes.Num(), &Error))) return false;
	USoundWave* Wave = LoadObject<USoundWave>(nullptr, *BlackjackCardSoundSetup::AssetPath(BlackjackCardSoundSetup::WavePackage));
	TArray<uint8> ImportedPCM;
	uint32 SampleRate = 0;
	uint16 Channels = 0;
	if (!TestTrue(TEXT("SoundWave contains imported PCM audio"), Wave->GetImportedSoundWaveData(ImportedPCM, SampleRate, Channels))) return false;
	TestEqual(TEXT("Imported sample rate"), SampleRate, uint32(44100));
	TestEqual(TEXT("Imported mono channel count"), Channels, uint16(1));
	TestEqual(TEXT("Imported data retains original PCM sample count"), ImportedPCM.Num(), static_cast<int32>(Header.SampleDataSize));
	TestTrue(TEXT("Imported audio samples are the user's recording"), ImportedPCM.Num() == static_cast<int32>(Header.SampleDataSize)
		&& FMemory::Memcmp(ImportedPCM.GetData(), Header.SampleDataStart, Header.SampleDataSize) == 0);
	return true;
}
#endif
