// Copyright Epic Games, Inc. All Rights Reserved.


#include "casino_simulatorPlayerHUD.h"

#include "Blueprint/WidgetTree.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemData.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "casino_simulatorPlayerState.h"

void Ucasino_simulatorPlayerHUD::RefreshConfirmedNumberSlots(Acasino_simulatorPlayerState* State)
{
	APlayerController* Owner = GetOwningPlayer();
	if (!IsValid(State) || !Owner || !Owner->IsLocalController()
		|| Owner->PlayerState != State || !WidgetTree)
		return;

	TSet<UWidgetTree*> Visited;
	TFunction<void(UWidgetTree*)> RefreshTree;
	RefreshTree = [&](UWidgetTree* Tree)
	{
		if (!Tree || Visited.Contains(Tree)) return;
		Visited.Add(Tree);
		TArray<UUserWidget*> Children;
		Tree->ForEachWidget([&](UWidget* Widget)
		{
			if (UUserWidget* Child = Cast<UUserWidget>(Widget)) Children.Add(Child);
		});
		for (UUserWidget* Child : Children)
		{
			FIntProperty* IndexProperty = FindFProperty<FIntProperty>(Child->GetClass(), TEXT("SlotNum"));
			const int32 Index = IndexProperty ? IndexProperty->GetPropertyValue_InContainer(Child) : INDEX_NONE;
			UFunction* Draw = Index >= 0 && Index < 2 && State->NumberSlots.IsValidIndex(Index)
				? Child->FindFunction(TEXT("SetDraw")) : nullptr;
			if (Draw)
			{
				FStructProperty* Input = nullptr;
				bool bSupportedSignature = true;
				for (TFieldIterator<FProperty> It(Draw); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					if (It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
					FStructProperty* Struct = CastField<FStructProperty>(*It);
					if (!Input && Struct && Struct->Struct == FItemData::StaticStruct()
						&& (!Struct->HasAnyPropertyFlags(CPF_OutParm) || Struct->HasAnyPropertyFlags(CPF_ConstParm)))
						Input = Struct;
					else bSupportedSignature = false;
				}
				FItemData Confirmed;
				if (bSupportedSignature && Input && State->FindItemData(State->NumberSlots[Index], Confirmed))
				{
					FStructOnScope Parameters(Draw);
					Input->CopyCompleteValue(Input->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()), &Confirmed);
					Child->ProcessEvent(Draw, Parameters.GetStructMemory());
				}
			}
			RefreshTree(Child->WidgetTree);
		}
	};
	RefreshTree(WidgetTree);
}

