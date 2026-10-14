// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blackjack/BlackjackUIBlueprintLibrary.h"

#include "Blackjack/BlackjackTableActor.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
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

void UBlackjackUIBlueprintLibrary::PrepareBetKeypadAppearance(UUserWidget* BlackjackHUD)
{
	if (!IsValid(BlackjackHUD)) return;
	UUserWidget* Keypad = Cast<UUserWidget>(BlackjackHUD->GetWidgetFromName(TEXT("WBP_Calculate")));
	if (!IsValid(Keypad) || !IsValid(Keypad->WidgetTree)) return;
	const FLinearColor Navy(.023f, .032f, .049f, 1.f);
	const FLinearColor Gold(.76f, .49f, .15f, 1.f);
	const FLinearColor Ivory(.91f, .86f, .75f, 1.f);
	Keypad->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		UButton* Button = Cast<UButton>(Widget);
		if (!Button) return;
		FButtonStyle Style = Button->GetStyle();
		if (Button->GetFName() == TEXT("Button_BG"))
		{
			// This invisible button retains its original click-to-dismiss behavior.
			if (Style.Normal.DrawAs == ESlateBrushDrawType::NoDrawType) return;
			Style.Normal.DrawAs = Style.Hovered.DrawAs = Style.Pressed.DrawAs = Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
			Button->SetStyle(Style);
			return;
		}
		const bool Primary = Button->GetFName() == TEXT("Button_OK");
		const FLinearColor Normal = Primary ? Gold : Navy;
		if (Style.Normal.TintColor.GetSpecifiedColor().Equals(Normal)) return;
		const auto ApplyBrush = [&](FSlateBrush& Brush, const FLinearColor& Color)
		{
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings = FSlateBrushOutlineSettings(8.f, FSlateColor(FLinearColor(.76f, .49f, .15f, .45f)), 1.f);
		};
		ApplyBrush(Style.Normal, Normal);
		ApplyBrush(Style.Hovered, Primary ? FLinearColor(.94f, .67f, .29f) : FLinearColor(.05f, .065f, .095f));
		ApplyBrush(Style.Pressed, Primary ? FLinearColor(.58f, .33f, .075f) : FLinearColor(.035f, .043f, .06f));
		ApplyBrush(Style.Disabled, Navy);
		Style.NormalForeground = Style.HoveredForeground = Style.PressedForeground = FSlateColor(Primary ? FLinearColor(.007f, .011f, .019f) : Ivory);
		Style.DisabledForeground = FSlateColor(FLinearColor(.39f, .46f, .55f));
		Style.NormalPadding = Style.PressedPadding = FMargin(10.f);
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::White);
		if (UTextBlock* Text = Cast<UTextBlock>(Button->GetContent()))
		{
			FSlateFontInfo Font = Text->GetFont(); Font.Size = Primary ? 22.f : 28.f;
			Text->SetFont(Font); Text->SetColorAndOpacity(FSlateColor::UseForeground());
		}
	});
}

bool UBlackjackUIBlueprintLibrary::RefreshOtherPlayers(UUserWidget* BlackjackHUD, ABlackjackTableActor* Table,
	AActor* LocalPlayer, UVerticalBox* OtherPlayersList, UBorder* OtherPlayersPanel)
{
	PrepareBetKeypadAppearance(BlackjackHUD);
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
	const FStructProperty* BackgroundProperty = FindFProperty<FStructProperty>(OtherPlayersPanel->GetClass(), TEXT("Background"));
	const FSlateBrush* PanelBrush = BackgroundProperty ? BackgroundProperty->ContainerPtrToValuePtr<FSlateBrush>(OtherPlayersPanel) : nullptr;
	const FLinearColor Background = (PanelBrush ? PanelBrush->TintColor.GetSpecifiedColor() : FLinearColor::White)
		* OtherPlayersPanel->GetBrushColor();
	const bool bLightBackground = Background.A > 0.5f && Background.GetLuminance() > 0.5f;
	const FLinearColor TextColor = bLightBackground ? FLinearColor(0.06f, 0.06f, 0.06f) : FLinearColor(.91f, .86f, .75f);
	const FLinearColor ActiveColor = bLightBackground ? FLinearColor(0.4f, 0.2f, 0.0f) : FLinearColor(.94f, .67f, .29f);
	UScrollBox* SeatList = ConstructNamedWidget<UScrollBox>(Tree, TEXT("BlackjackOtherSeatsScroll"));
	SeatList->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	SeatList->SetScrollbarThickness(FVector2D(6.f, 6.f));
	SeatList->SetScrollbarPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	FScrollBarStyle BarStyle = SeatList->GetWidgetBarStyle();
	BarStyle.NormalThumbImage.TintColor = FSlateColor(FLinearColor(.76f, .49f, .15f));
	BarStyle.HoveredThumbImage.TintColor = FSlateColor(ActiveColor);
	BarStyle.DraggedThumbImage.TintColor = FSlateColor(TextColor);
	SeatList->SetWidgetBarStyle(BarStyle);
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
		UTextBlock* SeatHeader = AddText(Tree, SeatRow, SeatName + TEXT("_Header"), Header, 16, TextColor);
		if (UVerticalBoxSlot* HeaderSlot = Cast<UVerticalBoxSlot>(SeatHeader->Slot))
			HeaderSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 6.f));

		if (Seat.Hands.IsEmpty())
		{
			AddText(Tree, SeatRow, SeatName + TEXT("_Waiting"),
				Seat.BetAmount > 0 ? LOCTEXT("OtherSeatWaitingCards", "카드 대기") : LOCTEXT("OtherSeatWaitingBet", "베팅 대기"), 14, TextColor);
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
			HandSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

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
			AddText(Tree, HandRow, HandName + TEXT("_Info"), HandText, 14,
				bCurrentTurn ? ActiveColor : TextColor);

			USizeBox* CardSize = ConstructNamedWidget<USizeBox>(Tree, HandName + TEXT("_CardSize"));
			CardSize->SetHeightOverride(80.0f);
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
