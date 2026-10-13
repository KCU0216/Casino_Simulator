#include "DealerAnimationAssetBuilder.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_AnimDynamics.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimPose.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/PinBoneOp.h"
#include "Retargeter/RetargetOps/RootMotionGeneratorOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigAutoCharacterizer.h"
#include "RigEditor/IKRigAutoFBIK.h"
#include "RigEditor/IKRigController.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogCasinoDealerAssets, Log, All);

namespace
{
const FString RetargetFolder = TEXT("/Game/Blackjack/BluePrint/Animation/Dealer/Retargeted");
const FString AnimBlueprintPath = TEXT("/Game/Blackjack/BluePrint/Animation/Dealer/ABP_BlackjackDealer_WBG");
const FString SourceFolder = TEXT("/Game/Casino_Animation/Animation/Blackjack/Dealer");
const FName SecondaryBones[] = { TEXT("breast_l"), TEXT("breast_r"), TEXT("butt_l"), TEXT("butt_r") };

FString ObjectPath(const FString& PackagePath)
{
	return PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
}

UObject* LoadExisting(const FString& PackagePath)
{
	if (!FPackageName::DoesPackageExist(PackagePath))
	{
		return FindObject<UObject>(nullptr, *ObjectPath(PackagePath));
	}
	return LoadObject<UObject>(nullptr, *ObjectPath(PackagePath));
}

bool SaveAsset(UObject* Asset, FString& OutError)
{
	UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
	if (!Package || Package == GetTransientPackage())
	{
		OutError = TEXT("Cannot save a null or transient dealer asset.");
		return false;
	}
	// Guard against accidentally saving a source mesh, skeleton, animation or physics asset.
	const FString PackageName = Package->GetName();
	if (!PackageName.StartsWith(RetargetFolder + TEXT("/")) && PackageName != AnimBlueprintPath)
	{
		OutError = FString::Printf(TEXT("Refused to save a source/out-of-scope asset: %s"), *PackageName);
		return false;
	}
	Package->MarkPackageDirty();
	FSavePackageArgs Arguments;
	Arguments.TopLevelFlags = RF_Public | RF_Standalone;
	Arguments.SaveFlags = SAVE_NoError;
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Package, Asset, *Filename, Arguments))
	{
		OutError = FString::Printf(TEXT("Failed to save dealer asset: %s"), *PackageName);
		return false;
	}
	return true;
}

template<typename AssetType>
AssetType* NewAsset(const FString& PackagePath)
{
	UPackage* Package = CreatePackage(*PackagePath);
	AssetType* Asset = NewObject<AssetType>(Package, *FPackageName::GetLongPackageAssetName(PackagePath), RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(Asset);
	return Asset;
}

UIKRigDefinition* GetOrCreateRig(const FString& Name, USkeletalMesh* Mesh, FAutoCharacterizeResults& Characterization, FString& OutError)
{
	const FString PackagePath = RetargetFolder + TEXT("/") + Name;
	UObject* Existing = LoadExisting(PackagePath);
	if (!Existing && FPackageName::DoesPackageExist(PackagePath))
	{
		OutError = FString::Printf(TEXT("Existing generated rig could not be loaded: %s"), *PackagePath);
		return nullptr;
	}
	UIKRigDefinition* Rig = Cast<UIKRigDefinition>(Existing);
	if (Existing && !Rig)
	{
		OutError = FString::Printf(TEXT("Existing generated rig has the wrong asset type: %s"), *PackagePath);
		return nullptr;
	}
	const bool bIsNew = !Rig;
	if (!Rig)
	{
		Rig = NewAsset<UIKRigDefinition>(PackagePath);
	}
	const UIKRigController* Controller = UIKRigController::GetController(Rig);
	if (!Controller || (!bIsNew && Controller->GetSkeletalMesh() != Mesh))
	{
		OutError = FString::Printf(TEXT("Generated rig uses a different preview mesh: %s"), *PackagePath);
		return nullptr;
	}
	if (bIsNew && !Controller->SetSkeletalMesh(Mesh))
	{
		OutError = FString::Printf(TEXT("Could not assign skeletal mesh to rig: %s"), *PackagePath);
		return nullptr;
	}
	Controller->AutoGenerateRetargetDefinition(Characterization);
	const FRetargetDefinition& Definition = Characterization.AutoRetargetDefinition.RetargetDefinition;
	if (Definition.RootBone.IsNone() || Definition.BoneChains.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Could not characterize skeleton for mesh: %s"), *Mesh->GetPathName());
		return nullptr;
	}
	if (bIsNew)
	{
		Controller->SetRetargetDefinition(Definition);
		FAutoFBIKResults IKResults;
		Controller->AutoGenerateFBIK(IKResults);
		if (!SaveAsset(Rig, OutError))
		{
			return nullptr;
		}
	}
	else if (Rig->GetRetargetChains().IsEmpty() || Rig->GetPelvis().IsNone())
	{
		OutError = FString::Printf(TEXT("Existing generated rig is incomplete: %s"), *PackagePath);
		return nullptr;
	}
	return Rig;
}

UIKRetargeter* GetOrCreateRetargeter(UIKRigDefinition* SourceRig, UIKRigDefinition* TargetRig,
	const FAutoCharacterizeResults& SourceCharacterization, const FAutoCharacterizeResults& TargetCharacterization, FString& OutError)
{
	const FString PackagePath = RetargetFolder + TEXT("/RTG_BlackjackDealer_WBG");
	UObject* Existing = LoadExisting(PackagePath);
	if (!Existing && FPackageName::DoesPackageExist(PackagePath))
	{
		OutError = TEXT("Existing generated dealer retargeter could not be loaded.");
		return nullptr;
	}
	UIKRetargeter* Retargeter = Cast<UIKRetargeter>(Existing);
	if (Existing && !Retargeter)
	{
		OutError = TEXT("Existing dealer retargeter has the wrong asset type.");
		return nullptr;
	}
	if (Retargeter)
	{
		const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
		if (Controller->GetIKRig(ERetargetSourceOrTarget::Source) != SourceRig ||
			Controller->GetIKRig(ERetargetSourceOrTarget::Target) != TargetRig || Controller->GetNumRetargetOps() == 0)
		{
			OutError = TEXT("Existing dealer retargeter has different rigs or an empty operation stack.");
			return nullptr;
		}
		return Retargeter;
	}
	Retargeter = NewAsset<UIKRetargeter>(PackagePath);
	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	{
		FScopedReinitializeIKRetargeter Reinitialize(Controller, ERetargetRefreshMode::ProcessorAndOpStack);
		Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceRig);
		Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetRig);
		Controller->SetPreviewMesh(ERetargetSourceOrTarget::Source, SourceRig->GetPreviewMesh());
		Controller->SetPreviewMesh(ERetargetSourceOrTarget::Target, TargetRig->GetPreviewMesh());
		Controller->RemoveAllOps();
		Controller->AddDefaultOps();
		Controller->AutoMapChains(EAutoMapChainType::Exact, true);
		const FName PoseName = Controller->GetCurrentRetargetPoseName(ERetargetSourceOrTarget::Target);
		Controller->ResetRetargetPose(PoseName, {}, ERetargetSourceOrTarget::Target);
		Controller->AutoAlignAllBones(ERetargetSourceOrTarget::Target);
		if (!TargetCharacterization.AutoRetargetDefinition.BonesToExcludeFromAutoPose.IsEmpty())
		{
			Controller->ResetRetargetPose(PoseName, TargetCharacterization.AutoRetargetDefinition.BonesToExcludeFromAutoPose, ERetargetSourceOrTarget::Target);
		}
		if (FIKRetargetRunIKRigOp* IKOp = Controller->GetFirstRetargetOpOfType<FIKRetargetRunIKRigOp>())
		{
			// Match the built-in automatic retargeter's conservative FK-only default.
			IKOp->SetEnabled(false);
		}
		if (FIKRetargetRootMotionOp* RootOp = Controller->GetFirstRetargetOpOfType<FIKRetargetRootMotionOp>())
		{
			const FName TargetRoot = TargetRig->GetSkeleton().BoneNames[0];
			const FName TargetPelvis = TargetCharacterization.AutoRetargetDefinition.RetargetDefinition.RootBone;
			if (TargetRoot == TargetPelvis)
			{
				RootOp->SetEnabled(false);
			}
			else if (SourceRig->GetSkeleton().BoneNames[0] == SourceCharacterization.AutoRetargetDefinition.RetargetDefinition.RootBone)
			{
				RootOp->Settings.RootMotionSource = ERootMotionSource::GenerateFromTargetPelvis;
				RootOp->Settings.RootHeightSource = ERootMotionHeightSource::SnapToGround;
				RootOp->Settings.bRotateWithPelvis = true;
				RootOp->Settings.bMaintainOffsetFromPelvis = true;
			}
		}
		Controller->AddRetargetOp(FIKRetargetPinBoneOp::StaticStruct());
		if (FIKRetargetPinBoneOp* PinOp = Controller->GetFirstRetargetOpOfType<FIKRetargetPinBoneOp>())
		{
			PinOp->Settings.TranslationMode = EPinBoneTranslationMode::CopyGlobalPosition;
			PinOp->Settings.RotationMode = EPinBoneRotationMode::CopyGlobalRotation;
			for (const FBoneToPin& Bone : TargetCharacterization.AutoRetargetDefinition.BonesToPin.GetBonesToPin())
			{
				FPinBoneData Data;
				Data.BoneToCopyFrom.BoneName = Bone.BoneToPinTo;
				Data.BoneToCopyTo.BoneName = Bone.BoneToPin;
				PinOp->Settings.BonesToPin.Add(Data);
			}
		}
	}
	return SaveAsset(Retargeter, OutError) ? Retargeter : nullptr;
}

bool RetargetSequences(USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UIKRetargeter* Retargeter,
	const TArray<UAnimSequence*>& SourceSequences, TMap<UAnimationAsset*, UAnimationAsset*>& Remapping, FString& OutError)
{
	FRetargetProfile Profile;
	Profile.FillProfileWithAssetSettings(Retargeter);
	FIKRetargetProcessor Processor;
	Processor.Initialize(SourceMesh, TargetMesh, Retargeter, Profile);
	if (!Processor.IsInitialized())
	{
		OutError = TEXT("Dealer IK retarget processor could not initialize; no animations were duplicated.");
		return false;
	}
	const FRetargetSkeleton& SourceSkeleton = Processor.GetSkeleton(ERetargetSourceOrTarget::Source);
	const FRetargetSkeleton& TargetSkeleton = Processor.GetSkeleton(ERetargetSourceOrTarget::Target);
	FAnimPoseEvaluationOptions EvaluationOptions;
	EvaluationOptions.OptionalSkeletalMesh = SourceMesh;
	EvaluationOptions.bExtractRootMotion = false;
	EvaluationOptions.bIncorporateRootMotionIntoPose = true;

	for (UAnimSequence* Source : SourceSequences)
	{
		const FString Destination = RetargetFolder + TEXT("/") + Source->GetName() + TEXT("_WBG");
		UObject* Existing = LoadExisting(Destination);
		if (!Existing && FPackageName::DoesPackageExist(Destination))
		{
			OutError = FString::Printf(TEXT("Existing generated animation could not be loaded: %s"), *Destination);
			return false;
		}
		if (Existing)
		{
			UAnimSequence* Sequence = Cast<UAnimSequence>(Existing);
			if (!Sequence || Sequence->GetSkeleton() != TargetMesh->GetSkeleton() || Sequence->GetPlayLength() <= 0.0f)
			{
				OutError = FString::Printf(TEXT("Existing generated animation is invalid or has a different skeleton: %s"), *Destination);
				return false;
			}
			Remapping.Add(Source, Sequence);
			continue;
		}
		// Use the same pose conversion as the engine's batch retargeter, without its
		// Content Browser synchronization and Slate notification (unsafe in a commandlet).
		if (Source->IsValidAdditive())
		{
			OutError = FString::Printf(TEXT("Dealer sequence is additive; refusing to alter its semantics: %s"), *Source->GetPathName());
			return false;
		}
		UPackage* Package = CreatePackage(*Destination);
		UAnimSequence* Result = Cast<UAnimSequence>(StaticDuplicateObject(Source, Package, *FPackageName::GetLongPackageAssetName(Destination)));
		if (!Result)
		{
			OutError = FString::Printf(TEXT("Could not duplicate dealer sequence: %s"), *Source->GetPathName());
			return false;
		}
		Result->SetFlags(RF_Public | RF_Standalone);
		Result->RetargetSource = NAME_None;
		Result->SetRetargetSourceAsset(TargetMesh);
		Result->SetPreviewMesh(TargetMesh);
		IAnimationDataController& Controller = Result->GetController();
		constexpr bool bTransact = false;
		Controller.OpenBracket(FText::FromString(TEXT("Retargeting blackjack dealer animation")), bTransact);
		Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform, bTransact);
		Controller.RemoveAllBoneTracks(bTransact);
		Result->SetSkeleton(TargetMesh->GetSkeleton());
		Controller.NotifyPopulated();
		Controller.UpdateWithSkeleton(TargetMesh->GetSkeleton(), bTransact);

		const int32 NumFrames = Source->GetNumberOfSampledKeys();
		TArray<FRawAnimSequenceTrack> Tracks;
		Tracks.SetNum(TargetSkeleton.BoneNames.Num());
		for (FRawAnimSequenceTrack& Track : Tracks)
		{
			Track.PosKeys.SetNum(NumFrames);
			Track.RotKeys.SetNum(NumFrames);
			Track.ScaleKeys.SetNum(NumFrames);
		}
		TArray<FTransform> SourceComponentPose;
		SourceComponentPose.SetNum(SourceSkeleton.BoneNames.Num());
		Processor.OnPlaybackReset();
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			FAnimPose SourcePose;
			UAnimPoseExtensions::GetAnimPoseAtFrame(Source, Frame, EvaluationOptions, SourcePose);
			for (int32 Bone = 0; Bone < SourceSkeleton.BoneNames.Num(); ++Bone)
			{
				SourceComponentPose[Bone] = UAnimPoseExtensions::GetBonePose(SourcePose, SourceSkeleton.BoneNames[Bone], EAnimPoseSpaces::World);
				SourceComponentPose[Bone].SetScale3D(FVector::OneVector);
			}
			const float Time = Source->GetTimeAtFrame(Frame);
			const float DeltaTime = Frame > 0 ? Time - Source->GetTimeAtFrame(Frame - 1) : Time;
			Processor.ApplySourceScaleToPose(SourceComponentPose);
			const TArray<FTransform>& ComponentPose = Processor.RunRetargeter(SourceComponentPose, Profile, DeltaTime);
			TArray<FTransform> LocalPose = ComponentPose;
			TargetSkeleton.UpdateLocalTransformsBelowBone(0, LocalPose, ComponentPose);
			for (int32 Bone = 0; Bone < Tracks.Num(); ++Bone)
			{
				Tracks[Bone].PosKeys[Frame] = FVector3f(LocalPose[Bone].GetLocation());
				Tracks[Bone].RotKeys[Frame] = FQuat4f(LocalPose[Bone].GetRotation().GetNormalized());
				Tracks[Bone].ScaleKeys[Frame] = FVector3f(LocalPose[Bone].GetScale3D());
			}
		}
		for (int32 Bone = 0; Bone < Tracks.Num(); ++Bone)
		{
			const FName BoneName = TargetSkeleton.BoneNames[Bone];
			Controller.AddBoneCurve(BoneName, bTransact);
			Controller.SetBoneTrackKeys(BoneName, Tracks[Bone].PosKeys, Tracks[Bone].RotKeys, Tracks[Bone].ScaleKeys, bTransact);
		}
		Controller.CloseBracket(bTransact);
		Result->UpdateRetargetSourceAssetData();
		Result->PostEditChange();
		FAssetRegistryModule::AssetCreated(Result);
		if (!SaveAsset(Result, OutError))
		{
			return false;
		}
		Remapping.Add(Source, Result);
	}
	return true;
}

bool BuildRetargetedMontage(UAnimMontage* Source, USkeletalMesh* TargetMesh,
	const TMap<UAnimationAsset*, UAnimationAsset*>& Remapping, FString& OutError)
{
	const FString Destination = RetargetFolder + TEXT("/") + Source->GetName() + TEXT("_WBG");
	UObject* Existing = LoadExisting(Destination);
	if (!Existing && FPackageName::DoesPackageExist(Destination))
	{
		OutError = TEXT("Existing generated dealer montage could not be loaded.");
		return false;
	}
	if (Existing)
	{
		UAnimMontage* Montage = Cast<UAnimMontage>(Existing);
		if (!Montage || Montage->GetSkeleton() != TargetMesh->GetSkeleton())
		{
			OutError = FString::Printf(TEXT("Existing generated montage has a different type or skeleton: %s"), *Destination);
			return false;
		}
		TArray<UAnimationAsset*> Referred;
		Montage->GetAllAnimationSequencesReferred(Referred);
		for (UAnimationAsset* Asset : Referred)
		{
			if (Asset && Asset->GetSkeleton() != TargetMesh->GetSkeleton())
			{
				OutError = FString::Printf(TEXT("Generated montage still references a source-skeleton sequence: %s"), *Destination);
				return false;
			}
		}
		return true;
	}
	UPackage* Package = CreatePackage(*Destination);
	UAnimMontage* Montage = Cast<UAnimMontage>(StaticDuplicateObject(Source, Package, *FPackageName::GetLongPackageAssetName(Destination)));
	if (!Montage)
	{
		OutError = TEXT("Could not duplicate the dealer deal montage.");
		return false;
	}
	Montage->SetFlags(RF_Public | RF_Standalone);
	Montage->ReplaceReferredAnimations(Remapping);
	Montage->SetSkeleton(TargetMesh->GetSkeleton());
	Montage->SetPreviewMesh(TargetMesh);
	for (FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		Track.SlotName = TEXT("DefaultSlot");
	}
	TArray<UAnimationAsset*> Referred;
	Montage->GetAllAnimationSequencesReferred(Referred);
	for (UAnimationAsset* Asset : Referred)
	{
		if (Asset && Asset->GetSkeleton() != TargetMesh->GetSkeleton())
		{
			OutError = FString::Printf(TEXT("Dealer montage contains an unretargeted reference: %s"), *Asset->GetPathName());
			return false;
		}
	}
	Montage->PostEditChange();
	FAssetRegistryModule::AssetCreated(Montage);
	return SaveAsset(Montage, OutError);
}

template<typename NodeType>
NodeType* AddAnimationNode(UEdGraph& Graph, int32 PositionX)
{
	FGraphNodeCreator<NodeType> Creator(Graph);
	NodeType* Node = Creator.CreateNode();
	Node->NodePosX = PositionX;
	Node->NodePosY = 0;
	Creator.Finalize();
	return Node;
}

bool ConnectPose(UEdGraph* Graph, UEdGraphNode* From, UEdGraphNode* To, FName InputName, FString& OutError)
{
	UEdGraphPin* Output = nullptr;
	for (UEdGraphPin* Pin : From->Pins)
	{
		if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == TEXT("struct"))
		{
			Output = Pin;
			break;
		}
	}
	UEdGraphPin* Input = To->FindPin(InputName, EGPD_Input);
	if (!Output || !Input || !Graph->GetSchema()->TryCreateConnection(Output, Input))
	{
		OutError = FString::Printf(TEXT("Could not connect dealer pose graph: %s -> %s.%s"), *From->GetName(), *To->GetName(), *InputName.ToString());
		return false;
	}
	return true;
}

bool BuildAnimBlueprint(USkeletalMesh* TargetMesh, UAnimSequence* Idle, FString& OutError)
{
	UObject* Existing = LoadExisting(AnimBlueprintPath);
	if (!Existing && FPackageName::DoesPackageExist(AnimBlueprintPath))
	{
		OutError = TEXT("Existing generated dealer AnimBP could not be loaded.");
		return false;
	}
	if (Existing)
	{
		UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Existing);
		if (!Blueprint || Blueprint->TargetSkeleton != TargetMesh->GetSkeleton() || !Blueprint->GeneratedClass || Blueprint->Status == BS_Error)
		{
			OutError = TEXT("Existing generated dealer AnimBP is invalid or targets another skeleton.");
			return false;
		}
		TArray<UAnimGraphNode_AnimDynamics*> DynamicsNodes;
		TArray<UAnimGraphNode_Slot*> Slots;
		TArray<UAnimGraphNode_SequencePlayer*> Players;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, DynamicsNodes);
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Slots);
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Players);
		if (DynamicsNodes.Num() != UE_ARRAY_COUNT(SecondaryBones) || Slots.Num() != 1 || Players.Num() != 1 ||
			Slots[0]->Node.SlotName != TEXT("DefaultSlot") || Players[0]->Node.GetSequence() != Idle)
		{
			OutError = TEXT("Existing generated dealer AnimBP has an unexpected animation graph; refusing to overwrite it.");
			return false;
		}
		for (FName Bone : SecondaryBones)
		{
			if (!DynamicsNodes.ContainsByPredicate([Bone](const UAnimGraphNode_AnimDynamics* Node) { return Node->Node.BoundBone.BoneName == Bone; }))
			{
				OutError = FString::Printf(TEXT("Existing generated dealer AnimBP is missing motion for bone: %s"), *Bone.ToString());
				return false;
			}
		}
		return true;
	}
	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->TargetSkeleton = TargetMesh->GetSkeleton();
	Factory->PreviewSkeletalMesh = TargetMesh;
	Factory->ParentClass = UAnimInstance::StaticClass();
	UPackage* Package = CreatePackage(*AnimBlueprintPath);
	UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package,
		*FPackageName::GetLongPackageAssetName(AnimBlueprintPath), RF_Public | RF_Standalone, nullptr, GWarn));
	if (!Blueprint)
	{
		OutError = TEXT("Could not create the target dealer Animation Blueprint.");
		return false;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UEdGraph* Graph = nullptr;
	UAnimGraphNode_Root* Root = nullptr;
	for (UEdGraph* Candidate : Graphs)
	{
		if (Candidate && Candidate->GetFName() == TEXT("AnimGraph"))
		{
			Graph = Candidate;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UAnimGraphNode_Root* CandidateRoot = Cast<UAnimGraphNode_Root>(Node))
				{
					Root = CandidateRoot;
					break;
				}
			}
			break;
		}
	}
	if (!Graph || !Root)
	{
		OutError = TEXT("New dealer AnimBP does not contain an AnimGraph output node.");
		return false;
	}
	UAnimGraphNode_SequencePlayer* Player = AddAnimationNode<UAnimGraphNode_SequencePlayer>(*Graph, -2100);
	Player->SetAnimationAsset(Idle);
	Player->Node.SetLoopAnimation(true);
	Player->ReconstructNode();
	UAnimGraphNode_Slot* Slot = AddAnimationNode<UAnimGraphNode_Slot>(*Graph, -1800);
	Slot->Node.SlotName = TEXT("DefaultSlot");
	UAnimGraphNode_LocalToComponentSpace* ToComponent = AddAnimationNode<UAnimGraphNode_LocalToComponentSpace>(*Graph, -1500);
	if (!ConnectPose(Graph, Player, Slot, TEXT("Source"), OutError) ||
		!ConnectPose(Graph, Slot, ToComponent, TEXT("LocalPose"), OutError))
	{
		return false;
	}
	UEdGraphNode* Previous = ToComponent;
	int32 PositionX = -1200;
	for (FName BoneName : SecondaryBones)
	{
		UAnimGraphNode_AnimDynamics* Dynamics = AddAnimationNode<UAnimGraphNode_AnimDynamics>(*Graph, PositionX);
		FAnimNode_AnimDynamics& Node = Dynamics->Node;
		Node.BoundBone.BoneName = BoneName;
		Node.bChain = false;
		Node.GravityScale = 0.0f;
		Node.bEnableWind = false;
		Node.Alpha = 0.65f;
		Node.bAngularSpring = true;
		Node.AngularSpringConstant = 8.0f;
		Node.bOverrideAngularDamping = true;
		Node.AngularDampingOverride = 0.8f;
		Node.bOverrideLinearDamping = true;
		Node.LinearDampingOverride = 0.8f;
		Node.PhysicsBodyDefinitions.SetNum(1);
		FAnimPhysBodyDefinition& Body = Node.PhysicsBodyDefinitions[0];
		Body.BoundBone.BoneName = BoneName;
		Body.BoxExtents = FVector(3.0f, 3.0f, 3.0f);
		Body.LocalJointOffset = FVector(2.0f, 0.0f, 0.0f);
		Body.ConstraintSetup.LinearAxesMin = FVector::ZeroVector;
		Body.ConstraintSetup.LinearAxesMax = FVector::ZeroVector;
		Body.ConstraintSetup.AngularLimitsMin = FVector(-8.0f, -8.0f, -8.0f);
		Body.ConstraintSetup.AngularLimitsMax = FVector(8.0f, 8.0f, 8.0f);
		Body.ConstraintSetup.AngularTarget = FVector(1.0f, 0.0f, 0.0f);
		Body.ConstraintSetup.AngularTargetAxis = AnimPhysTwistAxis::AxisX;
		Dynamics->ReconstructNode();
		if (!ConnectPose(Graph, Previous, Dynamics, TEXT("ComponentPose"), OutError))
		{
			return false;
		}
		Previous = Dynamics;
		PositionX += 300;
	}
	UAnimGraphNode_ComponentToLocalSpace* ToLocal = AddAnimationNode<UAnimGraphNode_ComponentToLocalSpace>(*Graph, 0);
	Root->NodePosX = 300;
	Root->NodePosY = 0;
	if (!ConnectPose(Graph, Previous, ToLocal, TEXT("ComponentPose"), OutError) ||
		!ConnectPose(Graph, ToLocal, Root, TEXT("Result"), OutError))
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	if (Results.NumErrors > 0 || Blueprint->Status == BS_Error || !Blueprint->GeneratedClass)
	{
		OutError = FString::Printf(TEXT("Dealer Animation Blueprint failed compilation with %d errors."), Results.NumErrors);
		return false;
	}
	FAssetRegistryModule::AssetCreated(Blueprint);
	return SaveAsset(Blueprint, OutError);
}
}

bool BuildBlackjackDealerAnimationAssets(FString& OutError)
{
	OutError.Reset();
	USkeletalMesh* TargetMesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/KCU/MyContent/WarriorBunnyGirl/Mesh/SKM_WarriorBunnyGirl_02.SKM_WarriorBunnyGirl_02"));
	if (!TargetMesh || !TargetMesh->GetSkeleton())
	{
		OutError = TEXT("Warrior Bunny Girl target mesh/skeleton could not be loaded.");
		return false;
	}
	for (FName Bone : SecondaryBones)
	{
		if (TargetMesh->GetRefSkeleton().FindBoneIndex(Bone) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Target mesh is missing secondary-motion bone: %s"), *Bone.ToString());
			return false;
		}
	}
	const TCHAR* RelativeSequences[] = {
		TEXT("AS_Dealer_BJ_Idle"),
		TEXT("Deal/AS_Dealer_BJ_Card_Deal_Start"), TEXT("Deal/AS_Dealer_BJ_Card_Deal_P2"),
		TEXT("Deal/AS_Dealer_BJ_Card_Deal_P4"), TEXT("Deal/AS_Dealer_BJ_Card_Deal_P7"),
		TEXT("Deal/AS_Dealer_BJ_Card_Deal_End"), TEXT("Deal/AS_Dealer_BJ_Card_Collect"),
		TEXT("Shuffle/AS_Dealer_BJ_Cards_Take"), TEXT("Shuffle/AS_Dealer_BJ_Cards_Shuffle1"),
		TEXT("Shuffle/AS_Dealer_BJ_Cards_Shuffle2"), TEXT("Shuffle/AS_Dealer_BJ_Cards_In_Box")
	};
	TArray<UAnimSequence*> Sequences;
	for (const TCHAR* Relative : RelativeSequences)
	{
		const FString PackagePath = SourceFolder + TEXT("/") + Relative;
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *ObjectPath(PackagePath));
		if (!Sequence || !Sequence->GetSkeleton() || Sequence->GetPlayLength() <= 0.0f || Sequence->GetNumberOfSampledKeys() < 1 ||
			(!Sequences.IsEmpty() && Sequence->GetSkeleton() != Sequences[0]->GetSkeleton()))
		{
			OutError = FString::Printf(TEXT("Source dealer sequence is missing, empty or uses a different skeleton: %s"), *PackagePath);
			return false;
		}
		Sequences.Add(Sequence);
	}
	UAnimMontage* SourceMontage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/Casino_Animation/Animation/Blackjack/Dealer/Deal/AS_Dealer_BJ_Card_Deal_P4_Montage.AS_Dealer_BJ_Card_Deal_P4_Montage"));
	if (!SourceMontage || SourceMontage->GetSkeleton() != Sequences[0]->GetSkeleton())
	{
		OutError = TEXT("Source P4 deal montage is missing or uses a different skeleton.");
		return false;
	}
	USkeletalMesh* SourceMesh = Sequences[0]->GetPreviewMesh();
	if (!SourceMesh || SourceMesh->GetSkeleton() != Sequences[0]->GetSkeleton())
	{
		SourceMesh = Sequences[0]->GetSkeleton()->GetPreviewMesh(true);
	}
	if (!SourceMesh || SourceMesh->GetSkeleton() != Sequences[0]->GetSkeleton())
	{
		SourceMesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/Casino_Animation/Demo/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	}
	if (!SourceMesh || SourceMesh->GetSkeleton() != Sequences[0]->GetSkeleton())
	{
		OutError = TEXT("A source skeletal mesh matching the dealer sequence skeleton could not be found.");
		return false;
	}
	FAutoCharacterizeResults SourceCharacterization;
	FAutoCharacterizeResults TargetCharacterization;
	UIKRigDefinition* SourceRig = GetOrCreateRig(TEXT("IK_BlackjackDealer_Source"), SourceMesh, SourceCharacterization, OutError);
	if (!SourceRig)
	{
		return false;
	}
	UIKRigDefinition* TargetRig = GetOrCreateRig(TEXT("IK_BlackjackDealer_WBG"), TargetMesh, TargetCharacterization, OutError);
	if (!TargetRig)
	{
		return false;
	}
	UIKRetargeter* Retargeter = GetOrCreateRetargeter(SourceRig, TargetRig, SourceCharacterization, TargetCharacterization, OutError);
	if (!Retargeter)
	{
		return false;
	}
	TMap<UAnimationAsset*, UAnimationAsset*> Remapping;
	if (!RetargetSequences(SourceMesh, TargetMesh, Retargeter, Sequences, Remapping, OutError) ||
		!BuildRetargetedMontage(SourceMontage, TargetMesh, Remapping, OutError) ||
		!BuildAnimBlueprint(TargetMesh, Cast<UAnimSequence>(Remapping.FindChecked(Sequences[0])), OutError))
	{
		return false;
	}
	UE_LOG(LogCasinoDealerAssets, Display, TEXT("Dealer retargeting complete: %d sequences, P4 montage, rigs, retargeter and four-bone AnimBP."), Sequences.Num());
	return true;
}
