#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Online/CasinoOnlineSettings.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/CasinoRoomDialogWidget.h"
#include "UI/CasinoMainMenuWidget.h"
#include "UI/CasinoUIManagerComponent.h"
#include "UI/CasinoUIRoot.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"
#include "Widgets/SWidget.h"
#include "casino_simulatorPlayerController.h"

namespace
{
bool HasPaintedArea(UWidget* Widget)
{
    // Inspect the content root rather than the UserWidget's SObjectWidget wrapper.
    if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
        Widget = UserWidget->WidgetTree ? UserWidget->WidgetTree->RootWidget.Get() : nullptr;
    const TSharedPtr<SWidget> SlateWidget = Widget ? Widget->GetCachedWidget() : nullptr;
    if (!SlateWidget || !SlateWidget->GetVisibility().IsVisible()) return false;
    const FVector2D Size = Widget->GetPaintSpaceGeometry().GetLocalSize();
    return Size.X > 1.0 && Size.Y > 1.0;
}

FString GeometryDescription(UWidget* Widget)
{
    if (!Widget) return TEXT("null");
    const FVector2D CachedSize = Widget->GetCachedGeometry().GetLocalSize();
    const FVector2D PaintSize = Widget->GetPaintSpaceGeometry().GetLocalSize();
    return FString::Printf(TEXT("%s visibility=%d cached=%s paint=%s slate=%d"), *Widget->GetName(),
        int32(Widget->GetVisibility()), *CachedSize.ToString(), *PaintSize.ToString(), Widget->GetCachedWidget().IsValid());
}

/** Runs against a real PIE viewport; no Submit call or room network request is made. */
class FCasinoRoomDialogPresentationCommand final : public IAutomationLatentCommand
{
public:
    explicit FCasinoRoomDialogPresentationCommand(FAutomationTestBase* InTest)
        : Test(InTest), StageStarted(FPlatformTime::Seconds()) {}

    virtual bool Update() override
    {
        if (!bStarted)
        {
            bStarted = true;
            StageStarted = FPlatformTime::Seconds();
        }
        if (FPlatformTime::Seconds() - StageStarted > 30.0)
        {
            ReportWorlds();
            Test->AddError(FString::Printf(TEXT("Room dialog PIE stage %d timed out."), Stage));
            if (Dialog.IsValid()) Dialog->Cancel();
            return true;
        }

        if (Stage == 0) return WaitForMainMenu();
        if (GFrameCounter < StageFrame + 6) return false;
        if (!Controller.IsValid() || !Menu.IsValid())
        {
            Test->AddError(TEXT("The local controller or original main menu disappeared during the dialog test."));
            if (Dialog.IsValid()) Dialog->Cancel();
            return true;
        }

        switch (Stage)
        {
        case 1:
            VerifyVisibleDialog(TEXT("Create"));
            VerifyOriginalMenu();
            if (!Dialog.IsValid()) return true;
            ScreenshotPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
                TEXT("CasinoDealerSetup"), FString::Printf(TEXT("RoomDialog_PIE_%s.png"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
            Advance(2);
            return false;
        case 2:
            if (FScreenshotRequest::IsScreenshotRequested()) return false;
            Test->TestTrue(TEXT("PIE screenshot including UI was saved"), IFileManager::Get().FileExists(*ScreenshotPath));
            Test->AddInfo(FString::Printf(TEXT("Live PIE room dialog screenshot: %s"), *ScreenshotPath));
            if (!Dialog.IsValid()) return true;
            if (!Test->TestNotNull(TEXT("Create popup retains its password input"), Dialog->GetPasswordInput()))
            {
                Dialog->Cancel();
                return true;
            }
            Dialog->GetPasswordInput()->SetText(FText::FromString(TEXT("local-test-password")));
            ClosedDialog.Reset(Dialog.Get());
            Dialog->Cancel();
            Test->TestNull(TEXT("Cancel detaches the dialog from its modal parent"), Dialog->GetParent());
            Test->TestFalse(TEXT("Cancel removes any direct viewport registration"), Dialog->IsInViewport());
            Test->TestTrue(TEXT("Cancel clears the password"), Dialog->GetPasswordInput()->GetText().IsEmpty());
            Advance(3);
            return false;
        case 3:
            VerifyOriginalMenu();
            Test->TestTrue(TEXT("The original menu is still painted after Cancel"), HasPaintedArea(Menu.Get()));
            Dialog = OpenDialog(false);
            Test->TestNotNull(TEXT("Create dialog can reopen after Cancel"), Dialog.Get());
            Test->TestTrue(TEXT("Reopen creates a new dialog instead of reusing the detached instance"), Dialog.Get() != ClosedDialog.Get());
            if (!Dialog.IsValid()) return true;
            Advance(4);
            return false;
        case 4:
            VerifyVisibleDialog(TEXT("Reopened create"));
            VerifyOriginalMenu();
            if (!Dialog.IsValid()) return true;
            Dialog->Cancel();
            Dialog = OpenDialog(true);
            Test->TestNotNull(TEXT("Protected-room join dialog opens without submitting a request"), Dialog.Get());
            if (!Dialog.IsValid()) return true;
            Advance(5);
            return false;
        case 5:
            VerifyVisibleDialog(TEXT("Protected join"));
            VerifyOriginalMenu();
            if (!Dialog.IsValid()) return true;
            Test->TestEqual(TEXT("Protected room has join mode"), Dialog->GetDialogMode(), ECasinoRoomDialogMode::Join);
            Test->TestFalse(TEXT("Opening the join popup does not submit"), Dialog->IsSubmitting());
            if (Test->TestNotNull(TEXT("Protected join retains its password input"), Dialog->GetPasswordInput()))
                Test->TestTrue(TEXT("Protected join password input is masked"), Dialog->GetPasswordInput()->GetIsPassword());
            Dialog->Cancel();
            Advance(6);
            return false;
        case 6:
            VerifyOriginalMenu();
            Test->TestTrue(TEXT("Original menu is still painted after protected join Cancel"), HasPaintedArea(Menu.Get()));
            return true;
        default:
            return true;
        }
    }

private:
    bool WaitForMainMenu()
    {
        if (!GEngine) return false;
        if (FPlatformTime::Seconds() - LastDiagnostic > 5.0)
        {
            ReportWorlds();
            LastDiagnostic = FPlatformTime::Seconds();
        }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || Context.WorldType != EWorldType::PIE) continue;
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                Acasino_simulatorPlayerController* Candidate = Cast<Acasino_simulatorPlayerController>(It->Get());
                if (!Candidate || !Candidate->IsLocalController() || !Candidate->GetLocalPlayer()) continue;
                UCasinoUIManagerComponent* Manager = Candidate->GetUIManager();
                if (!Manager || Manager->UIScreen != ECasinoUIScreen::MainMenu) continue;
                // L_MainMenu's legacy Level Blueprint adds its menu directly to
                // the global viewport, so UIManager's active-screen pointer can
                // legitimately be null. Find the real already displayed menu.
                for (TObjectIterator<UCasinoMainMenuWidget> MenuIt; MenuIt; ++MenuIt)
                {
                    UUserWidget* ActiveMenu = *MenuIt;
                    if (ActiveMenu->GetWorld() != World || ActiveMenu->GetOwningPlayer() != Candidate
                        || (!ActiveMenu->GetParent() && !ActiveMenu->IsInViewport())
                        || !ActiveMenu->GetCachedWidget().IsValid()) continue;
                    Controller = Candidate;
                    Menu = ActiveMenu;
                    MenuParent = ActiveMenu->GetParent();
                    OriginalManagedMenu = Candidate->ActiveScreenWidget;
                    bManagedMenu = OriginalManagedMenu.Get() == ActiveMenu && MenuParent.IsValid();
                    bMenuWasInViewport = ActiveMenu->IsInViewport();
                    OriginalMenuVisibility = ActiveMenu->GetVisibility();
                    Test->AddInfo(FString::Printf(TEXT("Actual main menu: %s managed=%d globalViewport=%d parent=%s"),
                        *ActiveMenu->GetName(), bManagedMenu, bMenuWasInViewport, *GetNameSafe(ActiveMenu->GetParent())));
                    ReportWorlds();
                    VerifyOriginalMenu();
                    Dialog = OpenDialog(false);
                    Test->TestNotNull(TEXT("Native create dialog opens in real PIE"), Dialog.Get());
                    if (!Dialog.IsValid()) return true;
                    Advance(1);
                    return false;
                }
            }
        }
        return false;
    }

    void ReportWorlds()
    {
        if (!GEngine) { Test->AddInfo(TEXT("PIE diagnostics: no engine.")); return; }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || Context.WorldType != EWorldType::PIE) continue;
            Test->AddInfo(FString::Printf(TEXT("PIE diagnostics: world=%s netmode=%d"), *World->GetName(), int32(World->GetNetMode())));
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* AnyController = It->Get();
                Acasino_simulatorPlayerController* Candidate = Cast<Acasino_simulatorPlayerController>(AnyController);
                UCasinoUIManagerComponent* Manager = Candidate ? Candidate->GetUIManager() : nullptr;
                UUserWidget* ActiveMenu = Candidate ? Candidate->ActiveScreenWidget.Get() : nullptr;
                UWidget* MenuRoot = ActiveMenu && ActiveMenu->WidgetTree ? ActiveMenu->WidgetTree->RootWidget.Get() : nullptr;
                Test->AddInfo(FString::Printf(TEXT("PIE diagnostics: controller=%s local=%d localplayer=%d manager=%d screen=%d screenClasses=%d managerActive=%s rootInViewport=%d menuParent=%s menu=[%s] content=[%s]"),
                    *GetNameSafe(AnyController), AnyController && AnyController->IsLocalController(), AnyController && AnyController->GetLocalPlayer() != nullptr,
                    Manager != nullptr, Manager ? int32(Manager->UIScreen) : -1, Candidate ? Candidate->ScreenWidgetClasses.Num() : 0,
                    *GetNameSafe(Manager ? Manager->ActiveScreenWidget.Get() : nullptr), Candidate && Candidate->UIRoot && Candidate->UIRoot->IsInViewport(),
                    *GetNameSafe(ActiveMenu ? ActiveMenu->GetParent() : nullptr), *GeometryDescription(ActiveMenu), *GeometryDescription(MenuRoot)));
            }
        }
        if (Dialog.IsValid())
        {
            Test->AddInfo(FString::Printf(TEXT("PIE diagnostics: dialog=[%s] root=[%s] card=[%s]"), *GeometryDescription(Dialog.Get()),
                *GeometryDescription(Dialog->WidgetTree ? Dialog->WidgetTree->RootWidget.Get() : nullptr),
                *GeometryDescription(Dialog->GetWidgetFromName(TEXT("RoomDialogCard")))));
        }
    }

    UCasinoRoomDialogWidget* OpenDialog(bool bProtectedJoin)
    {
        UCasinoOnlineSubsystem* Online = Controller->GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>();
        if (!Test->TestNotNull(TEXT("PIE online subsystem"), Online)) return nullptr;
        // Only bypass the presentation gate. Restore immediately and never call Submit.
        const ECasinoOnlineState PreviousState = Online->State;
        Online->State = ECasinoOnlineState::Ready;
        UCasinoRoomDialogWidget* Result = nullptr;
        if (bProtectedJoin)
        {
            FCasinoRoomInfo Room;
            Room.SessionId = TEXT("presentation-test-no-network");
            Room.RoomName = TEXT("PIE password popup test");
            Room.bPasswordProtected = true;
            Result = UCasinoRoomDialogWidget::ShowJoinRoomDialog(Controller.Get(), Room);
        }
        else Result = UCasinoRoomDialogWidget::ShowCreateRoomDialog(Controller.Get());
        Online->State = PreviousState;
        return Result;
    }

    void VerifyVisibleDialog(const TCHAR* Phase)
    {
        if (!Test->TestNotNull(FString::Printf(TEXT("%s dialog is alive"), Phase), Dialog.Get())) return;
        ReportWorlds();
        UWidget* Root = Dialog->WidgetTree ? Dialog->WidgetTree->RootWidget.Get() : nullptr;
        UWidget* Card = Dialog->GetWidgetFromName(TEXT("RoomDialogCard"));
        UWidget* Backdrop = Dialog->GetWidgetFromName(TEXT("RoomDialogBackdrop"));
        Test->TestTrue(FString::Printf(TEXT("%s popup root has visible painted geometry"), Phase), HasPaintedArea(Root));
        Test->TestTrue(FString::Printf(TEXT("%s popup card has visible painted geometry"), Phase), HasPaintedArea(Card));
        if (Root && Backdrop)
        {
            const FVector2D RootSize = Root->GetCachedGeometry().GetLocalSize();
            const FVector2D BackdropSize = Backdrop->GetCachedGeometry().GetLocalSize();
            Test->TestTrue(FString::Printf(TEXT("%s backdrop fills the popup height"), Phase),
                RootSize.Y > 1.0 && FMath::IsNearlyEqual(RootSize.Y, BackdropSize.Y, 1.0));
        }
        if (bManagedMenu)
            Test->TestTrue(FString::Printf(TEXT("%s popup shares the managed menu's modal layer"), Phase), Dialog->GetParent() == MenuParent.Get());
        else
            Test->TestTrue(FString::Printf(TEXT("%s popup uses the viewport route for the legacy menu"), Phase), Dialog->IsInViewport());
        Test->TestTrue(FString::Printf(TEXT("%s password input is rendered"), Phase), HasPaintedArea(Dialog->GetPasswordInput()));
    }

    void VerifyOriginalMenu()
    {
        if (!Controller.IsValid() || !Menu.IsValid()) return;
        Test->TestTrue(TEXT("The UI manager's original active-screen reference is unchanged"), Controller->ActiveScreenWidget == OriginalManagedMenu.Get());
        Test->TestTrue(TEXT("The same main menu parent remains attached"), Menu->GetParent() == MenuParent.Get());
        Test->TestTrue(TEXT("The original main menu keeps its viewport registration"), Menu->IsInViewport() == bMenuWasInViewport);
        Test->TestTrue(TEXT("The main menu's visibility is unchanged"), Menu->GetVisibility() == OriginalMenuVisibility);
        Test->TestTrue(TEXT("The main menu remains visible"), Menu->GetVisibility() != ESlateVisibility::Collapsed && Menu->GetVisibility() != ESlateVisibility::Hidden);
        for (const TCHAR* Name : {TEXT("StatusText"), TEXT("LobbyButton"), TEXT("FindRoomsButton"), TEXT("RoomListBox")})
        {
            FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Menu->GetClass(), Name);
            UObject* BoundWidget = Property ? Property->GetObjectPropertyValue_InContainer(Menu.Get()) : nullptr;
            Test->TestTrue(FString::Printf(TEXT("Main menu %s binding remains valid"), Name),
                IsValid(BoundWidget) && BoundWidget == Menu->GetWidgetFromName(Name));
        }
    }

    void Advance(int32 NextStage)
    {
        Stage = NextStage;
        StageFrame = GFrameCounter;
        StageStarted = FPlatformTime::Seconds();
    }

    FAutomationTestBase* Test;
    int32 Stage = 0;
    bool bStarted = false;
    bool bManagedMenu = false;
    bool bMenuWasInViewport = false;
    ESlateVisibility OriginalMenuVisibility = ESlateVisibility::Visible;
    uint64 StageFrame = 0;
    double StageStarted;
    double LastDiagnostic = 0.0;
    TWeakObjectPtr<Acasino_simulatorPlayerController> Controller;
    TWeakObjectPtr<UUserWidget> Menu;
    TWeakObjectPtr<UUserWidget> OriginalManagedMenu;
    TWeakObjectPtr<UPanelWidget> MenuParent;
    TWeakObjectPtr<UCasinoRoomDialogWidget> Dialog;
    TStrongObjectPtr<UCasinoRoomDialogWidget> ClosedDialog;
    FString ScreenshotPath;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoRoomDialogPIEVisibilityTest,
    "Casino.UI.RoomDialogs.PIEVisibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoRoomDialogPIEVisibilityTest::RunTest(const FString& Parameters)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        AddError(TEXT("Run the popup visibility test from a stopped editor session."));
        return false;
    }
    const FString MenuMap = GetDefault<UCasinoOnlineSettings>()->MenuMap.ToSoftObjectPath().GetLongPackageName();
    if (!TestFalse(TEXT("A main menu map is configured"), MenuMap.IsEmpty())) return false;
    // Existing menu-map startup faults are unrelated to popup presentation.
    // Match only these known messages; every unexpected error remains a failure.
    AddExpectedErrorPlain(TEXT("Police encounter requires exactly one police actor. Found: 0"), EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedErrorPlain(TEXT("Day loop: no actors tagged CasinoPaymentSpawn. Start cancelled."), EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedErrorPlain(TEXT("InputMode:UIOnly - Attempting to focus Non-Focusable widget SObjectWidget [Widget.cpp(990)]!"), EAutomationExpectedErrorFlags::Contains, 1);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(MenuMap));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCasinoRoomDialogPresentationCommand>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
