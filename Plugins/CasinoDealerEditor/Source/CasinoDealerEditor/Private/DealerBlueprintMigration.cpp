#include "DealerBlueprintMigration.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blackjack/BlackjackDealerVisual.h"
#include "Blackjack/BlackjackBlueprintLibrary.h"
#include "Blackjack/BlackjackTableActor.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "K2Node_CallFunction.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
const FString DealerPath = TEXT("/Game/Blackjack/BluePrint/BP_NPC_BlackjackDealer");
const FString TablePath = TEXT("/Game/Blackjack/BluePrint/BP_BlackjackTable");
const FString NotifyPath = TEXT("/Game/Blackjack/BluePrint/Notify/AN_Dealer_DealCardRelease");
const FString CardPath = TEXT("/Game/Blackjack/BluePrint/Card/BP_BlackjackCard");
const FString RetargetFolder = TEXT("/Game/Blackjack/BluePrint/Animation/Dealer/Retargeted");
const FString ShufflePath = RetargetFolder + TEXT("/AM_Dealer_BJ_Shuffle_WBG");

FString ObjectPath(const FString& PackagePath)
{
	return PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
}

bool SaveAsset(UObject* Asset, FString& OutError)
{
	const FString PackageName = Asset ? Asset->GetOutermost()->GetName() : FString();
	if (PackageName != DealerPath && PackageName != TablePath && PackageName != NotifyPath
		&& PackageName != CardPath && PackageName != ShufflePath)
	{
		OutError = TEXT("Migration refused to save an asset outside its five explicit output packages.");
		return false;
	}
	if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset); Blueprint && Blueprint->Status != BS_UpToDate)
	{
		OutError = FString::Printf(TEXT("Refusing to save a Blueprint that is not compiled successfully: %s"), *PackageName);
		return false;
	}
	UPackage* Package = Asset->GetOutermost();
	Package->MarkPackageDirty();
	FSavePackageArgs Arguments;
	Arguments.TopLevelFlags = RF_Public | RF_Standalone;
	Arguments.SaveFlags = SAVE_NoError;
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Package, Asset, *Filename, Arguments))
	{
		OutError = FString::Printf(TEXT("Failed to save migrated dealer asset: %s"), *PackageName);
		return false;
	}
	return true;
}

bool Compile(UBlueprint* Blueprint, FString& OutError)
{
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	if (Results.NumErrors || Blueprint->Status != BS_UpToDate)
	{
		OutError = FString::Printf(TEXT("Dealer migration compilation failed for %s (status %d, errors %d, warnings %d)."),
			*Blueprint->GetPathName(), static_cast<int32>(Blueprint->Status), Results.NumErrors, Results.NumWarnings);
		return false;
	}
	return true;
}

UEdGraph* FindGraph(UBlueprint* Blueprint, FName Name)
{
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == Name)
		{
			return Graph;
		}
	}
	return nullptr;
}

template<typename NodeType>
NodeType* NewNode(UEdGraph* Graph, int32 X, int32 Y)
{
	NodeType* Node = NewObject<NodeType>(Graph);
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	return Node;
}

UK2Node_CallFunction* NewCall(UEdGraph* Graph, UClass* Owner, FName Function, int32 X, int32 Y)
{
	UK2Node_CallFunction* Node = NewNode<UK2Node_CallFunction>(Graph, X, Y);
	Node->FunctionReference.SetExternalMember(Function, Owner);
	Node->AllocateDefaultPins();
	return Node;
}

bool Link(UEdGraphPin* A, UEdGraphPin* B, FString& OutError)
{
	if (!A || !B || !A->GetOwningNode()->GetSchema()->TryCreateConnection(A, B))
	{
		OutError = FString::Printf(TEXT("Failed to connect dealer graph pins: %s -> %s"),
			A ? *A->PinName.ToString() : TEXT("missing"), B ? *B->PinName.ToString() : TEXT("missing"));
		return false;
	}
	return true;
}

bool CopyLinks(UEdGraphPin* From, UEdGraphPin* To, FString& OutError)
{
	if (!From || !To)
	{
		OutError = TEXT("Expected graph pin was missing while copying dealer links.");
		return false;
	}
	To->DefaultValue = From->DefaultValue;
	To->DefaultObject = From->DefaultObject;
	To->DefaultTextValue = From->DefaultTextValue;
	const TArray<UEdGraphPin*> Links = From->LinkedTo;
	for (UEdGraphPin* Other : Links)
	{
		if (!Link(Other, To, OutError))
		{
			return false;
		}
	}
	return true;
}

bool MoveLinks(UEdGraphPin* From, UEdGraphPin* To, FString& OutError)
{
	if (!From || !To)
	{
		OutError = TEXT("Expected graph pin was missing while moving dealer links.");
		return false;
	}
	const TArray<UEdGraphPin*> Links = From->LinkedTo;
	To->DefaultValue = From->DefaultValue;
	To->DefaultObject = From->DefaultObject;
	To->DefaultTextValue = From->DefaultTextValue;
	From->BreakAllPinLinks();
	for (UEdGraphPin* Other : Links)
	{
		if (!Link(Other, To, OutError))
		{
			return false;
		}
	}
	return true;
}

UAnimMontage* BuildShuffleMontage(USkeletalMesh* Mesh, FString& OutError)
{
	const TCHAR* Names[] = { TEXT("AS_Dealer_BJ_Cards_Take_WBG"), TEXT("AS_Dealer_BJ_Cards_Shuffle1_WBG"),
		TEXT("AS_Dealer_BJ_Cards_Shuffle2_WBG"), TEXT("AS_Dealer_BJ_Cards_In_Box_WBG") };
	TArray<UAnimSequence*> Sequences;
	for (const TCHAR* Name : Names)
	{
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *ObjectPath(RetargetFolder + TEXT("/") + Name));
		if (!Sequence || Sequence->GetSkeleton() != Mesh->GetSkeleton() || Sequence->GetPlayLength() <= 0.0f)
		{
			OutError = FString::Printf(TEXT("Retargeted shuffle sequence is missing or incompatible: %s"), Name);
			return nullptr;
		}
		Sequences.Add(Sequence);
	}
	UAnimMontage* Montage = FPackageName::DoesPackageExist(ShufflePath)
		? LoadObject<UAnimMontage>(nullptr, *ObjectPath(ShufflePath))
		: FindObject<UAnimMontage>(nullptr, *ObjectPath(ShufflePath));
	if (!Montage)
	{
		if (FPackageName::DoesPackageExist(ShufflePath))
		{
			OutError = TEXT("An existing shuffle package could not be loaded as an animation montage.");
			return nullptr;
		}
		UPackage* Package = CreatePackage(*ShufflePath);
		Montage = NewObject<UAnimMontage>(Package, *FPackageName::GetLongPackageAssetName(ShufflePath), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Montage);
	}
	Montage->Modify();
	Montage->SetSkeleton(Mesh->GetSkeleton());
	Montage->SetPreviewMesh(Mesh);
	Montage->SlotAnimTracks.Reset();
	FSlotAnimationTrack& Track = Montage->SlotAnimTracks.AddDefaulted_GetRef();
	Track.SlotName = TEXT("DefaultSlot");
	float Time = 0.0f;
	for (UAnimSequence* Sequence : Sequences)
	{
		FAnimSegment& Segment = Track.AnimTrack.AnimSegments.AddDefaulted_GetRef();
		Segment.SetAnimReference(Sequence, true);
		Segment.StartPos = Time;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = Sequence->GetPlayLength();
		Segment.AnimPlayRate = 1.0f;
		Segment.LoopingCount = 1;
		Time += Segment.GetLength();
	}
	Montage->SetCompositeLength(Time);
	Montage->CompositeSections.Reset();
	FCompositeSection& Section = Montage->CompositeSections.AddDefaulted_GetRef();
	Section.SectionName = TEXT("Shuffle");
	Section.NextSectionName = NAME_None;
	Section.Link(Montage, 0.0f);
	Montage->BlendIn.SetBlendTime(0.15f);
	Montage->BlendOut.SetBlendTime(0.15f);
	Montage->bEnableAutoBlendOut = true;
	Montage->PostEditChange();
	return Montage;
}

bool RewriteDealerRelease(UBlueprint* Dealer, FString& OutError)
{
	UEdGraph* Graph = FindGraph(Dealer, TEXT("ReleaseCardFromDealer"));
	if (!Graph)
	{
		OutError = TEXT("Existing ReleaseCardFromDealer graph was not found.");
		return false;
	}
	UK2Node_FunctionEntry* Entry = nullptr;
	const TArray<TObjectPtr<UEdGraphNode>> Nodes = Graph->Nodes;
	for (UEdGraphNode* Node : Nodes)
	{
		if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
		{
			Entry = FunctionEntry;
			Node->BreakAllNodeLinks();
		}
		else
		{
			FBlueprintEditorUtils::RemoveNode(Dealer, Node, true);
		}
	}
	if (!Entry)
	{
		OutError = TEXT("Dealer release function has no entry node.");
		return false;
	}
	UK2Node_CallFunction* Release = NewCall(Graph, ABlackjackDealerVisual::StaticClass(), TEXT("ReleasePresentedCard"), Entry->NodePosX + 240, Entry->NodePosY);
	// Keep the native owner while the old generated/skeleton class is still being replaced.
	// The unconnected Target is the compatible BP self after its new parent is compiled.
	return Link(Entry->FindPin(UEdGraphSchema_K2::PN_Then), Release->FindPin(UEdGraphSchema_K2::PN_Execute), OutError);
}

bool IsDealerTarget(const UEdGraphPin* Target)
{
	if (!Target)
	{
		return false;
	}
	for (UEdGraphPin* LinkPin : Target->LinkedTo)
	{
		if (const UK2Node_VariableGet* Variable = Cast<UK2Node_VariableGet>(LinkPin->GetOwningNode());
			Variable && Variable->VariableReference.GetMemberName() == TEXT("DealerActor"))
		{
			return true;
		}
	}
	return false;
}

bool RewriteCharacterReferences(UBlueprint* Table, UAnimMontage* RetargetedDeal, FString& OutError)
{
	TArray<UEdGraph*> Graphs;
	Table->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		const TArray<TObjectPtr<UEdGraphNode>> Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			if (UK2Node_VariableGet* Variable = Cast<UK2Node_VariableGet>(Node);
				Variable && Variable->VariableReference.GetMemberParentClass() == ACharacter::StaticClass()
				&& Variable->VariableReference.GetMemberName() == TEXT("Mesh") && IsDealerTarget(Variable->FindPin(UEdGraphSchema_K2::PN_Self)))
			{
				UK2Node_VariableGet* Replacement = NewNode<UK2Node_VariableGet>(Graph, Variable->NodePosX, Variable->NodePosY);
				Replacement->VariableReference.SetExternalMember(TEXT("Mesh"), ABlackjackDealerVisual::StaticClass());
				Replacement->AllocateDefaultPins();
				if (!MoveLinks(Variable->FindPin(UEdGraphSchema_K2::PN_Self), Replacement->FindPin(UEdGraphSchema_K2::PN_Self), OutError)
					|| !MoveLinks(Variable->FindPin(TEXT("Mesh")), Replacement->FindPin(TEXT("Mesh")), OutError))
				{
					return false;
				}
				FBlueprintEditorUtils::RemoveNode(Table, Variable, true);
			}
			else if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				Call && Call->FunctionReference.GetMemberParentClass() == ACharacter::StaticClass()
				&& Call->FunctionReference.GetMemberName() == TEXT("PlayAnimMontage") && IsDealerTarget(Call->FindPin(UEdGraphSchema_K2::PN_Self)))
			{
				UK2Node_CallFunction* Replacement = NewCall(Graph, ABlackjackDealerVisual::StaticClass(), TEXT("PlayAnimMontage"), Call->NodePosX, Call->NodePosY);
				for (UEdGraphPin* Pin : Call->Pins)
				{
					if (UEdGraphPin* NewPin = Replacement->FindPin(Pin->PinName); NewPin && !MoveLinks(Pin, NewPin, OutError))
					{
						return false;
					}
				}
				if (UEdGraphPin* MontagePin = Replacement->FindPin(TEXT("AnimMontage")); MontagePin && MontagePin->LinkedTo.IsEmpty())
				{
					MontagePin->DefaultObject = RetargetedDeal;
				}
				FBlueprintEditorUtils::RemoveNode(Table, Call, true);
			}
		}
	}
	return true;
}

bool AddProductionPresentation(UBlueprint* Table, FName GraphName, FString& OutError)
{
	UEdGraph* Graph = FindGraph(Table, GraphName);
	if (!Graph)
	{
		OutError = FString::Printf(TEXT("Missing production card graph: %s"), *GraphName.ToString());
		return false;
	}
	UK2Node_CallFunction* Deal = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
		{
			if (Call->FunctionReference.GetMemberName() == TEXT("PresentCard"))
			{
				return true; // A successful previous migration already installed the guard and fallback.
			}
			if (Call->FunctionReference.GetMemberName() == TEXT("DealTo"))
			{
				if (Deal)
				{
					OutError = TEXT("Expected exactly one final DealTo in a production card graph.");
					return false;
				}
				Deal = Call;
			}
		}
	}
	if (!Deal)
	{
		OutError = FString::Printf(TEXT("Final DealTo was not found in %s."), *GraphName.ToString());
		return false;
	}
	UK2Node_VariableGet* Dealer = NewNode<UK2Node_VariableGet>(Graph, Deal->NodePosX - 440, Deal->NodePosY + 220);
	Dealer->VariableReference.SetSelfMember(TEXT("DealerActor"));
	Dealer->AllocateDefaultPins();
	UK2Node_CallFunction* Valid = NewCall(Graph, UKismetSystemLibrary::StaticClass(), TEXT("IsValid"), Deal->NodePosX - 220, Deal->NodePosY + 220);
	UK2Node_IfThenElse* Branch = NewNode<UK2Node_IfThenElse>(Graph, Deal->NodePosX - 240, Deal->NodePosY);
	Branch->AllocateDefaultPins();
	UK2Node_CallFunction* Present = NewCall(Graph, ABlackjackDealerVisual::StaticClass(), TEXT("PresentCard"), Deal->NodePosX + 60, Deal->NodePosY - 180);
	UEdGraphPin* OldExec = Deal->FindPin(UEdGraphSchema_K2::PN_Execute);
	if (!MoveLinks(OldExec, Branch->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)
		|| !Link(Dealer->FindPin(TEXT("DealerActor")), Valid->FindPin(TEXT("Object")), OutError)
		|| !Link(Valid->FindPin(UEdGraphSchema_K2::PN_ReturnValue), Branch->GetConditionPin(), OutError)
		|| !Link(Branch->FindPin(UEdGraphSchema_K2::PN_Then), Present->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)
		|| !Link(Branch->GetElsePin(), OldExec, OutError)
		|| !Link(Dealer->FindPin(TEXT("DealerActor")), Present->FindPin(UEdGraphSchema_K2::PN_Self), OutError)
		|| !CopyLinks(Deal->FindPin(UEdGraphSchema_K2::PN_Self), Present->FindPin(TEXT("CardActor")), OutError)
		|| !CopyLinks(Deal->FindPin(TEXT("TargetTransform")), Present->FindPin(TEXT("TargetTransform")), OutError)
		|| !CopyLinks(Deal->FindPin(UEdGraphSchema_K2::PN_Then), Present->FindPin(UEdGraphSchema_K2::PN_Then), OutError))
	{
		return false;
	}
	Present->FindPin(TEXT("Duration"))->DefaultValue = TEXT("0.25");
	Deal->FindPin(TEXT("Duration"))->DefaultValue = TEXT("0.25");
	return true;
}

bool MoveSplitReplacementGuard(UBlueprint* Table, FString& OutError)
{
	UEdGraph* Graph = FindGraph(Table, TEXT("ApplySplitCardVisuals"));
	if (!Graph)
	{
		OutError = TEXT("ApplySplitCardVisuals was not found for the paced split migration.");
		return false;
	}
	UK2Node_CallFunction* RemapStart = nullptr;
	UK2Node_CallFunction* FinalMove = nullptr;
	UK2Node_CallFunction* FirstReplacement = nullptr;
	UK2Node_IfThenElse* ReplacementGuard = nullptr;
	auto HasIndex = [](UK2Node_CallFunction* Call, FName PinName, const TCHAR* Value)
	{
		const UEdGraphPin* Pin = Call->FindPin(PinName);
		return Pin && Pin->LinkedTo.IsEmpty() && Pin->DefaultValue == Value;
	};
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
		{
			const FName Name = Call->FunctionReference.GetMemberName();
			if (Name == TEXT("MakePlayerCardKey") && HasIndex(Call, TEXT("HandIndex"), TEXT("0"))
				&& HasIndex(Call, TEXT("CardIndex"), TEXT("1")))
			{
				if (RemapStart) { OutError = TEXT("Split graph has multiple original-second-card remap starts."); return false; }
				RemapStart = Call;
			}
			else if (Name == TEXT("MovePlayerCardVisual") && HasIndex(Call, TEXT("HandIndex"), TEXT("1"))
					&& HasIndex(Call, TEXT("CardIndex"), TEXT("0")))
			{
				if (FinalMove) { OutError = TEXT("Split graph has multiple final original-card moves."); return false; }
				FinalMove = Call;
			}
			else if (Name == TEXT("DealVisualCardToPlayer") && HasIndex(Call, TEXT("HandIndex"), TEXT("0"))
					&& HasIndex(Call, TEXT("CardIndex"), TEXT("1")))
			{
				if (FirstReplacement) { OutError = TEXT("Split graph has multiple first replacement deals."); return false; }
				FirstReplacement = Call;
			}
		}
		if (UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(Node))
		{
			for (UEdGraphPin* Input : Branch->GetConditionPin()->LinkedTo)
			{
				if (const UK2Node_CallFunction* Condition = Cast<UK2Node_CallFunction>(Input->GetOwningNode());
					Condition && Condition->FunctionReference.GetMemberName() == TEXT("BooleanAND"))
				{
					if (ReplacementGuard) { OutError = TEXT("Split graph has multiple replacement-card AND guards."); return false; }
					ReplacementGuard = Branch;
				}
			}
		}
	}
	if (!RemapStart || !FinalMove || !FirstReplacement || !ReplacementGuard)
	{
		OutError = TEXT("The known split remap/move/replacement guard nodes could not be matched safely.");
		return false;
	}
	UEdGraphPin* RemapExec = RemapStart->FindPin(UEdGraphSchema_K2::PN_Execute);
	UEdGraphPin* MoveThen = FinalMove->FindPin(UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* ReplacementExec = FirstReplacement->FindPin(UEdGraphSchema_K2::PN_Execute);
	UEdGraphPin* GuardExec = ReplacementGuard->FindPin(UEdGraphSchema_K2::PN_Execute);
	UEdGraphPin* GuardThen = ReplacementGuard->FindPin(UEdGraphSchema_K2::PN_Then);
	if (!RemapExec || !MoveThen || !ReplacementExec || !GuardExec || !GuardThen)
	{
		OutError = TEXT("A split migration execution pin is missing.");
		return false;
	}
	if (GuardExec->LinkedTo.Num() == 1 && GuardExec->LinkedTo[0] == MoveThen
		&& GuardThen->LinkedTo.Num() == 1 && GuardThen->LinkedTo[0] == ReplacementExec)
	{
		return true; // Already migrated: original cards move before testing for replacements.
	}
	if (GuardExec->LinkedTo.Num() != 1 || GuardThen->LinkedTo.Num() != 1 || GuardThen->LinkedTo[0] != RemapExec
		|| MoveThen->LinkedTo.Num() != 1 || MoveThen->LinkedTo[0] != ReplacementExec)
	{
		OutError = TEXT("Split graph execution links differ from the inspected original or completed migration.");
		return false;
	}
	UEdGraphPin* HandsValidThen = GuardExec->LinkedTo[0];
	const UK2Node_IfThenElse* HandsGuard = Cast<UK2Node_IfThenElse>(HandsValidThen->GetOwningNode());
	if (!HandsGuard || HandsValidThen != HandsGuard->FindPin(UEdGraphSchema_K2::PN_Then))
	{
		OutError = TEXT("The original split-hand validity guard was not found before replacement gating.");
		return false;
	}
	GuardExec->BreakAllPinLinks();
	RemapExec->BreakAllPinLinks();
	ReplacementExec->BreakAllPinLinks();
	ReplacementGuard->NodePosX = FinalMove->NodePosX + 240;
	ReplacementGuard->NodePosY = FinalMove->NodePosY;
	ReplacementGuard->NodeComment = TEXT("Move both original split cards first; spawn replacements only when this snapshot already contains them.");
	return Link(HandsValidThen, RemapExec, OutError)
		&& Link(MoveThen, GuardExec, OutError)
		&& Link(GuardThen, ReplacementExec, OutError);
}

bool ConfigureCardFlightDuration(UBlueprint* Card, FString& OutError)
{
	UEdGraph* Graph = FindGraph(Card, TEXT("DealTo"));
	if (!Graph)
	{
		OutError = TEXT("The card DealTo graph was not found for its flight-duration migration.");
		return false;
	}
	UK2Node_FunctionEntry* Entry = nullptr;
	UK2Node_CallFunction* Play = nullptr;
	UK2Node_CallFunction* ExistingRate = nullptr;
	UK2Node_CallFunction* ExistingLength = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
		{
			Entry = FunctionEntry;
		}
		if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
		{
			const FName Name = Call->FunctionReference.GetMemberName();
			if (Name == TEXT("PlayFromStart"))
			{
				if (Play) { OutError = TEXT("Card DealTo contains multiple timeline starts."); return false; }
				Play = Call;
			}
			else if (Name == TEXT("SetPlayRate"))
			{
				if (ExistingRate) { OutError = TEXT("Card DealTo contains multiple timeline rate setters."); return false; }
				ExistingRate = Call;
			}
			else if (Name == TEXT("GetTimelineLength"))
			{
				ExistingLength = Call;
			}
		}
	}
	if (!Entry || !Play || !Entry->FindPin(TEXT("Duration")))
	{
		OutError = TEXT("Card DealTo's original duration input or timeline start was not found.");
		return false;
	}
	UEdGraphPin* PlayExec = Play->FindPin(UEdGraphSchema_K2::PN_Execute);
	if (ExistingRate || ExistingLength)
	{
		if (ExistingRate && ExistingLength && PlayExec && PlayExec->LinkedTo.Num() == 1
			&& PlayExec->LinkedTo[0] == ExistingRate->FindPin(UEdGraphSchema_K2::PN_Then))
		{
			return true;
		}
		OutError = TEXT("Card DealTo contains an incomplete or unrelated timeline rate/length chain.");
		return false;
	}
	UK2Node_CallFunction* Rate = NewCall(Graph, UTimelineComponent::StaticClass(), TEXT("SetPlayRate"), Play->NodePosX - 200, Play->NodePosY);
	UK2Node_CallFunction* Length = NewCall(Graph, UTimelineComponent::StaticClass(), TEXT("GetTimelineLength"), Play->NodePosX - 720, Play->NodePosY + 240);
	UK2Node_CallFunction* SafeDuration = NewCall(Graph, UKismetMathLibrary::StaticClass(), TEXT("FMax"), Play->NodePosX - 480, Play->NodePosY + 420);
	UK2Node_CallFunction* Divide = NewCall(Graph, UKismetMathLibrary::StaticClass(), TEXT("Divide_DoubleDouble"), Play->NodePosX - 240, Play->NodePosY + 240);
	SafeDuration->FindPin(TEXT("B"))->DefaultValue = TEXT("0.01");
	Rate->NodeComment = TEXT("Play the existing card timeline within the caller's requested Duration.");
	return MoveLinks(PlayExec, Rate->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)
		&& Link(Rate->FindPin(UEdGraphSchema_K2::PN_Then), PlayExec, OutError)
		&& CopyLinks(Play->FindPin(UEdGraphSchema_K2::PN_Self), Rate->FindPin(UEdGraphSchema_K2::PN_Self), OutError)
		&& CopyLinks(Play->FindPin(UEdGraphSchema_K2::PN_Self), Length->FindPin(UEdGraphSchema_K2::PN_Self), OutError)
		&& Link(Length->FindPin(UEdGraphSchema_K2::PN_ReturnValue), Divide->FindPin(TEXT("A")), OutError)
		&& Link(Entry->FindPin(TEXT("Duration")), SafeDuration->FindPin(TEXT("A")), OutError)
		&& Link(SafeDuration->FindPin(UEdGraphSchema_K2::PN_ReturnValue), Divide->FindPin(TEXT("B")), OutError)
		&& Link(Divide->FindPin(UEdGraphSchema_K2::PN_ReturnValue), Rate->FindPin(TEXT("NewRate")), OutError);
}

bool BindPresentationReset(UBlueprint* Table, FString& OutError)
{
	UEdGraph* Graph = nullptr;
	UK2Node_CallFunction* InitializeCounts = nullptr;
	for (UEdGraph* EventGraph : Table->UbergraphPages)
	{
		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			if (UK2Node_AddDelegate* Binding = Cast<UK2Node_AddDelegate>(Node);
				Binding && Binding->DelegateReference.GetMemberName() == TEXT("OnDealerTableVisualReset"))
			{
				return true;
			}
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				Call && Call->FunctionReference.GetMemberName() == TEXT("ResetVisualCardCounts"))
			{
				if (InitializeCounts)
				{
					OutError = TEXT("Expected one BeginPlay visual-count initialization call.");
					return false;
				}
				Graph = EventGraph;
				InitializeCounts = Call;
			}
		}
	}
	if (!InitializeCounts || !Graph)
	{
		OutError = TEXT("BeginPlay's visual initialization chain was not found for the reset binding.");
		return false;
	}
	UK2Node_AddDelegate* Binding = NewNode<UK2Node_AddDelegate>(Graph,
		InitializeCounts->NodePosX - 220, InitializeCounts->NodePosY);
	Binding->DelegateReference.SetExternalMember(TEXT("OnDealerTableVisualReset"), ABlackjackTableActor::StaticClass());
	Binding->AllocateDefaultPins();
	UFunction* Signature = Binding->GetDelegateSignature();
	if (!Signature)
	{
		OutError = TEXT("The native dealer reset delegate signature could not be resolved.");
		return false;
	}
	UK2Node_CustomEvent* Handler = UK2Node_CustomEvent::CreateFromFunction(
		FVector2D(InitializeCounts->NodePosX, InitializeCounts->NodePosY + 420), Graph,
		TEXT("HandleDealerTableVisualReset"), Signature, false);
	UK2Node_CallFunction* Clear = NewCall(Graph, Table->GeneratedClass, TEXT("ClearBlackjackCardVisuals"),
		InitializeCounts->NodePosX + 260, InitializeCounts->NodePosY + 420);
	return MoveLinks(InitializeCounts->FindPin(UEdGraphSchema_K2::PN_Execute), Binding->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)
		&& Link(Binding->FindPin(UEdGraphSchema_K2::PN_Then), InitializeCounts->FindPin(UEdGraphSchema_K2::PN_Execute), OutError)
		&& Link(Handler->FindPin(TEXT("OutputDelegate")), Binding->GetDelegatePin(), OutError)
		&& Link(Handler->FindPin(UEdGraphSchema_K2::PN_Then), Clear->FindPin(UEdGraphSchema_K2::PN_Execute), OutError);
}

void ClearOldInheritedOverrides(UBlueprint* Dealer)
{
	if (UInheritableComponentHandler* Handler = Dealer->GetInheritableComponentHandler(false))
	{
		TArray<FComponentKey> Keys;
		for (auto Iterator = Handler->CreateRecordIterator(); Iterator; ++Iterator)
		{
			Keys.Add(Iterator->ComponentKey);
		}
		for (const FComponentKey& Key : Keys)
		{
			Handler->RemoveOverridenComponentTemplate(Key);
		}
	}
	Dealer->ComponentTemplates.Reset();
}

bool SetTableTiming(UBlueprint* Table, ABlackjackDealerVisual* Dealer, FString& OutError)
{
	UObject* Defaults = Table->GeneratedClass->GetDefaultObject();
	FBoolProperty* Animated = FindFProperty<FBoolProperty>(Table->GeneratedClass, TEXT("bAnimateDealer"));
	FBoolProperty* AnimatedShuffle = FindFProperty<FBoolProperty>(Table->GeneratedClass, TEXT("bAnimateDealerShuffle"));
	FFloatProperty* Interval = FindFProperty<FFloatProperty>(Table->GeneratedClass, TEXT("DealerCardIntervalSeconds"));
	FFloatProperty* Shuffle = FindFProperty<FFloatProperty>(Table->GeneratedClass, TEXT("DealerShuffleSeconds"));
	if (!Animated || !AnimatedShuffle || !Interval || !Shuffle)
	{
		OutError = TEXT("Table does not expose the native dealer presentation timing properties.");
		return false;
	}
	Animated->SetPropertyValue_InContainer(Defaults, true);
	AnimatedShuffle->SetPropertyValue_InContainer(Defaults, false);
	Interval->SetPropertyValue_InContainer(Defaults, Dealer->GetDealPresentationDuration(0.25f) + 0.05f);
	Shuffle->SetPropertyValue_InContainer(Defaults, 0.0f);
	// Native defaults changed after compilation; refresh the optimized instance-copy list.
	CastChecked<UBlueprintGeneratedClass>(Table->GeneratedClass.Get())->UpdateCustomPropertyListForPostConstruction();
	return true;
}

bool WireProductionSeatIndex(UBlueprint* Table, FName GraphName, bool bPlayerCard, FString& OutError)
{
	UEdGraph* Graph = FindGraph(Table, GraphName);
	UK2Node_FunctionEntry* Entry = nullptr;
	UK2Node_CallFunction* Present = nullptr;
	if (Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
			{
				Entry = FunctionEntry;
			}
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				Call && Call->FunctionReference.GetMemberName() == TEXT("PresentCard"))
			{
				if (Present) { OutError = TEXT("Multiple production PresentCard nodes cannot be refined safely."); return false; }
				Present = Call;
			}
		}
	}
	if (!Entry || !Present)
	{
		OutError = FString::Printf(TEXT("Run Migrate before refining the existing production presentation graph: %s"), *GraphName.ToString());
		return false;
	}
	// Reconstruct preserves existing links/defaults and adds the new optional native pin.
	Present->ReconstructNode();
	UEdGraphPin* SeatInput = Present->FindPin(TEXT("SeatIndex"));
	UEdGraphPin* SeatOutput = bPlayerCard ? Entry->FindPin(TEXT("SeatIndex")) : nullptr;
	if (!SeatInput || (bPlayerCard && !SeatOutput))
	{
		OutError = TEXT("The native PresentCard seat index or player callback seat pin is missing.");
		return false;
	}
	if (bPlayerCard)
	{
		if (SeatInput->LinkedTo.Num() == 1 && SeatInput->LinkedTo[0] == SeatOutput)
		{
			return true;
		}
		if (!SeatInput->LinkedTo.IsEmpty())
		{
			OutError = TEXT("PresentCard SeatIndex has an unexpected existing connection; no graph was saved.");
			return false;
		}
		return Link(SeatOutput, SeatInput, OutError);
	}
	if (!SeatInput->LinkedTo.IsEmpty())
	{
		OutError = TEXT("The dealer-card presentation has an unexpected player-seat connection.");
		return false;
	}
	SeatInput->DefaultValue = TEXT("-1");
	return true;
}

bool ConfigureShuffleProps(ABlackjackDealerVisual* DealerDefaults, UBlueprint* Table, FString& OutError)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* BackMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/Card/Material/M_CardBack.M_CardBack"));
	// Engine's cube defaults to WorldGridMaterial. Use the plain shape material for paper edges.
	UMaterialInterface* BodyMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!DealerDefaults || !DealerDefaults->Mesh || !DealerDefaults->ShuffleLeftDeck || !DealerDefaults->ShuffleRightDeck
		|| !DealerDefaults->ShuffleLeftDeckBack || !DealerDefaults->ShuffleRightDeckBack || !Cube || !Plane || !BackMaterial || !BodyMaterial)
	{
		OutError = TEXT("The native shuffle deck components or existing cube/plane/card-back assets are missing.");
		return false;
	}
	UObject* TableDefaults = Table->GeneratedClass->GetDefaultObject();
	UDataTable* Data = nullptr;
	if (FObjectPropertyBase* DataProperty = FindFProperty<FObjectPropertyBase>(Table->GeneratedClass, TEXT("CardVisualDataTable")))
	{
		Data = Cast<UDataTable>(DataProperty->GetObjectPropertyValue_InContainer(TableDefaults));
	}
	if (!Data)
	{
		Data = LoadObject<UDataTable>(nullptr, TEXT("/Game/Blackjack/Data/DT_BlackjackCardVisuals.DT_BlackjackCardVisuals"));
	}
	EBlackjackDeckStyle Style = EBlackjackDeckStyle::CasinoRed;
	if (FByteProperty* StyleProperty = FindFProperty<FByteProperty>(Table->GeneratedClass, TEXT("CurrentDeckStyle")))
	{
		Style = static_cast<EBlackjackDeckStyle>(StyleProperty->GetPropertyValue_InContainer(TableDefaults));
	}
	FBlackjackCardVisualData VisualData;
	if (!UBlackjackBlueprintLibrary::FindBlackjackCardVisualData(Data, Style,
		FBlackjackCard(EBlackjackSuit::Clubs, EBlackjackRank::None, false), VisualData) || !VisualData.BackTexture)
	{
		OutError = TEXT("The current gameplay deck's card back texture could not be resolved for the shuffle props.");
		return false;
	}
	DealerDefaults->bFaceDealtSeat = true;
	DealerDefaults->bShowShuffleDecks = true;
	DealerDefaults->ShuffleDeckMesh = Cube;
	DealerDefaults->ShuffleDeckMaterial = BodyMaterial;
	DealerDefaults->ShuffleCardBackMaterial = BackMaterial;
	DealerDefaults->ShuffleBackTexture = VisualData.BackTexture;
	// Match the existing card BP's 6.954 x 10 cm faces; each half-deck is 0.7 cm thick.
	DealerDefaults->ShuffleLeftDeckOffset.SetScale3D(FVector(0.06954f, 0.1f, 0.007f));
	DealerDefaults->ShuffleRightDeckOffset.SetScale3D(FVector(0.06954f, 0.1f, 0.007f));
	UStaticMeshComponent* Bodies[] = { DealerDefaults->ShuffleLeftDeck, DealerDefaults->ShuffleRightDeck };
	UStaticMeshComponent* Backs[] = { DealerDefaults->ShuffleLeftDeckBack, DealerDefaults->ShuffleRightDeckBack };
	const FName HandBones[] = { TEXT("hand_l"), TEXT("hand_r") };
	const FTransform Offsets[] = { DealerDefaults->ShuffleLeftDeckOffset, DealerDefaults->ShuffleRightDeckOffset };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Bodies[Index]->SetupAttachment(DealerDefaults->Mesh, HandBones[Index]);
		Bodies[Index]->SetStaticMesh(Cube);
		Bodies[Index]->SetMaterial(0, BodyMaterial);
		Bodies[Index]->SetRelativeTransform(Offsets[Index]);
		Bodies[Index]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bodies[Index]->SetGenerateOverlapEvents(false);
		Bodies[Index]->SetHiddenInGame(true);
		Backs[Index]->SetupAttachment(Bodies[Index]);
		Backs[Index]->SetStaticMesh(Plane);
		Backs[Index]->SetMaterial(0, BackMaterial);
		Backs[Index]->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 50.1f), FVector::OneVector));
		Backs[Index]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Backs[Index]->SetGenerateOverlapEvents(false);
		Backs[Index]->SetHiddenInGame(true);
	}
	// Compile/load computes this native-property copy cache before the new material and
	// texture references are assigned. Rebuild it before duplicating a child template;
	// otherwise newly configured fields can be skipped and serialized as null overrides.
	CastChecked<UBlueprintGeneratedClass>(DealerDefaults->GetClass())->UpdateCustomPropertyListForPostConstruction();
	return true;
}

bool RefreshDealerChildTemplates(UBlueprint* Table, UBlueprint* Dealer, FString& OutError)
{
	ABlackjackDealerVisual* Defaults = Cast<ABlackjackDealerVisual>(Dealer->GeneratedClass->GetDefaultObject());
	if (Defaults)
	{
		CastChecked<UBlueprintGeneratedClass>(Dealer->GeneratedClass.Get())->UpdateCustomPropertyListForPostConstruction();
	}
	auto RefreshChild = [&](UChildActorComponent* Child)
	{
		Child->SetChildActorClass(Dealer->GeneratedClass.Get(), Defaults);
		const ABlackjackDealerVisual* Template = Cast<ABlackjackDealerVisual>(Child->GetChildActorTemplate());
		if (!Template || Template->ShuffleCardBackMaterial != Defaults->ShuffleCardBackMaterial
			|| Template->ShuffleBackTexture != Defaults->ShuffleBackTexture
			|| Template->ShuffleDeckMaterial != Defaults->ShuffleDeckMaterial)
		{
			OutError = FString::Printf(TEXT("Refined child template did not inherit configured shuffle materials/texture: %s"), *Child->GetPathName());
			return false;
		}
		return true;
	};
	int32 Found = 0;
	if (Table->SimpleConstructionScript && Defaults)
	{
		for (USCS_Node* Node : Table->SimpleConstructionScript->GetAllNodes())
		{
			if (UChildActorComponent* Child = Cast<UChildActorComponent>(Node->ComponentTemplate);
				Child && Node->GetVariableName() == TEXT("BP_NPC_BlackjackDealer"))
			{
				if (!RefreshChild(Child)) { return false; }
				++Found;
			}
		}
	}
	if (Found != 1)
	{
		OutError = TEXT("Refinement expected exactly one existing table dealer child template.");
		return false;
	}
	// The compiled component template can differ from its editable SCS template.
	TArray<UChildActorComponent*> DefaultChildren;
	CastChecked<AActor>(Table->GeneratedClass->GetDefaultObject())->GetComponents(DefaultChildren);
	for (UChildActorComponent* Child : DefaultChildren)
	{
		if (Child->GetChildActorClass() == Dealer->GeneratedClass.Get())
		{
			if (!RefreshChild(Child)) { return false; }
		}
	}
	return true;
}
}

bool MigrateBlackjackDealerBlueprints(FString& OutError)
{
	UBlueprint* Dealer = LoadObject<UBlueprint>(nullptr, *ObjectPath(DealerPath));
	UBlueprint* Table = LoadObject<UBlueprint>(nullptr, *ObjectPath(TablePath));
	UBlueprint* Notify = LoadObject<UBlueprint>(nullptr, *ObjectPath(NotifyPath));
	UBlueprint* Card = LoadObject<UBlueprint>(nullptr, *ObjectPath(CardPath));
	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/Animation/Dealer/ABP_BlackjackDealer_WBG.ABP_BlackjackDealer_WBG"));
	UAnimMontage* DealMontage = LoadObject<UAnimMontage>(nullptr,
		*ObjectPath(RetargetFolder + TEXT("/AS_Dealer_BJ_Card_Deal_P4_Montage_WBG")));
	if (!Dealer || !Table || !Notify || !Card || !AnimBP || AnimBP->Status != BS_UpToDate || !AnimBP->GeneratedClass || !DealMontage)
	{
		OutError = TEXT("Dealer/table/notify or generated animation assets are missing or not compiled.");
		return false;
	}
	AActor* OldDefaults = Cast<AActor>(Dealer->GeneratedClass->GetDefaultObject());
	USkeletalMeshComponent* OldMesh = nullptr;
	if (ACharacter* Character = Cast<ACharacter>(OldDefaults))
	{
		OldMesh = Character->GetMesh();
	}
	else if (ABlackjackDealerVisual* Visual = Cast<ABlackjackDealerVisual>(OldDefaults))
	{
		OldMesh = Visual->GetMesh();
	}
	if (!OldMesh || !OldMesh->GetSkeletalMeshAsset() || OldMesh->GetSkeletalMeshAsset()->GetSkeleton() != DealMontage->GetSkeleton())
	{
		OutError = TEXT("Existing dealer mesh is missing or incompatible with the retargeted deal montage.");
		return false;
	}
	USkeletalMesh* MeshAsset = OldMesh->GetSkeletalMeshAsset();
	const FTransform MeshTransform = OldMesh->GetRelativeTransform();
	const TArray<UMaterialInterface*> Materials = OldMesh->GetMaterials();
	UAnimMontage* ShuffleMontage = BuildShuffleMontage(MeshAsset, OutError);
	if (!ShuffleMontage)
	{
		return false;
	}

	Dealer->Modify();
	ClearOldInheritedOverrides(Dealer);
	Dealer->ParentClass = ABlackjackDealerVisual::StaticClass();
	if (!RewriteDealerRelease(Dealer, OutError))
	{
		return false;
	}
	FBlueprintEditorUtils::RefreshAllNodes(Dealer);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Dealer);
	if (!Compile(Dealer, OutError))
	{
		return false;
	}
	ABlackjackDealerVisual* Defaults = Cast<ABlackjackDealerVisual>(Dealer->GeneratedClass->GetDefaultObject());
	USceneComponent* Root = Defaults ? Cast<USceneComponent>(Defaults->GetDefaultSubobjectByName(TEXT("SceneRoot"))) : nullptr;
	if (!Defaults || !Defaults->Mesh || !Root)
	{
		OutError = TEXT("Reparented dealer does not have its native scene root and skeletal mesh.");
		return false;
	}
	Defaults->SetRootComponent(Root);
	Defaults->Mesh->SetupAttachment(Root);
	Defaults->Mesh->SetSkeletalMeshAsset(MeshAsset);
	Defaults->Mesh->SetRelativeTransform(MeshTransform);
	Defaults->Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Defaults->Mesh->SetAnimInstanceClass(AnimBP->GeneratedClass);
	Defaults->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Defaults->Mesh->SetGenerateOverlapEvents(false);
	Defaults->Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		Defaults->Mesh->SetMaterial(Index, Materials[Index]);
	}
	Defaults->DealMontage = DealMontage;
	Defaults->ShuffleMontage = ShuffleMontage;
	Defaults->DealPlayRate = 1.25f;
	Defaults->PrimaryActorTick.bCanEverTick = false;
	Defaults->SetReplicates(true);
	Defaults->SetReplicateMovement(false);
	CastChecked<UBlueprintGeneratedClass>(Dealer->GeneratedClass.Get())->UpdateCustomPropertyListForPostConstruction();
	TArray<UActorComponent*> Components;
	Defaults->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component != Root && Component != Defaults->Mesh
			&& Component != Defaults->ShuffleLeftDeck && Component != Defaults->ShuffleRightDeck
			&& Component != Defaults->ShuffleLeftDeckBack && Component != Defaults->ShuffleRightDeckBack)
		{
			OutError = FString::Printf(TEXT("Unexpected old NPC component survived reparenting: %s"), *Component->GetName());
			return false;
		}
	}

	Table->Modify();
	if (!RewriteCharacterReferences(Table, DealMontage, OutError)
		|| !AddProductionPresentation(Table, TEXT("DealVisualCardToPlayer"), OutError)
		|| !AddProductionPresentation(Table, TEXT("DealVisualCardToDealer"), OutError)
		|| !MoveSplitReplacementGuard(Table, OutError)
		|| !BindPresentationReset(Table, OutError))
	{
		return false;
	}
	int32 DealerComponents = 0;
	if (Table->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Table->SimpleConstructionScript->GetAllNodes())
		{
			if (UChildActorComponent* Child = Cast<UChildActorComponent>(Node->ComponentTemplate);
				Child && Node->GetVariableName() == TEXT("BP_NPC_BlackjackDealer"))
			{
				Child->SetChildActorClass(Dealer->GeneratedClass.Get(), Defaults);
				++DealerComponents;
			}
		}
	}
	if (DealerComponents != 1)
	{
		OutError = TEXT("Expected exactly one existing dealer ChildActorComponent on the table.");
		return false;
	}
	FBlueprintEditorUtils::RefreshAllNodes(Table);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Table);
	if (!Compile(Table, OutError) || !SetTableTiming(Table, Defaults, OutError))
	{
		return false;
	}
	FBlueprintEditorUtils::RefreshAllNodes(Notify);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Notify);
	if (!Compile(Notify, OutError))
	{
		return false;
	}
	if (!ConfigureCardFlightDuration(Card, OutError))
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Card);
	if (!Compile(Card, OutError))
	{
		return false;
	}
	AActor* CardDefaults = Cast<AActor>(Card->GeneratedClass->GetDefaultObject());
	if (!CardDefaults)
	{
		OutError = TEXT("The existing card Blueprint does not derive from Actor.");
		return false;
	}
	// Presentation events run locally on every client; replicating the server's card too
	// would create a second visual actor at the same destination.
	FBoolProperty* CardReplication = FindFProperty<FBoolProperty>(Card->GeneratedClass, TEXT("bReplicates"));
	FBoolProperty* CardMovementReplication = FindFProperty<FBoolProperty>(Card->GeneratedClass, TEXT("bReplicateMovement"));
	if (!CardReplication || !CardMovementReplication)
	{
		OutError = TEXT("Card actor replication default properties were not found.");
		return false;
	}
	// Editing defaults uses reflected properties; runtime setters expect an initialized actor.
	CardReplication->SetPropertyValue_InContainer(CardDefaults, false);
	CardMovementReplication->SetPropertyValue_InContainer(CardDefaults, false);
	CastChecked<UBlueprintGeneratedClass>(Card->GeneratedClass.Get())->UpdateCustomPropertyListForPostConstruction();
	// Do not persist any migrated Blueprint until all four compile successfully.
	return SaveAsset(ShuffleMontage, OutError) && SaveAsset(Dealer, OutError)
		&& SaveAsset(Table, OutError) && SaveAsset(Notify, OutError) && SaveAsset(Card, OutError);
}

bool ExtendBlackjackDealerPresentation(FString& OutError)
{
	UBlueprint* Dealer = LoadObject<UBlueprint>(nullptr, *ObjectPath(DealerPath));
	UBlueprint* Table = LoadObject<UBlueprint>(nullptr, *ObjectPath(TablePath));
	if (!Dealer || !Table || !Dealer->GeneratedClass || !Dealer->GeneratedClass->IsChildOf(ABlackjackDealerVisual::StaticClass()))
	{
		OutError = TEXT("Migrate the dealer to its visual Actor parent before running presentation refinement.");
		return false;
	}
	Dealer->Modify();
	FBlueprintEditorUtils::RefreshAllNodes(Dealer);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Dealer);
	if (!Compile(Dealer, OutError))
	{
		return false;
	}
	ABlackjackDealerVisual* DealerDefaults = Cast<ABlackjackDealerVisual>(Dealer->GeneratedClass->GetDefaultObject());
	if (!ConfigureShuffleProps(DealerDefaults, Table, OutError))
	{
		return false;
	}
	Table->Modify();
	if (!WireProductionSeatIndex(Table, TEXT("DealVisualCardToPlayer"), true, OutError)
		|| !WireProductionSeatIndex(Table, TEXT("DealVisualCardToDealer"), false, OutError)
		|| !RefreshDealerChildTemplates(Table, Dealer, OutError))
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Table);
	if (!Compile(Table, OutError))
	{
		return false;
	}
	// Refresh again after table compilation/reinstancing so every spawned child inherits
	// the configured props and material references, rather than a stale instance override.
	if (!ConfigureShuffleProps(Cast<ABlackjackDealerVisual>(Dealer->GeneratedClass->GetDefaultObject()), Table, OutError)
		|| !RefreshDealerChildTemplates(Table, Dealer, OutError)
		|| !SetTableTiming(Table, Cast<ABlackjackDealerVisual>(Dealer->GeneratedClass->GetDefaultObject()), OutError))
	{
		return false;
	}
	return SaveAsset(Dealer, OutError) && SaveAsset(Table, OutError);
}
