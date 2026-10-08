#if WITH_DEV_AUTOMATION_TESTS

#include "RaceManager.h"
#include "RaceRunner.h"
#include "RaceBillboardTestWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/TargetPoint.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceBillboardFlowTest, "Casino.Race.BillboardFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaceBillboardFlowTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->TimeSeconds = 10.f;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    auto* Manager = World->SpawnActor<ARaceManager>();
    auto* Widget = CreateWidget<URaceBillboardTestWidget>(World);
    if (!TestNotNull(TEXT("Manager"), Manager) || !TestNotNull(TEXT("Widget"), Widget)) return false;
    TArray<UChildActorComponent*> ManagerChildActors;
    Manager->GetComponents(ManagerChildActors);
    TestEqual(TEXT("Manager no longer creates camera child actors"), ManagerChildActors.Num(), 0);
    Manager->BillboardWidget->SetWidget(Widget);
    Manager->OnRep_Phase();
    TestTrue(TEXT("Widget receives manager before its event"), Widget->RaceManager == Manager);
    TestEqual(TEXT("Initial phase delivered once"), Widget->PhaseCount, 1);
    Manager->RefreshBillboard();
    Manager->OnRep_Phase();
    TestEqual(TEXT("Repeated refresh does not restart BP phase animation"), Widget->PhaseCount, 1);

    const ERacePhase Phases[] = { ERacePhase::Entering, ERacePhase::Betting,
        ERacePhase::Racing, ERacePhase::Exiting, ERacePhase::Finished, ERacePhase::Idle };
    for (ERacePhase Phase : Phases)
    {
        Manager->Phase = Phase;
        Manager->OnRep_Phase(); // Same callback used when clients receive replicated Phase.
        TestTrue(TEXT("Widget receives each current phase"), Widget->CurrentPhase == Phase);
    }
    TestEqual(TEXT("All six transitions delivered"), Widget->PhaseCount, 7);
    const int32 PreviousDataCount = Widget->DataCount;
    Manager->OnRep_BillboardData();
    TestEqual(TEXT("Late replicated data refreshes the widget"), Widget->DataCount, PreviousDataCount + 1);
    TestEqual(TEXT("Data refresh does not replay phase animation"), Widget->PhaseCount, 7);

    auto* Replacement = CreateWidget<URaceBillboardTestWidget>(World);
    Manager->Phase = ERacePhase::Betting;
    Manager->BillboardWidget->SetWidget(Replacement);
    Manager->RefreshBillboard();
    TestEqual(TEXT("Late widget receives current phase"), Replacement->PhaseCount, 1);
    TestTrue(TEXT("Late widget starts in betting"), Replacement->CurrentPhase == ERacePhase::Betting);
    Manager->BettingDuration = 30.f;
    Manager->BettingStartServerTime = World->GetTimeSeconds() - 7.0;
    TestEqual(TEXT("Countdown uses elapsed server time"), Manager->GetRemainingBettingSeconds(), 23.f);
    Manager->BettingStartServerTime = World->GetTimeSeconds() - 40.0;
    TestEqual(TEXT("Countdown clamps at zero"), Manager->GetRemainingBettingSeconds(), 0.f);

    auto* First = World->SpawnActor<ARaceRunner>();
    auto* Second = World->SpawnActor<ARaceRunner>();
    Manager->Runners = { First, Second };
    TArray<UChildActorComponent*> RunnerChildActors;
    First->GetComponents(RunnerChildActors);
    TestEqual(TEXT("Runner no longer creates a camera child actor"), RunnerChildActors.Num(), 0);
    auto SetupRunner = [World](ARaceRunner* Runner, float Speed)
    {
        FRunnerRaceScript Script;
        Script.Speed = Speed;
        Script.TrackLength = 1000.f;
        Script.RaceStartServerTime = World->GetTimeSeconds() - 1.0;
        Runner->ServerSetupScript(Script);
        Runner->ServerSetRacing(true, Script.RaceStartServerTime);
        Runner->Tick(0.f);
    };
    SetupRunner(First, 100.f);
    SetupRunner(Second, 200.f);
    Manager->WinnerIndex = 0;
    Manager->Phase = ERacePhase::Racing;
    TestTrue(TEXT("Leader display uses current progress, not predetermined winner"), Manager->GetLeadingRunner() == Second);
    Manager->Phase = ERacePhase::Exiting;
    TestNull(TEXT("Live leader tracking stops outside racing"), Manager->GetLeadingRunner());
    TestTrue(TEXT("Final winner remains separately available"), Manager->GetWinner() == First);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceSixLaneBillboardTest, "Casino.Race.SixLaneBillboard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaceSixLaneBillboardTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    auto* Manager = World->SpawnActor<ARaceManager>();
    Manager->RunnerClass = LoadClass<ARaceRunner>(nullptr,
        TEXT("/Game/KCU/HorseRacing/BP_RunnerBase.BP_RunnerBase_C"));
    for (int32 Index = 0; Index < 6; ++Index)
        Manager->RunnerSpawnPoints.Add(World->SpawnActor<ATargetPoint>());
    Manager->StartNewRound();
    if (!TestEqual(TEXT("Six lanes spawn six runners"), Manager->GetRunners().Num(), 6)) return false;
    Manager->Phase = ERacePhase::Betting;
    UClass* Class = LoadClass<URaceBillboardWidget>(nullptr,
        TEXT("/Game/KCU/HorseRacing/WBP_RaceBillboard.WBP_RaceBillboard_C"));
    if (!TestNotNull(TEXT("Billboard class"), Class)) return false;
    auto* Widget = CreateWidget<URaceBillboardWidget>(World, Class);
    Widget->RefreshFromManager(Manager);
    const auto Root = Widget->TakeWidget();
    for (int32 Lane = 5; Lane <= 6; ++Lane)
    {
        auto* Profile = Cast<UTextBlock>(Widget->GetWidgetFromName(
            FName(*FString::Printf(TEXT("RaceLane%dProfile"), Lane))));
        auto* Portrait = Cast<UImage>(Widget->GetWidgetFromName(
            FName(*FString::Printf(TEXT("RaceLane%dPortrait"), Lane))));
        if (!TestNotNull(TEXT("Additional profile"), Profile)
            || !TestNotNull(TEXT("Additional portrait"), Portrait)) return false;
        ARaceRunner* Runner = Manager->GetRunners()[Lane - 1];
        TestTrue(TEXT("Additional profile displays its own runner"),
            Profile->GetText().ToString().Contains(Runner->Stats.Name));
        TestTrue(TEXT("Additional portrait displays its own runner"),
            Portrait->GetBrush().GetResourceObject() == Runner->GetRunnerPortrait());
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("RenderRaceLanes")))
    {
#if WITH_EDITOR
        TArray<UTexture*> Portraits;
        for (ARaceRunner* Runner : Manager->GetRunners())
            if (Runner->GetRunnerPortrait()) Portraits.Add(Runner->GetRunnerPortrait());
        FTextureCompilingManager::Get().FinishCompilation(Portraits);
#endif
        const FVector2D Size(1920.f, 1080.f);
        FWidgetRenderer Renderer(false, true);
        UTextureRenderTarget2D* Target = Renderer.DrawWidget(Root, Size);
        if (Target) Renderer.DrawWidget(Target, Root, Size, 0.f);
        for (int32 Lane = 5; Lane <= 6; ++Lane)
        {
            UWidget* Row = Widget->GetWidgetFromName(
                FName(*FString::Printf(TEXT("RaceLane%dRow"), Lane)));
            const auto& Geometry = Row->GetCachedGeometry();
            TestTrue(TEXT("Additional row fits the billboard"), Geometry.GetAbsolutePosition().Y >= 0.f
                && Geometry.GetAbsolutePosition().Y + Geometry.GetAbsoluteSize().Y <= Size.Y + 1.f);
        }
        FBufferArchive PNG;
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("RaceLanePreview");
        IFileManager::Get().MakeDirectory(*Directory, true);
        TestTrue(TEXT("Billboard preview renders"), Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG)
            && FFileHelper::SaveArrayToFile(PNG, *(Directory / TEXT("billboard.png"))));
    }
    auto* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    auto* Player = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass);
    auto* Buyer = World->SpawnActor<APlayerState>();
    FindFProperty<FObjectPropertyBase>(APawn::StaticClass(), TEXT("PlayerState"))
        ->SetObjectPropertyValue_InContainer(Player, Buyer);
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 1000.f);
    UClass* NPCClass = LoadClass<UUserWidget>(nullptr,
        TEXT("/Game/KCU/HorseRacing/WBP_RaceNPCWidget.WBP_RaceNPCWidget_C"));
    auto* NPC = CreateWidget<UUserWidget>(World, NPCClass);
    FindFProperty<FObjectPropertyBase>(NPCClass, TEXT("RaceManager"))->SetObjectPropertyValue_InContainer(NPC, Manager);
    FindFProperty<FObjectPropertyBase>(NPCClass, TEXT("Player"))->SetObjectPropertyValue_InContainer(NPC, Player);
    const auto NPCRoot = NPC->TakeWidget();
    CastChecked<UButton>(NPC->GetWidgetFromName(TEXT("PerchaseTicketButton")))->OnClicked.Broadcast();
    auto* Purchase = CastChecked<UUserWidget>(NPC->GetWidgetFromName(TEXT("WBP_RaceBettingWidget")));
    auto* Cards = CastChecked<UScrollBox>(Purchase->GetWidgetFromName(TEXT("ScrollBox_0")));
    if (!TestEqual(TEXT("Purchase lists all six runners"), Cards->GetChildrenCount(), 6)) return false;
    auto* SixthCard = CastChecked<UUserWidget>(Cards->GetChildAt(5));
    CastChecked<UButton>(SixthCard->GetWidgetFromName(TEXT("Button_24")))->OnClicked.Broadcast();
    TestEqual(TEXT("Sixth card selects runner index five"),
        FindFProperty<FIntProperty>(Purchase->GetClass(), TEXT("SelectedRunnerIndex "))
            ->GetPropertyValue_InContainer(Purchase), 5);
    auto* NumberPad = CastChecked<UUserWidget>(FindFProperty<FObjectPropertyBase>(Purchase->GetClass(), TEXT("Calculate"))
        ->GetObjectPropertyValue_InContainer(Purchase));
    for (const TCHAR* Button : { TEXT("Button_1"), TEXT("Button_0"), TEXT("Button_0") })
        CastChecked<UButton>(NumberPad->GetWidgetFromName(Button))->OnClicked.Broadcast();
    const int32 TicketsBefore = Manager->Tickets.Num();
    CastChecked<UButton>(Purchase->GetWidgetFromName(TEXT("Perchase_Button")))->OnClicked.Broadcast();
    if (TestEqual(TEXT("Purchase button buys the sixth runner"), Manager->Tickets.Num(), TicketsBefore + 1))
        TestEqual(TEXT("Ticket records runner index five"), Manager->Tickets.Last().RunnerIndex, 5);

    ARaceRunner* Sixth = Manager->GetRunners()[5];
    Sixth->Stats.Name = TEXT("Updated sixth runner");
    Sixth->RunnerPortrait = nullptr;
    Widget->RefreshFromManager(Manager);
    TestTrue(TEXT("Late stats update without a phase change"),
        CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("RaceLane6Profile")))
            ->GetText().ToString().Contains(Sixth->Stats.Name));
    TestNull(TEXT("Late empty portrait clears the previous image"),
        CastChecked<UImage>(Widget->GetWidgetFromName(TEXT("RaceLane6Portrait")))
            ->GetBrush().GetResourceObject());
    Manager->Phase = ERacePhase::Finished;
    Manager->ResetRace();
    Widget->RefreshFromManager(Manager);
    TestTrue(TEXT("Absent runner row collapses"),
        Widget->GetWidgetFromName(TEXT("RaceLane6Row"))->GetVisibility() == ESlateVisibility::Collapsed);
    return true;
}

#endif
