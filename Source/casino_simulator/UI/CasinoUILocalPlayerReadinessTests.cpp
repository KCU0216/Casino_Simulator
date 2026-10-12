#if WITH_DEV_AUTOMATION_TESTS
#include "UI/CasinoUIManagerComponent.h"
#include "UI/CasinoManagedWidget.h"
#include "UI/casino_simulatorPlayerHUD.h"
#include "casino_simulatorPlayerController.h"
#include "casino_simulatorPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoUILocalPlayerReadinessTest,
    "Casino.UI.LocalPlayerReadiness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoUILocalPlayerReadinessTest::RunTest(const FString& Parameters)
{
    UClass* ControllerClass = LoadClass<Acasino_simulatorPlayerController>(nullptr,
        TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
    UClass* IntroClass = LoadClass<UCasinoManagedWidget>(nullptr,
        TEXT("/Game/Blackjack/WBP/WBP_DayIntro.WBP_DayIntro_C"));
    if (!TestNotNull(TEXT("Shipped player controller class"), ControllerClass)
        || !TestNotNull(TEXT("Shipped day-intro class"), IntroClass)) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    auto* State = World->SpawnActor<ACasinoLoopGameState>();
    auto* Controller = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass);
    auto* PlayerState = World->SpawnActor<Acasino_simulatorPlayerState>();
    if (!TestNotNull(TEXT("Loop state"), State) || !TestNotNull(TEXT("Controller"), Controller)
        || !TestNotNull(TEXT("Player state"), PlayerState)) return false;
    World->SetGameState(State);
    State->LoopStatus.Phase = ECasinoLoopPhase::DayIntro;
    State->LoopStatus.CurrentDay = 1;
    State->LoopStatus.RequiredPayment = 3000;
    Controller->PlayerState = PlayerState;
    PlayerState->SetOwner(Controller);
    Controller->ScreenWidgetClasses.Reset();
    Controller->ScreenWidgetClasses.Add(ECasinoUIScreen::DayIntro, IntroClass);
    if (!TestNotNull(TEXT("Configured HUD class"), Controller->PlayerHUDWidgetClass.Get())) return false;
    auto* Manager = Controller->GetUIManager();
    if (!TestNotNull(TEXT("Controller UI manager"), Manager)) return false;

    // IsLocalController caches true during controller initialization/travel, even
    // though the engine has not yet attached the LocalPlayer required by UMG.
    Controller->Player = nullptr;
    Controller->SetAsLocalPlayerController();
    TestTrue(TEXT("Cached local classification reproduces the reported race"), Controller->IsLocalController());
    TestFalse(TEXT("No Player means presentation is not ready"), Manager->IsLocalUIReady());
    for (int32 Retry = 0; Retry < 3; ++Retry)
    {
        Manager->EnsureUIRoot();
        Manager->SetCasinoUIScreen(ECasinoUIScreen::DayIntro);
        Manager->RefreshCasinoUIScreen();
        Manager->UpdateUI();
        Controller->TryInitializePlayerHUD();
    }
    TestNull(TEXT("Early retries allocate no UI root"), Manager->UIRoot.Get());
    TestNull(TEXT("Early retries allocate no day-intro widget"), Manager->ActiveScreenWidget.Get());
    TestNull(TEXT("Early retries allocate no HUD despite a valid PlayerState"), Controller->PlayerHUDWidget.Get());
    TestEqual(TEXT("A deferred screen is not marked initialized"), Manager->UIScreen, ECasinoUIScreen::Loading);
    Manager->UIScreen = ECasinoUIScreen::Playing;
    TestNull(TEXT("Interaction factory also waits for LocalPlayer"), Manager->OpenInteractionUI(UCasinoUIRoot::StaticClass()));
    Manager->ShowWorldEventAnnouncement();
    TestNull(TEXT("Announcement cannot indirectly create an early HUD"), Controller->PlayerHUDWidget.Get());
    Manager->UIScreen = ECasinoUIScreen::Loading;

    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    Controller->Player = LocalPlayer;
    World->AddController(Controller);
    TestFalse(TEXT("LocalPlayer without its reverse controller link is still incomplete"), Manager->IsLocalUIReady());
    Manager->UpdateUI();
    Controller->TryInitializePlayerHUD();
    TestNull(TEXT("Incomplete attachment does not consume the screen retry"), Manager->ActiveScreenWidget.Get());
    TestNull(TEXT("Incomplete attachment does not create a HUD"), Controller->PlayerHUDWidget.Get());

    LocalPlayer->PlayerController = Controller;
    TestTrue(TEXT("Complete local ownership enables presentation"), Manager->IsLocalUIReady());
    Manager->UpdateUI();
    Controller->TryInitializePlayerHUD();
    TestNotNull(TEXT("First ready update creates the root without a delay"), Manager->UIRoot.Get());
    TestEqual(TEXT("First ready update derives the replicated phase"), Manager->UIScreen, ECasinoUIScreen::DayIntro);
    auto* Intro = Cast<UCasinoManagedWidget>(Manager->ActiveScreenWidget);
    if (!TestNotNull(TEXT("First ready update creates the actual day-intro screen"), Intro)) return false;
    TestTrue(TEXT("Ready screen receives the replicated snapshot"), Intro->bHasLoopStatus);
    TestEqual(TEXT("Ready screen has the first-day payment target"), Intro->LoopStatus.RequiredPayment, 3000);
    if (!TestNotNull(TEXT("HUD initialization resumes once local ownership is ready"), Controller->PlayerHUDWidget.Get())) return false;
    UCasinoUIRoot* FirstRoot = Manager->UIRoot;
    Ucasino_simulatorPlayerHUD* FirstHUD = Controller->PlayerHUDWidget;
    for (int32 Retry = 0; Retry < 3; ++Retry)
    {
        Manager->UpdateUI();
        Controller->TryInitializePlayerHUD();
    }
    TestTrue(TEXT("Repeated ready updates reuse the root"), Manager->UIRoot == FirstRoot);
    TestTrue(TEXT("Repeated ready updates do not reopen the modal"), Manager->ActiveScreenWidget == Intro);
    TestTrue(TEXT("Repeated ready updates do not duplicate the HUD"), Controller->PlayerHUDWidget == FirstHUD);

    // A phase update can arrive while a controller is temporarily detached. The
    // next ready update must observe it even though GameState itself is unchanged.
    Controller->Player = nullptr;
    State->LoopStatus.Phase = ECasinoLoopPhase::Playing;
    State->OnLoopChanged.Broadcast();
    Manager->UpdateUI();
    Controller->TryInitializePlayerHUD();
    TestTrue(TEXT("Detached update does not allocate a replacement root"), Manager->UIRoot == FirstRoot);
    TestTrue(TEXT("Detached update does not allocate a replacement HUD"), Controller->PlayerHUDWidget == FirstHUD);
    Controller->Player = LocalPlayer;
    Manager->UpdateUI();
    TestEqual(TEXT("Recovered ownership refreshes a phase changed on the same GameState"), Manager->UIScreen, ECasinoUIScreen::Playing);
    TestNull(TEXT("Recovered playing phase removes the old intro"), Manager->ActiveScreenWidget.Get());
    TestTrue(TEXT("Recovered phase preserves the balance HUD"), Controller->PlayerHUDWidget == FirstHUD);
    UUserWidget* Interaction = Manager->OpenInteractionUI(UCasinoUIRoot::StaticClass());
    if (!TestNotNull(TEXT("Recovered gameplay can open an interaction widget"), Interaction)) return false;
    TestTrue(TEXT("Recovered interaction has the correct owning controller"), Interaction->GetOwningPlayer() == Controller);
    Manager->CloseInteractionUI(Interaction);
    State->LoopStatus.Phase = ECasinoLoopPhase::DayIntro;
    State->OnLoopChanged.Broadcast();

    Manager->ShutdownUI();
    Controller->Player = nullptr;
    TestTrue(TEXT("Detaching Player leaves the engine's cached local flag true"), Controller->IsLocalController());
    TestFalse(TEXT("Detached controller cannot recreate presentation"), Manager->IsLocalUIReady());
    Manager->UpdateUI();
    Controller->TryInitializePlayerHUD();
    TestNull(TEXT("Detached retry creates no modal"), Manager->ActiveScreenWidget.Get());
    TestNull(TEXT("Detached retry creates no HUD"), Controller->PlayerHUDWidget.Get());
    Controller->Player = LocalPlayer;
    Manager->UpdateUI();
    Controller->TryInitializePlayerHUD();
    TestNotNull(TEXT("Reattaching the same local controller resumes the screen"), Manager->ActiveScreenWidget.Get());
    TestNotNull(TEXT("Reattaching the same local controller resumes the HUD"), Controller->PlayerHUDWidget.Get());
    Manager->ShutdownUI();

    auto* Replacement = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass);
    if (!TestNotNull(TEXT("Replacement controller"), Replacement)) return false;
    Replacement->ScreenWidgetClasses.Reset();
    Replacement->ScreenWidgetClasses.Add(ECasinoUIScreen::DayIntro, IntroClass);
    Replacement->PlayerState = World->SpawnActor<Acasino_simulatorPlayerState>();
    Replacement->Player = LocalPlayer;
    LocalPlayer->PlayerController = Replacement;
    Replacement->SetAsLocalPlayerController();
    World->AddController(Replacement);
    TestFalse(TEXT("Old controller cannot borrow a LocalPlayer reassigned during travel"), Manager->IsLocalUIReady());
    Manager->EnsureUIRoot();
    Manager->SetCasinoUIScreen(ECasinoUIScreen::DayIntro);
    Manager->UpdateUI();
    Controller->TryInitializePlayerHUD();
    TestNull(TEXT("Old controller has no root after ownership transfer"), Manager->UIRoot.Get());
    TestNull(TEXT("Old controller has no HUD after ownership transfer"), Controller->PlayerHUDWidget.Get());
    // SetPlayer normally detaches the previous same-world controller. Complete
    // that handoff after testing the deliberately stale reverse-link interval;
    // UMG resolves the owner by the world's first controller with this Player.
    Controller->Player = nullptr;
    Replacement->GetUIManager()->UpdateUI();
    Replacement->TryInitializePlayerHUD();
    TestTrue(TEXT("New controller owns the ready local presentation"), Replacement->GetUIManager()->IsLocalUIReady());
    TestNotNull(TEXT("New controller opens the phase screen"), Replacement->GetUIManager()->ActiveScreenWidget.Get());
    TestNotNull(TEXT("New controller creates its HUD"), Replacement->PlayerHUDWidget.Get());

    auto* Remote = World->SpawnActor<Acasino_simulatorPlayerController>(ControllerClass);
    if (!TestNotNull(TEXT("Server-side remote controller"), Remote)) return false;
    auto* RemoteConnection = NewObject<USimulatedClientNetConnection>(GEngine);
    Remote->Player = RemoteConnection;
    RemoteConnection->PlayerController = Remote;
    Remote->PlayerState = World->SpawnActor<Acasino_simulatorPlayerState>();
    Remote->ScreenWidgetClasses.Add(ECasinoUIScreen::DayIntro, IntroClass);
    TestFalse(TEXT("A server-side network owner is not a local controller"), Remote->IsLocalController());
    Remote->SetAsLocalPlayerController();
    TestFalse(TEXT("A server's network connection is not a LocalPlayer"), Remote->GetUIManager()->IsLocalUIReady());
    Remote->GetUIManager()->EnsureUIRoot();
    Remote->GetUIManager()->SetCasinoUIScreen(ECasinoUIScreen::DayIntro);
    Remote->GetUIManager()->UpdateUI();
    Remote->TryInitializePlayerHUD();
    TestNull(TEXT("Server-side remote controller creates no root"), Remote->UIRoot.Get());
    TestNull(TEXT("Server-side remote controller creates no modal"), Remote->ActiveScreenWidget.Get());
    TestNull(TEXT("Server-side remote controller creates no HUD"), Remote->PlayerHUDWidget.Get());
    Replacement->GetUIManager()->ShutdownUI();
    Remote->Player = nullptr;
    RemoteConnection->PlayerController = nullptr;
    return true;
}
#endif
