// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackUIBlueprintLibrary.h"

#include "Blackjack/BlackjackTableActor.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "casino_simulatorCharacter.h"

#define LOCTEXT_NAMESPACE "BlackjackUI"

namespace
{
	struct FExistingCardRenderer
	{
		UFunction* Function = nullptr;
		FArrayProperty* Cards = nullptr;
		FObjectPropertyBase* TargetPanel = nullptr;

		bool Find(UUserWidget* HUD)
		{
			Function = HUD->FindFunction(TEXT("RenderCards"));
			if (!Function)
			{
				return false;
			}

			Cards = FindFProperty<FArrayProperty>(Function, TEXT("Cards"));
			TargetPanel = FindFProperty<FObjectPropertyBase>(Function, TEXT("TargetPanel"));
			const FStructProperty* CardElement = Cards ? CastField<FStructProperty>(Cards->Inner) : nullptr;
			if (!CardElement || CardElement->Struct != FBlackjackCard::StaticStruct()
				|| !TargetPanel || TargetPanel->PropertyClass != UHorizontalBox::StaticClass()
				|| !Cards->HasAnyPropertyFlags(CPF_Parm)
				|| !TargetPanel->HasAnyPropertyFlags(CPF_Parm)
				|| Cards->HasAnyPropertyFlags(CPF_ReturnParm)
				|| TargetPanel->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				return false;
			}

			int32 ParameterCount = 0;
			for (TFieldIterator<FProperty> Property(Function); Property; ++Property)
			{
				if (Property->HasAnyPropertyFlags(CPF_Parm))
				{
					++ParameterCount;
					if (*Property != Cards && *Property != TargetPanel)
					{
						return false;
					}
				}
			}
			return ParameterCount == 2;
		}

		void Render(UUserWidget* HUD, const FBlackjackHand& Hand, UHorizontalBox* Panel) const
		{
			// Blueprint reference flags and parameter offsets need not match a native
			// C++ parameter struct. Let the reflected properties own their storage.
			FStructOnScope Parameters(Function);
			Cards->CopyCompleteValue(Cards->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()), &Hand.Cards);
			TargetPanel->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Panel);
			HUD->ProcessEvent(Function, Parameters.GetStructMemory());
		}
	};

	template<typename TWidget>
	TWidget* ConstructNamedWidget(UWidgetTree* Tree, const FString& BaseName)
	{
		const FName Name = MakeUniqueObjectName(Tree, TWidget::StaticClass(), FName(*BaseName));
		return Tree->ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
	}

	UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Name,
		const FText& Text, int32 FontSize, const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* Label = ConstructNamedWidget<UTextBlock>(Tree, Name);
		Label->SetText(Text);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetAutoWrapText(true);
		UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Label);
		Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		return Label;
	}

	FText GetOccupantName(const FBlackjackSeatState& Seat)
	{
		const APawn* Pawn = Cast<APawn>(Seat.Occupant);
		const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
		if (IsValid(PlayerState) && !PlayerState->GetPlayerName().IsEmpty())
		{
			return FText::FromString(PlayerState->GetPlayerName());
		}
		return LOCTEXT("OtherPlayerFallback", "플레이어");
	}

	FText GetSettledHandResult(EBlackjackSeatResult Result)
	{
		switch (Result)
		{
		case EBlackjackSeatResult::PlayerWin:
			return LOCTEXT("HandPlayerWin", "승리");
		case EBlackjackSeatResult::DealerWin:
			return LOCTEXT("HandDealerWin", "패배");
		case EBlackjackSeatResult::Push:
			return LOCTEXT("HandPush", "무승부");
		case EBlackjackSeatResult::PlayerBlackjack:
			return LOCTEXT("HandBlackjackWin", "블랙잭 승리");
		case EBlackjackSeatResult::PlayerBust:
			return LOCTEXT("HandResultBust", "버스트");
		case EBlackjackSeatResult::None:
		default:
			break;
		}
		return FText::GetEmpty();
	}

	void RefreshOwnHandResults(UUserWidget* HUD, ABlackjackTableActor* Table, AActor* LocalPlayer)
	{
		if (!IsValid(HUD) || !IsValid(HUD->WidgetTree))
		{
			return;
		}

		EBlackjackSeatResult Results[2] = {EBlackjackSeatResult::None, EBlackjackSeatResult::None};
		if (IsValid(Table) && IsValid(LocalPlayer))
		{
			for (const FBlackjackSeatState& Seat : Table->GetSeats())
			{
				if (Seat.Occupant != LocalPlayer)
				{
					continue;
				}
				for (int32 HandIndex = 0; HandIndex < UE_ARRAY_COUNT(Results); ++HandIndex)
				{
					if (Seat.Hands.IsValidIndex(HandIndex))
					{
						Results[HandIndex] = Seat.Hands[HandIndex].LastResult;
					}
				}
				break;
			}
		}

		const FName PanelNames[] = {TEXT("Hand1Panel"), TEXT("Hand2Panel")};
		const FName ScoreNames[] = {TEXT("Text_Hand1Value"), TEXT("Text_Hand2Value")};
		const FName ResultNames[] = {TEXT("BlackjackOwnHand1Result"), TEXT("BlackjackOwnHand2Result")};
		for (int32 HandIndex = 0; HandIndex < UE_ARRAY_COUNT(Results); ++HandIndex)
		{
			UVerticalBox* HandPanel = Cast<UVerticalBox>(HUD->GetWidgetFromName(PanelNames[HandIndex]));
			if (!IsValid(HandPanel))
			{
				continue;
			}

			UWidget* ExistingResult = HUD->GetWidgetFromName(ResultNames[HandIndex]);
			UTextBlock* ResultLabel = Cast<UTextBlock>(ExistingResult);
			if (ExistingResult && !ResultLabel)
			{
				continue;
			}
			if (!ResultLabel)
			{
				ResultLabel = HUD->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), ResultNames[HandIndex]);
				UVerticalBoxSlot* Slot = HandPanel->AddChildToVerticalBox(ResultLabel);
				Slot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
				Slot->SetHorizontalAlignment(HAlign_Fill);
				ResultLabel->SetAutoWrapText(true);
			}
			if (UTextBlock* ScoreLabel = Cast<UTextBlock>(HUD->GetWidgetFromName(ScoreNames[HandIndex])))
			{
				ResultLabel->SetFont(ScoreLabel->GetFont());
				ResultLabel->SetColorAndOpacity(ScoreLabel->GetColorAndOpacity());
			}

			const FText Result = GetSettledHandResult(Results[HandIndex]);
			ResultLabel->SetText(Result);
			ResultLabel->SetVisibility(Result.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}

	FText GetHandStatus(ABlackjackTableActor* Table, const FBlackjackHand& Hand, bool bCurrentTurn)
	{
		const FText Result = GetSettledHandResult(Hand.LastResult);
		if (!Result.IsEmpty())
		{
			return Result;
		}
		if (Hand.Cards.ContainsByPredicate([](const FBlackjackCard& Card) { return !Card.bFaceUp; }))
		{
			return LOCTEXT("HandHiddenStatus", "미공개");
		}
		if (Table->IsHandBust(Hand))
		{
			return LOCTEXT("HandBust", "버스트");
		}
		if (Table->IsNaturalBlackjack(Hand))
		{
			return LOCTEXT("HandBlackjack", "블랙잭");
		}
		if (Hand.bStood || Table->GetHandBestValue(Hand) >= 21)
		{
			return LOCTEXT("HandComplete", "완료");
		}
		return bCurrentTurn ? LOCTEXT("HandCurrentTurn", "현재 차례") : LOCTEXT("HandWaiting", "대기");
	}
}

bool UBlackjackUIBlueprintLibrary::RefreshOtherPlayers(UUserWidget* BlackjackHUD, ABlackjackTableActor* Table,
	AActor* LocalPlayer, UVerticalBox* OtherPlayersList, UBorder* OtherPlayersPanel)
{
	RefreshOwnHandResults(BlackjackHUD, Table, LocalPlayer);

	// Clear first so a released seat or lost table reference cannot leave stale cards.
	float PreviousScrollOffset = 0.0f;
	if (IsValid(OtherPlayersList))
	{
		if (UScrollBox* PreviousScroll = Cast<UScrollBox>(OtherPlayersList->GetChildAt(0)))
		{
			PreviousScrollOffset = PreviousScroll->GetScrollOffset();
		}
		OtherPlayersList->ClearChildren();
	}
	if (IsValid(OtherPlayersPanel))
	{
		OtherPlayersPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!IsValid(BlackjackHUD) || !IsValid(BlackjackHUD->WidgetTree) || !IsValid(Table)
		|| !IsValid(LocalPlayer) || !IsValid(OtherPlayersList) || !IsValid(OtherPlayersPanel))
	{
		return false;
	}

	FExistingCardRenderer Renderer;
	if (!Renderer.Find(BlackjackHUD))
	{
		return false;
	}

	// Keep a value snapshot while the existing Blueprint renderer executes.
	TArray<FBlackjackSeatState> OtherSeats;
	for (const FBlackjackSeatState& Seat : Table->GetSeats())
	{
		if (IsValid(Seat.Occupant) && Seat.Occupant != LocalPlayer)
		{
			OtherSeats.Add(Seat);
		}
	}
	OtherSeats.Sort([](const FBlackjackSeatState& Left, const FBlackjackSeatState& Right)
	{
		return Left.SeatIndex < Right.SeatIndex;
	});

	UWidgetTree* Tree = BlackjackHUD->WidgetTree;
	const FLinearColor Background = OtherPlayersPanel->GetBrushColor();
	const bool bLightBackground = Background.A > 0.5f && Background.GetLuminance() > 0.5f;
	const FLinearColor TextColor = bLightBackground ? FLinearColor(0.06f, 0.06f, 0.06f) : FLinearColor::White;
	const FLinearColor ActiveColor = bLightBackground ? FLinearColor(0.4f, 0.2f, 0.0f) : FLinearColor(1.0f, 0.8f, 0.25f);
	UScrollBox* SeatList = ConstructNamedWidget<UScrollBox>(Tree, TEXT("BlackjackOtherSeatsScroll"));
	SeatList->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	UVerticalBoxSlot* ListSlot = OtherPlayersList->AddChildToVerticalBox(SeatList);
	ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListSlot->SetHorizontalAlignment(HAlign_Fill);
	for (const FBlackjackSeatState& Seat : OtherSeats)
	{
		const FString SeatName = FString::Printf(TEXT("BlackjackOtherSeat_%d"), Seat.SeatIndex);
		UVerticalBox* SeatRow = ConstructNamedWidget<UVerticalBox>(Tree, SeatName);
		SeatList->AddChild(SeatRow);

		int64 TotalBet = FMath::Max(0, Seat.BetAmount);
		if (!Seat.Hands.IsEmpty())
		{
			TotalBet = 0;
			for (const FBlackjackHand& Hand : Seat.Hands)
			{
				TotalBet += FMath::Max(0, Hand.BetAmount);
			}
		}
		FText Header = FText::Format(LOCTEXT("OtherSeatHeader", "{Seat}번 자리 · {Player} · 베팅 {Bet}원"),
			FFormatNamedArguments{
				{TEXT("Seat"), FText::AsNumber(Seat.SeatIndex + 1)},
				{TEXT("Player"), GetOccupantName(Seat)},
				{TEXT("Bet"), FText::AsNumber(TotalBet)}});
		if (Seat.InsuranceBetAmount > 0)
		{
			Header = FText::Format(LOCTEXT("OtherSeatInsurance", "{Header} · 보험 {Insurance}원"),
				FFormatNamedArguments{{TEXT("Header"), Header}, {TEXT("Insurance"), FText::AsNumber(Seat.InsuranceBetAmount)}});
		}
		AddText(Tree, SeatRow, SeatName + TEXT("_Header"), Header, 14, TextColor);

		if (Seat.Hands.IsEmpty())
		{
			AddText(Tree, SeatRow, SeatName + TEXT("_Waiting"),
				Seat.BetAmount > 0 ? LOCTEXT("OtherSeatWaitingCards", "카드 대기") : LOCTEXT("OtherSeatWaitingBet", "베팅 대기"), 12, TextColor);
			continue;
		}

		const bool bSeatTurn = Table->IsPlayerTurn(Cast<Acasino_simulatorCharacter>(Seat.Occupant));
		for (int32 HandIndex = 0; HandIndex < Seat.Hands.Num(); ++HandIndex)
		{
			const FBlackjackHand& Hand = Seat.Hands[HandIndex];
			const bool bCurrentTurn = bSeatTurn && Seat.ActiveHandIndex == HandIndex;
			const FString HandName = FString::Printf(TEXT("%s_Hand_%d"), *SeatName, HandIndex);
			UVerticalBox* HandRow = ConstructNamedWidget<UVerticalBox>(Tree, HandName);
			UVerticalBoxSlot* HandSlot = SeatRow->AddChildToVerticalBox(HandRow);
			HandSlot->SetHorizontalAlignment(HAlign_Fill);
			HandSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));

			const bool bHasHiddenCard = Hand.Cards.ContainsByPredicate([](const FBlackjackCard& Card) { return !Card.bFaceUp; });
			const FText Value = bHasHiddenCard ? LOCTEXT("HandHiddenValue", "미공개")
				: FText::Format(LOCTEXT("HandVisibleValue", "{0}점"), FText::AsNumber(Table->GetHandBestValue(Hand)));
			FText Status = GetHandStatus(Table, Hand, bCurrentTurn);
			if (Hand.bDoubledDown)
			{
				Status = FText::Format(LOCTEXT("HandDoubledStatus", "{0} · 더블"), Status);
			}
			const FText HandText = FText::Format(LOCTEXT("OtherSeatHand", "{Hand}번 핸드 · {Value} · 베팅 {Bet}원 · {Status}"),
				FFormatNamedArguments{
					{TEXT("Hand"), FText::AsNumber(HandIndex + 1)}, {TEXT("Value"), Value},
					{TEXT("Bet"), FText::AsNumber(FMath::Max(0, Hand.BetAmount))}, {TEXT("Status"), Status}});
			AddText(Tree, HandRow, HandName + TEXT("_Info"), HandText, 12,
				bCurrentTurn ? ActiveColor : TextColor);

			USizeBox* CardSize = ConstructNamedWidget<USizeBox>(Tree, HandName + TEXT("_CardSize"));
			CardSize->SetHeightOverride(68.0f);
			UVerticalBoxSlot* CardSlot = HandRow->AddChildToVerticalBox(CardSize);
			CardSlot->SetHorizontalAlignment(HAlign_Fill);
			UScaleBox* CardScale = ConstructNamedWidget<UScaleBox>(Tree, HandName + TEXT("_CardScale"));
			CardScale->SetStretch(EStretch::ScaleToFit);
			CardScale->SetStretchDirection(EStretchDirection::DownOnly);
			CardSize->AddChild(CardScale);
			UHorizontalBox* CardPanel = ConstructNamedWidget<UHorizontalBox>(Tree, HandName + TEXT("_Cards"));
			CardScale->AddChild(CardPanel);
			Renderer.Render(BlackjackHUD, Hand, CardPanel);
		}
	}

	SeatList->SetScrollOffset(PreviousScrollOffset);
	OtherPlayersPanel->SetVisibility(OtherSeats.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	return true;
}

#undef LOCTEXT_NAMESPACE
