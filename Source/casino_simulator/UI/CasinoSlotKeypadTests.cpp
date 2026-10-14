#if WITH_DEV_AUTOMATION_TESTS

#include "UI/CasinoCalculatorWidget.h"
#include "UI/CasinoCalculatorKeyboardTests.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Interaction/InteractionSessionComponent.h"
#include "Layout/WidgetPath.h"
#include "Machine/SeatedMachineBase.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SVirtualWindow.h"
#include "casino_simulatorAttributeSet.h"
#include "casino_simulatorCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoSlotKeypadTest, "Casino.UI.Slot.Keypad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoSlotKeypadTest::RunTest(const FString& Parameters)
{
    if (!TestTrue(TEXT("Slate is available for actual slot key routing"), FSlateApplication::IsInitialized())) return false;
    UClass* Class = LoadClass<UCasinoCalculatorWidget>(nullptr,
        TEXT("/Game/LJH/UI/WBP_SlotMachineControl.WBP_SlotMachineControl_C"));
    UClass* MachineClass = LoadClass<ASeatedMachineBase>(nullptr,
        TEXT("/Game/LJH/Asset/SlotMachine/BP/Actors/BP_SlotMachine_5x3.BP_SlotMachine_5x3_C"));
    UClass* PlayerClass = LoadClass<Acasino_simulatorCharacter>(nullptr,
        TEXT("/Game/1_BluePrint/Actor/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Slot controls use the shared keyboard parent"), Class) ||
        !TestNotNull(TEXT("Actual owning slot-machine class"), MachineClass) ||
        !TestNotNull(TEXT("Actual player class for the existing amount cap"), PlayerClass)) return false;
    if (!TestTrue(TEXT("Slot controls retain native activation ticking"),
        CastChecked<UWidgetBlueprintGeneratedClass>(Class)->ClassRequiresNativeTick())) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    // AppendDigit reads the slot's current session user and caps input by currency.
    // Deferred actors provide those data only: no construction scripts,
    // BeginPlay, seat/camera transition, spin, purchase, or network request is executed.
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Spawn.bDeferConstruction = true;
    ASeatedMachineBase* Machine = World->SpawnActor<ASeatedMachineBase>(MachineClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    Acasino_simulatorCharacter* Player = World->SpawnActor<Acasino_simulatorCharacter>(PlayerClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Isolated deferred slot fixture"), Machine) || !TestNotNull(TEXT("Isolated deferred player fixture"), Player)) return false;
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player);
    ASC->InitStats(Ucasino_simulatorAttributeSet::StaticClass(), nullptr);
    ASC->SetNumericAttributeBase(Ucasino_simulatorAttributeSet::GetCurrencyAttribute(), 10000.f);
    UInteractionSessionComponent* Session = Machine->GetInteractionSessionComponent();
    if (!TestNotNull(TEXT("Isolated slot session"), Session) ||
        !TestTrue(TEXT("Fixture player joins the deferred slot session"), Session->TryJoin(Player))) return false;
    if (!TestEqual(TEXT("Fixture currency is readable through the original player getter"), Player->GetCurrency(), 10000.f) ||
        !TestTrue(TEXT("Fixture slot current user is the controlled player"), Machine->GetCurrentUser() == Player)) return false;

    UCasinoCalculatorWidget* Control = CreateWidget<UCasinoCalculatorWidget>(World, Class);
    if (!TestNotNull(TEXT("Live isolated slot controls"), Control)) return false;
    FObjectPropertyBase* OwningMachine = FindFProperty<FObjectPropertyBase>(Class, TEXT("OwningSlotMachine"));
    FIntProperty* Input = FindFProperty<FIntProperty>(Class, TEXT("InputBetAmount"));
    FIntProperty* Confirmed = FindFProperty<FIntProperty>(Class, TEXT("ConfirmedBetAmount"));
    FBoolProperty* HasConfirmed = FindFProperty<FBoolProperty>(Class, TEXT("bHasConfirmedBet"));
    FIntProperty* SpinCount = FindFProperty<FIntProperty>(MachineClass, TEXT("PaidSpinCount"));
    if (!TestNotNull(TEXT("Existing owning-machine property"), OwningMachine) || !TestNotNull(TEXT("Existing editable amount"), Input) ||
        !TestNotNull(TEXT("Existing confirmed amount"), Confirmed) || !TestNotNull(TEXT("Existing confirmation state"), HasConfirmed) ||
        !TestNotNull(TEXT("Existing paid-spin counter"), SpinCount)) return false;
    OwningMachine->SetObjectPropertyValue_InContainer(Control, Machine);
    const int32 SpinsBefore = SpinCount->GetPropertyValue_InContainer(Machine);
    UWidget* Grid = Control->GetWidgetFromName(TEXT("SlotKeypadGrid"));
    UButton* Open = Cast<UButton>(Control->GetWidgetFromName(TEXT("Button_BetAmount")));
    UButton* OK = Cast<UButton>(Control->GetWidgetFromName(TEXT("Button_OK")));
    UButton* Lever = Cast<UButton>(Control->GetWidgetFromName(TEXT("Button_PullLever")));
    UButton* Nine = Cast<UButton>(Control->GetWidgetFromName(TEXT("Button_9")));
    if (!TestNotNull(TEXT("Original slot editing grid"), Grid) || !TestNotNull(TEXT("Original open-edit button"), Open) ||
        !TestNotNull(TEXT("Original slot OK button"), OK) || !TestNotNull(TEXT("Separate original lever button"), Lever) ||
        !TestNotNull(TEXT("Original focused numeric button"), Nine)) return false;
    auto* OKReceiver = NewObject<UCasinoCalculatorKeyboardTestReceiver>(World);
    auto* LeverReceiver = NewObject<UCasinoCalculatorKeyboardTestReceiver>(World);
    OK->OnClicked.AddDynamic(OKReceiver, &UCasinoCalculatorKeyboardTestReceiver::RecordConfirm);
    // Keep every numeric/edit/OK binding. Remove only the isolated fixture's
    // lever action so a failed routing assertion cannot initiate an actual spin.
    Lever->OnClicked.Clear();
    Lever->OnClicked.AddDynamic(LeverReceiver, &UCasinoCalculatorKeyboardTestReceiver::RecordConfirm);

    const TSharedRef<SWidget> ControlSlate = Control->TakeWidget();
    if (!TestTrue(TEXT("Live slot retains the controlled owning machine"), OwningMachine->GetObjectPropertyValue_InContainer(Control) == Machine) ||
        !TestTrue(TEXT("Original slot digit click is bound"), CastChecked<UButton>(Control->GetWidgetFromName(TEXT("Button_1")))->OnClicked.IsBound())) return false;
    TestTrue(TEXT("Slot digit buttons match blackjack navy"),
        Nine->GetStyle().Normal.TintColor.GetSpecifiedColor().Equals(FLinearColor(.023f, .032f, .049f, 1.f)));
    TestTrue(TEXT("Slot OK matches blackjack gold"),
        OK->GetStyle().Normal.TintColor.GetSpecifiedColor().Equals(FLinearColor(.76f, .49f, .15f, 1.f)));
    const TSharedRef<SButton> Outside = SNew(SButton);
    const TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(FVector2D(1920.f, 1080.f));
    Window->SetContent(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Outside]
        + SVerticalBox::Slot().FillHeight(1.f)[ControlSlate]);
    FSlateApplication& Slate = FSlateApplication::Get();
    const TSharedPtr<SWidget> OldFocus = Slate.GetUserFocusedWidget(0);
    Slate.RegisterVirtualWindow(Window);
    ON_SCOPE_EXIT
    {
        Window->SetContent(SNullWidget::NullWidget);
        Control->ReleaseSlateResources(true);
        Slate.UnregisterVirtualWindow(Window);
        if (OldFocus.IsValid()) Slate.SetUserFocus(0, OldFocus, EFocusCause::SetDirectly);
    };
    Window->SlatePrepass();
    const auto Event = [](const FKey& Key, bool Repeat = false)
    {
        return FKeyEvent(Key, FModifierKeysState(), 0, Repeat, 0, 0);
    };
    const auto Route = [&](const FKey& Key, bool Repeat = false)
    {
        const bool Handled = Slate.ProcessKeyDownEvent(Event(Key, Repeat));
        Slate.ProcessKeyUpEvent(Event(Key));
        return Handled;
    };
    Slate.SetUserFocus(0, Outside, EFocusCause::SetDirectly);
    Control->NativeTick(FGeometry(), 1.f / 60.f);
    FWidgetPath Path;
    TestFalse(TEXT("Visible slot controls with hidden grid are not a numeric input target"), Control->GetActivePath(Path));
    TestFalse(TEXT("Hidden slot grid leaves numeric keys unhandled"), Control->HandleCalculatorKey(Event(EKeys::One)));
    TestTrue(TEXT("Hidden slot grid does not steal focus"), Slate.GetUserFocusedWidget(0) == Outside);

    Open->OnClicked.Broadcast();
    Control->NativeTick(FGeometry(), 1.f / 60.f);
    if (!TestTrue(TEXT("Original edit button opens the numeric grid"), Grid->IsVisible()) ||
        !TestTrue(TEXT("Visible slot grid receives keyboard focus"), Slate.GetUserFocusedWidget(0) == ControlSlate)) return false;
    CastChecked<UButton>(Control->GetWidgetFromName(TEXT("Button_1")))->OnClicked.Broadcast();
    if (!TestEqual(TEXT("Original digit mouse-click graph can append one in the same fixture"), Input->GetPropertyValue_InContainer(Control), 1)) return false;
    Open->OnClicked.Broadcast();
    Control->NativeTick(FGeometry(), 1.f / 60.f);
    TestTrue(TEXT("Top-row one reaches original AppendDigit"), Route(EKeys::One));
    TestTrue(TEXT("Top-row zero reaches original AppendDigit"), Route(EKeys::Zero));
    TestTrue(TEXT("Numpad zero reaches original AppendDigit"), Route(EKeys::NumPadZero));
    TestTrue(TEXT("Fourth keyboard digit reaches original AppendDigit"), Route(EKeys::Zero));
    if (!TestEqual(TEXT("The existing slot amount is exactly 1000"), Input->GetPropertyValue_InContainer(Control), 1000)) return false;
    TestTrue(TEXT("Backspace uses the existing slot BackspaceBet button"), Route(EKeys::BackSpace));
    TestEqual(TEXT("Backspace removes one existing amount digit"), Input->GetPropertyValue_InContainer(Control), 100);
    TestTrue(TEXT("Delete also uses the slot BackspaceBet fallback"), Route(EKeys::Delete));
    TestEqual(TEXT("Delete removes another amount digit"), Input->GetPropertyValue_InContainer(Control), 10);
    Route(EKeys::NumPadZero); Route(EKeys::Zero);
    TestEqual(TEXT("Re-entered slot amount is exactly 1000"), Input->GetPropertyValue_InContainer(Control), 1000);
    Slate.SetUserFocus(0, Nine->TakeWidget(), EFocusCause::SetDirectly);
    TestTrue(TEXT("Enter preview confirms instead of clicking a focused numeric button"), Route(EKeys::Enter));
    TestEqual(TEXT("Enter invokes the original OK click exactly once"), OKReceiver->ConfirmCount, 1);
    TestEqual(TEXT("Original ConfirmBetAmount stores the typed 1000"), Confirmed->GetPropertyValue_InContainer(Control), 1000);
    TestTrue(TEXT("Original ConfirmBetAmount marks the amount confirmed"), HasConfirmed->GetPropertyValue_InContainer(Control));
    TestEqual(TEXT("Enter cannot append the focused nine"), Input->GetPropertyValue_InContainer(Control), 1000);
    TestFalse(TEXT("Original confirmation hides only the numeric grid"), Grid->IsVisible());
    TestTrue(TEXT("The wider slot controls remain visible after confirmation"), Control->IsVisible());
    TestEqual(TEXT("Amount confirmation never clicks the separate lever"), LeverReceiver->ConfirmCount, 0);
    TestEqual(TEXT("Amount confirmation never starts a paid spin"), SpinCount->GetPropertyValue_InContainer(Machine), SpinsBefore);
    TestEqual(TEXT("Amount confirmation spends no currency"), Player->GetCurrency(), 10000.f);
    Control->NativeTick(FGeometry(), 1.f / 60.f);
    TestTrue(TEXT("Closing only the numeric grid restores earlier UI focus"), Slate.GetUserFocusedWidget(0) == Outside);
    TestFalse(TEXT("Confirmed hidden grid does not consume another digit"), Control->HandleCalculatorKey(Event(EKeys::Two)));

    // Sibling controls retain their normal Slate Enter behavior when the pad is
    // hidden; the receiver proves routing without executing the real lever.
    Slate.SetUserFocus(0, Lever->TakeWidget(), EFocusCause::SetDirectly);
    Control->NativeTick(FGeometry(), 1.f / 60.f);
    TestTrue(TEXT("Hidden grid leaves the lever's focus alone"), Slate.GetUserFocusedWidget(0) == Lever->GetCachedWidget());
    TestTrue(TEXT("Hidden grid lets the focused lever handle Enter normally"), Route(EKeys::Enter));
    TestEqual(TEXT("The focused sibling gets exactly its own click"), LeverReceiver->ConfirmCount, 1);
    TestEqual(TEXT("Sibling Enter cannot confirm another keypad amount"), OKReceiver->ConfirmCount, 1);

    Open->OnClicked.Broadcast();
    Control->NativeTick(FGeometry(), 1.f / 60.f);
    TestTrue(TEXT("Reopening the numeric grid reacquires focus"), Slate.GetUserFocusedWidget(0) == ControlSlate);
    TestEqual(TEXT("Original reopen clears the edit amount"), Input->GetPropertyValue_InContainer(Control), 0);
    TestFalse(TEXT("Original reopen resets confirmed state"), HasConfirmed->GetPropertyValue_InContainer(Control));
    Route(EKeys::NumPadTwo); Route(EKeys::Zero); Route(EKeys::Zero);
    TestEqual(TEXT("Reopened slot keypad accepts numpad digits"), Input->GetPropertyValue_InContainer(Control), 200);
    TestTrue(TEXT("Held Enter repeat is consumed while the pad is active"), Route(EKeys::Enter, true));
    TestEqual(TEXT("Held Enter cannot produce another OK click"), OKReceiver->ConfirmCount, 1);
    TestTrue(TEXT("Held Enter leaves the reopened numeric grid visible"), Grid->IsVisible());
    TestTrue(TEXT("A fresh Enter confirms the reopened edit"), Route(EKeys::Enter));
    TestEqual(TEXT("The second physical Enter confirms exactly once"), OKReceiver->ConfirmCount, 2);
    TestEqual(TEXT("The reopened confirmed amount is 200"), Confirmed->GetPropertyValue_InContainer(Control), 200);
    TestEqual(TEXT("Reopened amount confirmation still cannot start a spin"), SpinCount->GetPropertyValue_InContainer(Machine), SpinsBefore);
    TestEqual(TEXT("Reopened amount confirmation leaves currency unchanged"), Player->GetCurrency(), 10000.f);

    // Cancel a fully erased amount through both actual confirmation routes.
    // Closing the grid must not confirm zero or click the separate lever.
    Control->NativeTick(FGeometry(), 1.f / 60.f);
    for (const bool UseEnter : { false, true })
    {
        Slate.SetUserFocus(0, Outside, EFocusCause::SetDirectly);
        Open->OnClicked.Broadcast();
        Control->NativeTick(FGeometry(), 1.f / 60.f);
        TestTrue(TEXT("The keypad can reopen before zero-amount cancellation"), Grid->IsVisible());
        Route(EKeys::One);
        Route(EKeys::BackSpace);
        TestEqual(TEXT("Backspace can erase the amount to zero"), Input->GetPropertyValue_InContainer(Control), 0);
        const int32 ConfirmationsBefore = OKReceiver->ConfirmCount;
        const int32 LeverClicksBefore = LeverReceiver->ConfirmCount;
        if (UseEnter) TestTrue(TEXT("Enter handles a zero-amount cancellation"), Route(EKeys::Enter));
        else OK->OnClicked.Broadcast();
        TestEqual(TEXT("Zero confirmation clicks OK exactly once"), OKReceiver->ConfirmCount, ConfirmationsBefore + 1);
        TestFalse(TEXT("Zero confirmation dismisses the numeric grid"), Grid->IsVisible());
        TestFalse(TEXT("Zero is not accepted as a confirmed bet"), HasConfirmed->GetPropertyValue_InContainer(Control));
        TestEqual(TEXT("Zero cancellation does not click the lever"), LeverReceiver->ConfirmCount, LeverClicksBefore);
        TestEqual(TEXT("Zero cancellation does not start a paid spin"), SpinCount->GetPropertyValue_InContainer(Machine), SpinsBefore);
        TestEqual(TEXT("Zero cancellation does not spend currency"), Player->GetCurrency(), 10000.f);
        Control->NativeTick(FGeometry(), 1.f / 60.f);
        TestTrue(TEXT("Zero cancellation restores earlier UI focus"), Slate.GetUserFocusedWidget(0) == Outside);
        TestFalse(TEXT("Closed zero-amount keypad stops consuming digits"), Control->HandleCalculatorKey(Event(EKeys::Two)));
    }

    Open->OnClicked.Broadcast();
    Control->NativeTick(FGeometry(), 1.f / 60.f);
    Route(EKeys::Three); Route(EKeys::Zero); Route(EKeys::Zero);
    TestTrue(TEXT("A valid amount can be confirmed after cancelling zero"), Route(EKeys::Enter));
    TestEqual(TEXT("The subsequent valid bet remains exactly 300"), Confirmed->GetPropertyValue_InContainer(Control), 300);
    TestTrue(TEXT("The subsequent valid amount is confirmed"), HasConfirmed->GetPropertyValue_InContainer(Control));
    TestFalse(TEXT("The subsequent valid confirmation also closes the keypad"), Grid->IsVisible());
    TestEqual(TEXT("Subsequent confirmation still spends no currency"), Player->GetCurrency(), 10000.f);
    return true;
}

#endif
