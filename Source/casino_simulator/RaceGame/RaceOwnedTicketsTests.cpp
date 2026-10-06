#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "RaceManager.h"
#include "RaceRunner.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/TargetPoint.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceOwnedTicketsUITest, "Casino.Race.OwnedTicketsUI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaceOwnedTicketsUITest::RunTest(const FString& Parameters)
{
    auto* NPCClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/KCU/HorseRacing/WBP_RaceNPCWidget.WBP_RaceNPCWidget_C"));
    auto* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("NPC widget class"), NPCClass) || !TestNotNull(TEXT("Player class"), PlayerClass)) return false;
    auto* Portrait = LoadObject<UTexture2D>(nullptr, TEXT("/Game/KCU/HorseRacing/runner_cane_man.runner_cane_man"));
    if (!TestNotNull(TEXT("Ticket portrait"), Portrait)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    auto* Manager = World->SpawnActor<ARaceManager>();
    auto* Buyer = World->SpawnActor<APlayerState>();
    auto* OtherBuyer = World->SpawnActor<APlayerState>();
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!Manager || !Buyer || !OtherBuyer || !Player) return false;
    FindFProperty<FObjectPropertyBase>(APawn::StaticClass(), TEXT("PlayerState"))->SetObjectPropertyValue_InContainer(Player, Buyer);
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 1000.f);

    FBetTicket Pending;
    Pending.TicketId = 1;
    Pending.Buyer = Buyer;
    Pending.RoundNumber = 2;
    Pending.RunnerIndex = 1;
    Pending.RunnerName = TEXT("김철수");
    Pending.RunnerAge = 78;
    Pending.RunnerPortrait = Portrait;
    Pending.Amount = 100;
    Pending.Count = 3;
    Pending.Odds = 2.f;
    auto Other = Pending;
    Other.TicketId = 2;
    Other.Buyer = OtherBuyer;
    Other.bWon = true;
    auto Winner = Pending;
    Winner.TicketId = 3;
    Winner.RoundNumber = 1;
    Winner.RunnerName = TEXT("박영희");
    Winner.Count = 2;
    Winner.bWon = true;
    Manager->Tickets = { Pending, Other, Winner };
    Manager->CurrentRoundNumber = 2;
    TestEqual(TEXT("Unresolved player state sees no tickets"), Manager->GetTicketsForPlayer(nullptr).Num(), 0);

    auto* NPC = CreateWidget<UUserWidget>(World, NPCClass);
    if (!TestNotNull(TEXT("NPC widget"), NPC)) return false;
    FindFProperty<FObjectPropertyBase>(NPCClass, TEXT("RaceManager"))->SetObjectPropertyValue_InContainer(NPC, Manager);
    FindFProperty<FObjectPropertyBase>(NPCClass, TEXT("Player"))->SetObjectPropertyValue_InContainer(NPC, Player);
    const TSharedRef<SWidget> NPCRoot = NPC->TakeWidget();
    auto* Button = Cast<UButton>(NPC->GetWidgetFromName(TEXT("OwnedTicketsButton")));
    auto* Switcher = Cast<UWidgetSwitcher>(NPC->GetWidgetFromName(TEXT("WidgetSwitcher_13")));
    auto* Page = Cast<UUserWidget>(NPC->GetWidgetFromName(TEXT("WBP_RaceOwnedTicketsWidget")));
    if (!TestNotNull(TEXT("My tickets menu button"), Button) || !TestNotNull(TEXT("Menu switcher"), Switcher)
        || !TestNotNull(TEXT("My tickets page"), Page)) return false;
    TestTrue(TEXT("Owned ticket UI has a plain UserWidget parent"),Page->GetClass()->GetSuperClass()==UUserWidget::StaticClass());
    TestFalse(TEXT("Ticket refresh is implemented in Blueprint"),Page->FindFunctionChecked(TEXT("RefreshTicketList"))->HasAnyFunctionFlags(FUNC_Native));
    const auto Displayed = [&]() -> const TArray<FBetTicket>& { return *FindFProperty<FArrayProperty>(Page->GetClass(), TEXT("DisplayedTickets"))->ContainerPtrToValuePtr<TArray<FBetTicket>>(Page); };
    TestEqual(TEXT("NPC opens on menu"), Switcher->GetActiveWidgetIndex(), 0);

    const auto Render = [&](const TCHAR* Name,FVector2D Resolution=FVector2D(1920.f,1080.f))
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("RenderOwnedTickets"))) return;
#if WITH_EDITOR
        FTextureCompilingManager::Get().FinishCompilation({ Portrait });
#endif
        FWidgetRenderer Renderer(false, true);
        auto* Target = Renderer.DrawWidget(NPCRoot, Resolution);
        // ScaleBox updates its layout from the previous paint geometry after a resize.
        // Render a second frame, as the viewport does, before checking or exporting.
        if(Target)Renderer.DrawWidget(Target,NPCRoot,Resolution,0.f);
        if(auto* ActivePage=Cast<UUserWidget>(Switcher->GetActiveWidget()))
        {
            if(auto* BackButton=ActivePage->GetWidgetFromName(TEXT("GoBack_Button")))
            {
                const auto& Geometry=BackButton->GetCachedGeometry();
                const auto Position=Geometry.GetAbsolutePosition();const auto Size=Geometry.GetAbsoluteSize();
                TestTrue(TEXT("Back button stays inside the rendered viewport"),Position.X>=0.f&&Position.Y>=0.f&&Position.X+Size.X<=Resolution.X+1.f&&Position.Y+Size.Y<=Resolution.Y+1.f);
            }
        }
        FBufferArchive PNG;
        const bool Exported = Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG);
        TestTrue(TEXT("UI preview renders"), Exported);
        IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("RaceWidgetStyle")),true);
        if (Exported) TestTrue(TEXT("UI preview saves"), FFileHelper::SaveArrayToFile(PNG,
            *(FPaths::ProjectSavedDir() / TEXT("RaceWidgetStyle") / Name)));
    };
    Render(TEXT("menu.png"));
    Button->OnClicked.Broadcast();
    TestTrue(TEXT("NPC button opens the owned-tickets page"), Switcher->GetActiveWidget() == Page);
    if (!TestEqual(TEXT("Only this player's pending and winning tickets are shown"), Displayed().Num(), 2)) return false;
    TestEqual(TEXT("Latest purchases shown first"), Displayed()[0].TicketId, 3);
    auto* Scroll = Cast<UScrollBox>(Page->GetWidgetFromName(TEXT("ScrollBox_0")));
    auto* Empty = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("EmptyStateText")));
    if (!Scroll || !Empty || !TestEqual(TEXT("Two cards appear"), Scroll->GetChildrenCount(), 2)) return false;
    auto* Card = Cast<UUserWidget>(Scroll->GetChildAt(0));
    if (!TestNotNull(TEXT("Winning ticket card"), Card)) return false;
    TestEqual(TEXT("Stored round is shown"), CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("라운드표시")))->GetText().ToString(), FString(TEXT("제1회")));
    TestEqual(TEXT("Stored age is shown"), CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Age")))->GetText().ToString(), FString(TEXT("78세")));
    TestEqual(TEXT("Stored name is shown"), CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("Name")))->GetText().ToString(), FString(TEXT("박영희")));
    TestEqual(TEXT("Winning status is shown"), CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("TicketStatus")))->GetText().ToString(), FString(TEXT("당첨 · 환전 대기")));
    TestTrue(TEXT("Stored portrait is shown"), CastChecked<UImage>(Card->GetWidgetFromName(TEXT("RunnerImage")))->GetBrush().GetResourceObject() == Portrait);
    TestEqual(TEXT("Confirmed payout includes ticket quantity"),CastChecked<UTextBlock>(Card->GetWidgetFromName(TEXT("ExpectedPayout")))->GetText().ToString(),FString(TEXT("환전금 400원")));
    auto* PendingCard=CastChecked<UUserWidget>(Scroll->GetChildAt(1));
    TestEqual(TEXT("Pending ticket shows expected payout"),CastChecked<UTextBlock>(PendingCard->GetWidgetFromName(TEXT("ExpectedPayout")))->GetText().ToString(),FString(TEXT("예상 환전금 600원")));
    struct FCardInput {FBetTicket Ticket;} CardInput{Pending};
    for(const auto& Sample:TArray<FVector3f>{{23761.f,7.f,2.05f},{1337.f,3.f,1.1f},{16777217.f,1.f,1.03f},{321.f,8.f,2.75f}})
    {
        CardInput.Ticket.Amount=static_cast<int32>(Sample.X);CardInput.Ticket.Count=static_cast<int32>(Sample.Y);CardInput.Ticket.Odds=Sample.Z;
        PendingCard->ProcessEvent(PendingCard->FindFunctionChecked(TEXT("SetTicketInfo")),&CardInput);
        TestEqual(TEXT("Blueprint payout rounding matches the server"),CastChecked<UTextBlock>(PendingCard->GetWidgetFromName(TEXT("ExpectedPayout")))->GetText().ToString(),
            FText::Format(FText::FromString(TEXT("예상 환전금 {0}원")),FText::AsNumber(CardInput.Ticket.Payout())).ToString());
    }
    CardInput.Ticket=Pending;PendingCard->ProcessEvent(PendingCard->FindFunctionChecked(TEXT("SetTicketInfo")),&CardInput);
    TestEqual(TEXT("Summary counts quantities and total cost"),CastChecked<UTextBlock>(Page->GetWidgetFromName(TEXT("TicketSummary")))->GetText().ToString(),FString(TEXT("보유 5장 · 총 구매액 500원")));
    Render(TEXT("tickets.png"));
    Render(TEXT("tickets-720p.png"),FVector2D(1280.f,720.f));

    TestEqual(TEXT("Winning ticket redeemed"), Manager->ServerClaimWinnings(Player), 400);
    TestEqual(TEXT("Open page updates after redemption"), Displayed().Num(), 1);
    TestEqual(TEXT("Other player's ticket remains"), Manager->GetTicketsForPlayer(OtherBuyer).Num(), 1);
    Manager->WinnerIndex = 0;
    Manager->SettleTickets();
    TestEqual(TEXT("Losing ticket disappears when results settle"), Displayed().Num(), 0);
    TestTrue(TEXT("Empty-state message is visible"), Empty->GetVisibility() == ESlateVisibility::Visible);
    Render(TEXT("empty.png"));

    // A replicated ledger snapshot refreshes the same open page.
    Pending.TicketId = 4;
    Manager->Tickets.Add(Pending);
    Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("OnRep_Tickets")), nullptr);
    TestEqual(TEXT("Replicated ticket addition refreshes open page"), Displayed().Num(), 1);
    auto* Back = CastChecked<UButton>(Page->GetWidgetFromName(TEXT("GoBack_Button")));
    Back->OnClicked.Broadcast();
    TestEqual(TEXT("Back button returns to NPC menu"), Switcher->GetActiveWidgetIndex(), 0);
    Pending.TicketId = 5;
    Manager->Tickets.Add(Pending);
    Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("OnRep_Tickets")), nullptr);
    TestEqual(TEXT("Closed page unsubscribes from ledger updates"), Displayed().Num(), 1);
    Button->OnClicked.Broadcast();
    TestEqual(TEXT("Reopening refreshes all current tickets without duplicates"), Displayed().Num(), 2);
    Back->OnClicked.Broadcast();

    // Open the payout page through the same NPC menu and redeem through its BP button.
    Manager->Tickets.Add(Winner);
    auto* PayoutPage=CastChecked<UUserWidget>(NPC->GetWidgetFromName(TEXT("WBP_RacePayoutWidget")));
    CastChecked<UButton>(NPC->GetWidgetFromName(TEXT("PayoutButton")))->OnClicked.Broadcast();
    TestTrue(TEXT("NPC opens payout page"),Switcher->GetActiveWidget()==PayoutPage);
    auto* PayoutScroll=CastChecked<UScrollBox>(PayoutPage->GetWidgetFromName(TEXT("ScrollBox_0")));
    TestEqual(TEXT("Payout lists only this player's winners"),PayoutScroll->GetChildrenCount(),1);
    TestEqual(TEXT("Payout summary uses fixed ticket odds and quantity"),CastChecked<UTextBlock>(PayoutPage->GetWidgetFromName(TEXT("TicketSummary")))->GetText().ToString(),FString(TEXT("당첨 2장 · 총 환전금 400원")));
    Render(TEXT("payout.png"));
    auto* Claim=CastChecked<UButton>(PayoutPage->GetWidgetFromName(TEXT("PayOut_Button")));
    const float CurrencyBefore=ASC->GetNumericAttribute(Ucasino_simulatorAttributeSet::GetCurrencyAttribute());
    Claim->OnClicked.Broadcast();
    TestEqual(TEXT("BP payout button pays winnings"),ASC->GetNumericAttribute(Ucasino_simulatorAttributeSet::GetCurrencyAttribute()),CurrencyBefore+400.f);
    TestEqual(TEXT("Payout refreshes after claim"),PayoutScroll->GetChildrenCount(),0);
    TestFalse(TEXT("Empty payout disables claim"),Claim->GetIsEnabled());
    CastChecked<UButton>(PayoutPage->GetWidgetFromName(TEXT("GoBack_Button")))->OnClicked.Broadcast();
    TestEqual(TEXT("Payout dispatcher returns to menu"),Switcher->GetActiveWidgetIndex(),0);

    // Preserve participant selection, number-pad input, and purchase RPC wiring.
    Manager->RunnerClass=LoadClass<ARaceRunner>(nullptr,TEXT("/Game/KCU/HorseRacing/BP_RunnerBase.BP_RunnerBase_C"));
    for(int32 I=0;I<4;++I)Manager->RunnerSpawnPoints.Add(World->SpawnActor<ATargetPoint>());
    Manager->StartNewRound();Manager->Phase=ERacePhase::Betting;
    auto* PurchasePage=CastChecked<UUserWidget>(NPC->GetWidgetFromName(TEXT("WBP_RaceBettingWidget")));
    CastChecked<UButton>(NPC->GetWidgetFromName(TEXT("PerchaseTicketButton")))->OnClicked.Broadcast();
    TestTrue(TEXT("NPC opens purchase page"),Switcher->GetActiveWidget()==PurchasePage);
    auto* RunnerScroll=CastChecked<UScrollBox>(PurchasePage->GetWidgetFromName(TEXT("ScrollBox_0")));
    if(!TestEqual(TEXT("Purchase still creates runner cards"),RunnerScroll->GetChildrenCount(),4))return false;
    auto* RunnerCard=CastChecked<UUserWidget>(RunnerScroll->GetChildAt(0));
    CastChecked<UButton>(RunnerCard->GetWidgetFromName(TEXT("Button_24")))->OnClicked.Broadcast();
    TestEqual(TEXT("Runner click selects its index"),FindFProperty<FIntProperty>(PurchasePage->GetClass(),TEXT("SelectedRunnerIndex "))->GetPropertyValue_InContainer(PurchasePage),0);
    auto* NumberPad=CastChecked<UUserWidget>(FindFProperty<FObjectPropertyBase>(PurchasePage->GetClass(),TEXT("Calculate"))->GetObjectPropertyValue_InContainer(PurchasePage));
    TestEqual(TEXT("Existing shared number pad is preserved"),NumberPad->GetClass()->GetPathName(),FString(TEXT("/Game/1_BluePrint/Widget/WBP_Calculate.WBP_Calculate_C")));
    CastChecked<UButton>(NumberPad->GetWidgetFromName(TEXT("Button_1")))->OnClicked.Broadcast();
    CastChecked<UButton>(NumberPad->GetWidgetFromName(TEXT("Button_0")))->OnClicked.Broadcast();
    CastChecked<UButton>(NumberPad->GetWidgetFromName(TEXT("Button_0")))->OnClicked.Broadcast();
    TestEqual(TEXT("Number pad builds entered amount"),FindFProperty<FIntProperty>(PurchasePage->GetClass(),TEXT("InputValue"))->GetPropertyValue_InContainer(PurchasePage),100);
#if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("RenderOwnedTickets")))
    {TArray<UTexture*> Portraits;for(auto* Runner:Manager->GetRunners())if(Runner->GetRunnerPortrait())Portraits.Add(Runner->GetRunnerPortrait());FTextureCompilingManager::Get().FinishCompilation(Portraits);}
#endif
    Render(TEXT("purchase.png"));
    const int32 BeforePurchase=Manager->Tickets.Num();
    CastChecked<UButton>(PurchasePage->GetWidgetFromName(TEXT("Perchase_Button")))->OnClicked.Broadcast();
    TestEqual(TEXT("Purchase BP still buys a ticket"),Manager->Tickets.Num(),BeforePurchase+1);
    CastChecked<UButton>(PurchasePage->GetWidgetFromName(TEXT("GoBack_Button")))->OnClicked.Broadcast();
    TestEqual(TEXT("Purchase dispatcher returns to menu"),Switcher->GetActiveWidgetIndex(),0);
    return true;
}
#endif
