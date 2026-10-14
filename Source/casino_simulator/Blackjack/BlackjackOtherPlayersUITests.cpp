// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blackjack/BlackjackUIBlueprintLibrary.h"
#include "Blackjack/BlackjackBlueprintLibrary.h"
#include "Blackjack/BlackjackPlayerComponent.h"
#include "Blackjack/BlackjackTableActor.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UI/CasinoUIManagerComponent.h"
#include "UObject/UnrealType.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"

namespace
{
void CollectCardWidgets(UWidget* Widget, UClass* CardClass, TArray<UUserWidget*>& OutCards)
{
	if (Widget && Widget->IsA(CardClass))
	{
		OutCards.Add(CastChecked<UUserWidget>(Widget));
		return;
	}
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (UWidget* Child : Panel->GetAllChildren()) CollectCardWidgets(Child, CardClass, OutCards);
	}
}

UTextBlock* HeaderAt(UScrollBox* Rows, int32 Index)
{
	UBorder* Frame = Rows ? Cast<UBorder>(Rows->GetChildAt(Index)) : nullptr;
	UVerticalBox* Row = Frame ? Cast<UVerticalBox>(Frame->GetContent()) : nullptr;
	return Row ? Cast<UTextBlock>(Row->GetChildAt(0)) : nullptr;
}

UTextBlock* WagerAt(UScrollBox* Rows, int32 Index)
{
	UBorder* Frame = Rows ? Cast<UBorder>(Rows->GetChildAt(Index)) : nullptr;
	UVerticalBox* Row = Frame ? Cast<UVerticalBox>(Frame->GetContent()) : nullptr;
	return Row ? Cast<UTextBlock>(Row->GetChildAt(1)) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackOtherPlayersUITest,
	"Casino.Blackjack.OtherPlayersUI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackOtherPlayersUITest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* CharacterClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
		TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
	if (!TestNotNull(TEXT("Playable character class"), CharacterClass)
		|| !TestNotNull(TEXT("Playable controller class"), ControllerClass)) return false;
	ACasinoLoopGameState* State = World->SpawnActor<ACasinoLoopGameState>();
	ABlackjackTableActor* Table = World->SpawnActor<ABlackjackTableActor>();
	if (!TestNotNull(TEXT("Game state"), State) || !TestNotNull(TEXT("Table"), Table)) return false;
	World->SetGameState(State);
	State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
	Table->DispatchBeginPlay();
	Acasino_simulatorCharacter* Players[4];
	const TCHAR* Names[] = {TEXT("Viewer"), TEXT("Alice"), TEXT("Bob"), TEXT("Charlie")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Players[Index] = World->SpawnActor<Acasino_simulatorCharacter>(
			CharacterClass, FVector(Index * 50.0f, 0.0f, 100.0f), FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("Seated player"), Players[Index])) return false;
		auto* ASC = Players[Index]->GetAbilitySystemComponent();
		ASC->InitAbilityActorInfo(Players[Index], Players[Index]);
		ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
		ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 10000.0f);
		APlayerState* PlayerState = World->SpawnActor<APlayerState>();
		PlayerState->SetPlayerName(Names[Index]);
		Players[Index]->SetPlayerState(PlayerState);
		if (!TestTrue(TEXT("Fixture seat claimed"), Table->TryClaimSeat(Players[Index], Index))) return false;
	}
	Acasino_simulatorPlayerController* Controller = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass,
		FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Local controller"), Controller)) return false;
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->Player->PlayerController = Controller;
	Controller->SetAsLocalPlayerController();
	World->AddController(Controller);
	Controller->SpawnPlayerCameraManager();
	Controller->Possess(Players[0]);
	Controller->UIScreen = ECasinoUIScreen::Playing;
	Controller->UIManager->UIScreen = ECasinoUIScreen::Playing;
	Players[0]->GetBlackjackPlayerComponent()->EnterBlackjackSeatMode(Table, 0);

	UClass* HUDClass = LoadClass<UUserWidget>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackHUD.WBP_BlackjackHUD_C"));
	UClass* CardClass = LoadClass<UUserWidget>(nullptr,
		TEXT("/Game/Blackjack/BluePrint/Widget/WBP_BlackjackCardUI.WBP_BlackjackCardUI_C"));
	if (!TestNotNull(TEXT("Shipped HUD class"), HUDClass) || !TestNotNull(TEXT("Shipped card class"), CardClass)) return false;
	UUserWidget* HUD = CreateWidget<UUserWidget>(Controller, HUDClass);
	if (!TestNotNull(TEXT("Shipped HUD"), HUD)) return false;
	for (const auto& Reference : TArray<TPair<FName, UObject*>>{
		{TEXT("Player"), Players[0]}, {TEXT("BlackjackComponent"), Players[0]->GetBlackjackPlayerComponent()}, {TEXT("TableRef"), Table}})
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(HUDClass, Reference.Key);
		if (!TestNotNull(TEXT("HUD context property"), Property)) return false;
		Property->SetObjectPropertyValue_InContainer(HUD, Reference.Value);
	}
	if (!TestTrue(TEXT("HUD opens through UI manager"), Controller->UIManager->ShowInteractionUI(HUD))) return false;
	HUD->TakeWidget();
	UVerticalBox* List = Cast<UVerticalBox>(HUD->GetWidgetFromName(TEXT("OtherPlayersList")));
	UBorder* Panel = Cast<UBorder>(HUD->GetWidgetFromName(TEXT("OtherPlayersPanel")));
	UFunction* Refresh = HUD->FindFunction(TEXT("RefreshBlackjackHUD"));
	if (!TestNotNull(TEXT("Other player list"), List) || !TestNotNull(TEXT("Other player panel"), Panel)
		|| !TestNotNull(TEXT("HUD refresh"), Refresh)) return false;
	HUD->ProcessEvent(Refresh, nullptr);
	UScrollBox* Rows = Cast<UScrollBox>(List->GetChildAt(0));
	if (!TestNotNull(TEXT("Other seats use a bounded scrolling list"), Rows)) return false;
	TestEqual(TEXT("Initial refresh shows every occupied other seat"), Rows->GetChildrenCount(), 3);
	if (!TestNotNull(TEXT("First seat header"), HeaderAt(Rows, 0))) return false;
	TestTrue(TEXT("Other seats are ordered and show remote player names"), HeaderAt(Rows, 0)->GetText().ToString().Contains(TEXT("Alice")));
	if (!TestNotNull(TEXT("Seat wager label"), WagerAt(Rows, 1))) return false;
	TestTrue(TEXT("Unbet occupied seat displays zero"), WagerAt(Rows, 1)->GetText().ToString().Contains(TEXT("0")));

	TArray<FBlackjackSeatState>& Seats = const_cast<TArray<FBlackjackSeatState>&>(Table->GetSeats());
	FBlackjackHand First;
	First.Cards = {{EBlackjackSuit::Spades, EBlackjackRank::Ten}, {EBlackjackSuit::Hearts, EBlackjackRank::Seven}};
	First.BetAmount = 200;
	First.bFromSplit = true;
	First.bDoubledDown = true;
	FBlackjackHand Second;
	Second.Cards = {{EBlackjackSuit::Clubs, EBlackjackRank::Ace}, {EBlackjackSuit::Diamonds, EBlackjackRank::Nine}};
	Second.BetAmount = 100;
	Second.bFromSplit = true;
	Seats[1].BetAmount = 100;
	Seats[1].InsuranceBetAmount = 50;
	Seats[1].Hands = {First, Second};
	Seats[1].ActiveHandIndex = 1;
	Seats[3].BetAmount = 400;
	FBlackjackHand Hidden;
	Hidden.Cards = {{EBlackjackSuit::Clubs, EBlackjackRank::Ten, false}, {EBlackjackSuit::Diamonds, EBlackjackRank::Two}};
	Hidden.BetAmount = 400;
	Seats[3].Hands = {Hidden};
	FEnumProperty* Phase = FindFProperty<FEnumProperty>(Table->GetClass(), TEXT("RoundState"));
	FIntProperty* Active = FindFProperty<FIntProperty>(Table->GetClass(), TEXT("ActiveSeatIndex"));
	if (!TestNotNull(TEXT("Round phase property"), Phase) || !TestNotNull(TEXT("Active seat property"), Active)) return false;
	Phase->GetUnderlyingProperty()->SetIntPropertyValue(Phase->ContainerPtrToValuePtr<void>(Table),
		static_cast<int64>(EBlackjackRoundState::PlayerTurns));
	Active->SetPropertyValue_InContainer(Table, 1);
	Table->OnTableChanged.Broadcast(); // Use the actual Blueprint's bound update path.
	Rows = Cast<UScrollBox>(List->GetChildAt(0));
	if (!TestNotNull(TEXT("Updated rows"), Rows)) return false;
	TestTrue(TEXT("Split and double total is 300 rather than original 100"), WagerAt(Rows, 0)->GetText().ToString().Contains(TEXT("300")));
	TestTrue(TEXT("Insurance is listed separately"), WagerAt(Rows, 0)->GetText().ToString().Contains(TEXT("50")));
	TestTrue(TEXT("Third other seat retains its own wager"), WagerAt(Rows, 2)->GetText().ToString().Contains(TEXT("400")));
	TArray<UUserWidget*> Cards;
	CollectCardWidgets(Rows, CardClass, Cards);
	TestEqual(TEXT("Both split hands and the other player's hand are rendered"), Cards.Num(), 6);
	if (!TestEqual(TEXT("Six card widgets exist"), Cards.Num(), 6)) return false;
	FObjectPropertyBase* VisualTableProperty = FindFProperty<FObjectPropertyBase>(CardClass, TEXT("CardVisualDataTable"));
	FObjectPropertyBase* BackTexture = FindFProperty<FObjectPropertyBase>(CardClass, TEXT("CardBackTexture"));
	FProperty* DeckProperty = FindFProperty<FProperty>(CardClass, TEXT("DeckStyle"));
	if (!TestNotNull(TEXT("Card visual table property"), VisualTableProperty)
		|| !TestNotNull(TEXT("Card back texture"), BackTexture)
		|| !TestNotNull(TEXT("Card deck style"), DeckProperty)) return false;
	UObject* Defaults = CardClass->GetDefaultObject();
	const FNumericProperty* DeckNumber = CastField<FNumericProperty>(DeckProperty);
	if (FEnumProperty* DeckEnum = CastField<FEnumProperty>(DeckProperty)) DeckNumber = DeckEnum->GetUnderlyingProperty();
	if (!TestNotNull(TEXT("Numeric deck style storage"), DeckNumber)) return false;
	const EBlackjackDeckStyle Style = static_cast<EBlackjackDeckStyle>(DeckNumber->GetSignedIntPropertyValue(
		DeckProperty->ContainerPtrToValuePtr<void>(Defaults)));
	UDataTable* VisualTable = Cast<UDataTable>(VisualTableProperty->GetObjectPropertyValue_InContainer(Defaults));
	const TArray<FBlackjackCard> ExpectedCards = {First.Cards[0], First.Cards[1], Second.Cards[0],
		Second.Cards[1], Hidden.Cards[0], Hidden.Cards[1]};
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		UImage* Image = Cast<UImage>(Cards[Index]->GetWidgetFromName(TEXT("Image_Card")));
		if (!TestNotNull(TEXT("Rendered card image"), Image)) return false;
		FBlackjackCardVisualData Visual;
		if (!TestTrue(TEXT("Expected card visual exists"),
			UBlackjackBlueprintLibrary::FindBlackjackCardVisualData(VisualTable, Style, ExpectedCards[Index], Visual))) return false;
		UObject* ExpectedTexture = ExpectedCards[Index].bFaceUp ? Visual.FrontTexture.Get()
			: BackTexture->GetObjectPropertyValue_InContainer(Cards[Index]);
		TestNotNull(TEXT("Expected visible card texture"), ExpectedTexture);
		TestTrue(TEXT("Every seat and split card uses its correct face or back image"),
			Image->GetBrush().GetResourceObject() == ExpectedTexture);
	}
	for (int32 Repeat = 0; Repeat < 3; ++Repeat) Table->OnTableChanged.Broadcast();
	Rows = Cast<UScrollBox>(List->GetChildAt(0));
	TestEqual(TEXT("Repeated snapshots do not duplicate seats"), Rows->GetChildrenCount(), 3);
	Cards.Reset();
	CollectCardWidgets(Rows, CardClass, Cards);
	TestEqual(TEXT("Repeated snapshots do not duplicate cards"), Cards.Num(), 6);

	UVerticalBox* OwnPanels[] = {
		Cast<UVerticalBox>(HUD->GetWidgetFromName(TEXT("Hand1Panel"))),
		Cast<UVerticalBox>(HUD->GetWidgetFromName(TEXT("Hand2Panel")))};
	UTextBlock* OwnResults[] = {
		Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("BlackjackOwnHand1Result"))),
		Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("BlackjackOwnHand2Result")))};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!TestNotNull(TEXT("Shipped own-hand panel"), OwnPanels[Index])
			|| !TestNotNull(TEXT("Own-hand result label"), OwnResults[Index])) return false;
		TestTrue(TEXT("Result belongs to its own hand panel"), OwnResults[Index]->GetParent() == OwnPanels[Index]);
		TestTrue(TEXT("Missing hand has no old result"), OwnResults[Index]->GetText().IsEmpty());
		TestTrue(TEXT("Missing hand result is collapsed"), OwnResults[Index]->GetVisibility() == ESlateVisibility::Collapsed);
	}
	Seats[0].Hands = {First, Second};
	Seats[0].Hands[0].LastResult = EBlackjackSeatResult::PlayerWin;
	Seats[0].Hands[1].LastResult = EBlackjackSeatResult::DealerWin;
	Seats[0].LastResult = EBlackjackSeatResult::DealerWin; // The seat summary is only the final split hand.
	Phase->GetUnderlyingProperty()->SetIntPropertyValue(Phase->ContainerPtrToValuePtr<void>(Table),
		static_cast<int64>(EBlackjackRoundState::RoundComplete));
	Table->OnTableChanged.Broadcast();
	TestEqual(TEXT("First split hand keeps its own win"), OwnResults[0]->GetText().ToString(), FString(TEXT("승리")));
	TestEqual(TEXT("Second split hand keeps its own loss"), OwnResults[1]->GetText().ToString(), FString(TEXT("패배")));
	TestTrue(TEXT("Settled result does not intercept mouse input"),
		OwnResults[0]->GetVisibility() == ESlateVisibility::HitTestInvisible);
	for (UTextBlock* OwnResult : OwnResults)
	{
		for (UWidget* Parent = OwnResult->GetParent(); Parent; Parent = Parent->GetParent())
		{
			TestTrue(FString::Printf(TEXT("Settled own result's parent %s remains visible"), *Parent->GetName()),
				Parent->GetVisibility() != ESlateVisibility::Collapsed && Parent->GetVisibility() != ESlateVisibility::Hidden);
		}
	}
	UTextBlock* OwnScores[] = {
		Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("Text_Hand1Value"))),
		Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("Text_Hand2Value")))};
	if (!TestNotNull(TEXT("First hand score"), OwnScores[0]) || !TestNotNull(TEXT("Second hand score"), OwnScores[1])) return false;
	TestFalse(TEXT("First hand score does not reserve a trailing empty line"), OwnScores[0]->GetText().ToString().EndsWith(TEXT("\n")));
	if (UTextBlock* DealerScore = Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("DealerInfoText"))))
		TestFalse(TEXT("Dealer score does not reserve a trailing empty line"), DealerScore->GetText().ToString().EndsWith(TEXT("\n")));
	TestTrue(TEXT("First hand still displays its point value"), OwnScores[0]->GetText().ToString().Contains(TEXT("17")));
	TestTrue(TEXT("Second hand still displays its point value"), OwnScores[1]->GetText().ToString().Contains(TEXT("20")));
	Cards.Reset();
	CollectCardWidgets(OwnPanels[0], CardClass, Cards);
	CollectCardWidgets(OwnPanels[1], CardClass, Cards);
	TestEqual(TEXT("Own split cards remain visible beside results"), Cards.Num(), 4);
	const TPair<EBlackjackSeatResult, FString> ResultCases[] = {
		{EBlackjackSeatResult::PlayerBlackjack, TEXT("블랙잭 승리")},
		{EBlackjackSeatResult::PlayerBust, TEXT("버스트")},
		{EBlackjackSeatResult::Push, TEXT("무승부")}};
	for (const auto& ResultCase : ResultCases)
	{
		Seats[0].Hands[0].LastResult = ResultCase.Key;
		Table->OnTableChanged.Broadcast();
		TestEqual(TEXT("Own result displays the server's settled outcome"), OwnResults[0]->GetText().ToString(), ResultCase.Value);
	}
	Seats[0].Hands[0].Cards = {{EBlackjackSuit::Spades, EBlackjackRank::Ace}, {EBlackjackSuit::Hearts, EBlackjackRank::King}};
	Seats[0].Hands[0].LastResult = EBlackjackSeatResult::None;
	Table->OnTableChanged.Broadcast();
	TestTrue(TEXT("Unsettled 21 does not invent a win"), OwnResults[0]->GetText().IsEmpty());
	Seats[0].Hands.SetNum(1);
	Table->OnTableChanged.Broadcast();
	TestTrue(TEXT("Removed split hand clears its result"), OwnResults[1]->GetText().IsEmpty());
	TestTrue(TEXT("Removed split hand collapses its result"), OwnResults[1]->GetVisibility() == ESlateVisibility::Collapsed);
	Seats[0].Hands = {First, Second};
	Seats[0].Hands[0].LastResult = EBlackjackSeatResult::PlayerWin;
	Seats[0].Hands[1].LastResult = EBlackjackSeatResult::DealerWin;
	const int32 OwnChildCounts[] = {OwnPanels[0]->GetChildrenCount(), OwnPanels[1]->GetChildrenCount()};
	for (int32 Repeat = 0; Repeat < 3; ++Repeat) Table->OnTableChanged.Broadcast();
	for (int32 Index = 0; Index < 2; ++Index)
	{
		TestEqual(TEXT("Repeated results do not accumulate widgets"), OwnPanels[Index]->GetChildrenCount(), OwnChildCounts[Index]);
		TestTrue(TEXT("Repeated results reuse the label"),
			HUD->GetWidgetFromName(Index == 0 ? TEXT("BlackjackOwnHand1Result") : TEXT("BlackjackOwnHand2Result")) == OwnResults[Index]);
	}

	// A separate table snapshot drives the same RepNotify path used on a client.
	// This checks UI parity without pretending to exercise network transport.
	ABlackjackTableActor* Replica = World->SpawnActor<ABlackjackTableActor>();
	Replica->DispatchBeginPlay();
	const_cast<TArray<FBlackjackSeatState>&>(Replica->GetSeats()) = Seats;
	Phase->GetUnderlyingProperty()->SetIntPropertyValue(Phase->ContainerPtrToValuePtr<void>(Replica),
		static_cast<int64>(EBlackjackRoundState::RoundComplete));
	FindFProperty<FObjectProperty>(HUDClass, TEXT("TableRef"))->SetObjectPropertyValue_InContainer(HUD, Replica);
	FScriptDelegate RefreshDelegate;
	RefreshDelegate.BindUFunction(HUD, TEXT("HandleBlackjackTableChanged"));
	Replica->OnTableChanged.AddUnique(RefreshDelegate);
	UFunction* RepNotify = Replica->FindFunction(TEXT("OnRep_TableState"));
	if (!TestNotNull(TEXT("Replicated table notification"), RepNotify)) return false;
	Replica->ProcessEvent(RepNotify, nullptr);
	Rows = Cast<UScrollBox>(List->GetChildAt(0));
	TestEqual(TEXT("Replicated snapshot displays all other seats"), Rows->GetChildrenCount(), 3);
	TestTrue(TEXT("Replicated snapshot displays the split wager total"), WagerAt(Rows, 0)->GetText().ToString().Contains(TEXT("300")));
	TestEqual(TEXT("RepNotify refreshes the first own split result"), OwnResults[0]->GetText().ToString(), FString(TEXT("승리")));
	TestEqual(TEXT("RepNotify refreshes the second own split result"), OwnResults[1]->GetText().ToString(), FString(TEXT("패배")));
	const_cast<TArray<FBlackjackSeatState>&>(Replica->GetSeats())[3].Occupant = nullptr;
	Replica->ProcessEvent(RepNotify, nullptr);
	Rows = Cast<UScrollBox>(List->GetChildAt(0));
	TestEqual(TEXT("Replicated departure removes the stale seat row"), Rows->GetChildrenCount(), 2);
	Cards.Reset();
	CollectCardWidgets(Rows, CardClass, Cards);
	TestEqual(TEXT("Replicated departure removes the departed player's cards"), Cards.Num(), 4);
	const_cast<TArray<FBlackjackSeatState>&>(Replica->GetSeats())[0].Occupant = nullptr;
	Replica->ProcessEvent(RepNotify, nullptr);
	TestTrue(TEXT("Released local seat clears its first result"), OwnResults[0]->GetText().IsEmpty());
	TestTrue(TEXT("Released local seat clears its second result"), OwnResults[1]->GetText().IsEmpty());
	const_cast<TArray<FBlackjackSeatState>&>(Replica->GetSeats())[0].Occupant = Players[0];
	Replica->ProcessEvent(RepNotify, nullptr);
	Replica->ResetRound();
	Rows = Cast<UScrollBox>(List->GetChildAt(0));
	Cards.Reset();
	CollectCardWidgets(Rows, CardClass, Cards);
	TestEqual(TEXT("New round removes all old cards"), Cards.Num(), 0);
	TestTrue(TEXT("New round resets the displayed wager"), WagerAt(Rows, 0)->GetText().ToString().Contains(TEXT("0")));
	for (UTextBlock* OwnResult : OwnResults)
	{
		TestTrue(TEXT("New round clears the own settled result"), OwnResult->GetText().IsEmpty());
		TestTrue(TEXT("New round collapses the own settled result"), OwnResult->GetVisibility() == ESlateVisibility::Collapsed);
	}
	const_cast<TArray<FBlackjackSeatState>&>(Replica->GetSeats())[0].Hands = Seats[0].Hands;
	Replica->ProcessEvent(RepNotify, nullptr);
	TestFalse(TEXT("Own result is restored before context loss"), OwnResults[0]->GetText().IsEmpty());
	TestFalse(TEXT("Missing table fails safely"),
		UBlackjackUIBlueprintLibrary::RefreshOtherPlayers(HUD, nullptr, Players[0], List, Panel));
	TestEqual(TEXT("Missing table clears old rows"), List->GetChildrenCount(), 0);
	TestTrue(TEXT("Missing table collapses its panel"), Panel->GetVisibility() == ESlateVisibility::Collapsed);
	for (UTextBlock* OwnResult : OwnResults)
	{
		TestTrue(TEXT("Lost table clears the own settled result"), OwnResult->GetText().IsEmpty());
		TestTrue(TEXT("Lost table collapses the own settled result"), OwnResult->GetVisibility() == ESlateVisibility::Collapsed);
	}
	Controller->UIManager->CloseInteractionUI(HUD);
	return true;
}

#endif
