#include "UI/CasinoCalculatorKeyboardTests.h"

void UCasinoCalculatorKeyboardTestReceiver::RecordNumber(int32 Value) { Values.Add(Value); }
void UCasinoCalculatorKeyboardTestReceiver::RecordConfirm() { ++ConfirmCount; }

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/CasinoCalculatorWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoCalculatorKeyboardTest, "Casino.UI.Calculator.Keyboard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoCalculatorKeyboardTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Slate is available for real key routing"), FSlateApplication::IsInitialized())) return false;
	UClass* Class = LoadClass<UCasinoCalculatorWidget>(nullptr,
		TEXT("/Game/1_BluePrint/Widget/WBP_Calculate.WBP_Calculate_C"));
	if (!TestNotNull(TEXT("Shared keypad has its native keyboard parent"), Class)) return false;
	if (!TestTrue(TEXT("Shared Blueprint enables native activation ticking"), CastChecked<UWidgetBlueprintGeneratedClass>(Class)->ClassRequiresNativeTick()) ||
		!TestTrue(TEXT("Shared keypad CDO is focusable"), CastChecked<UUserWidget>(Class->GetDefaultObject())->IsFocusable())) return false;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UCasinoCalculatorWidget* Keypad = CreateWidget<UCasinoCalculatorWidget>(World, Class);
	UCasinoCalculatorKeyboardTestReceiver* Receiver = NewObject<UCasinoCalculatorKeyboardTestReceiver>(World);
	if (!TestNotNull(TEXT("Live shared keypad instance"), Keypad)) return false;
	FMulticastDelegateProperty* NumberPadEvent = FindFProperty<FMulticastDelegateProperty>(Class, TEXT("NumberPadEvent"));
	if (!TestNotNull(TEXT("Original numeric dispatcher"), NumberPadEvent)) return false;
	FScriptDelegate NumberListener;
	NumberListener.BindUFunction(Receiver, GET_FUNCTION_NAME_CHECKED(UCasinoCalculatorKeyboardTestReceiver, RecordNumber));
	NumberPadEvent->AddDelegate(NumberListener, Keypad);
	UButton* Confirm = CastChecked<UButton>(Keypad->GetWidgetFromName(TEXT("Button_OK")));
	Confirm->OnClicked.AddDynamic(Receiver, &UCasinoCalculatorKeyboardTestReceiver::RecordConfirm);

	// This virtual window routes through the real SObjectWidget preview path,
	// without opening a game screen or submitting a payment, bet, or trade.
	const TSharedRef<SWidget> KeypadSlate = Keypad->TakeWidget();
	TestTrue(TEXT("Shared digit buttons use the casino navy palette"),
		CastChecked<UButton>(Keypad->GetWidgetFromName(TEXT("Button_1")))->GetStyle().Normal.TintColor.GetSpecifiedColor()
			.Equals(FLinearColor(.023f, .032f, .049f, 1.f)));
	TestTrue(TEXT("Shared confirmation uses the casino gold palette"),
		Confirm->GetStyle().Normal.TintColor.GetSpecifiedColor().Equals(FLinearColor(.76f, .49f, .15f, 1.f)));
	const TSharedRef<SButton> OutsideFocus = SNew(SButton);
	const TSharedRef<SBox> Layer = SNew(SBox)[KeypadSlate];
	const TSharedRef<SWidgetSwitcher> Switcher = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()[Layer]
		+ SWidgetSwitcher::Slot()[SNullWidget::NullWidget];
	const TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(FVector2D(1280.f, 720.f));
	Window->SetContent(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[OutsideFocus]
		+ SVerticalBox::Slot().FillHeight(1.f)[Switcher]);
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSharedPtr<SWidget> OldFocus = Slate.GetUserFocusedWidget(0);
	Slate.RegisterVirtualWindow(Window);
	ON_SCOPE_EXIT
	{
		Window->SetContent(SNullWidget::NullWidget);
		Keypad->ReleaseSlateResources(true);
		Slate.UnregisterVirtualWindow(Window);
		if (OldFocus.IsValid()) Slate.SetUserFocus(0, OldFocus, EFocusCause::SetDirectly);
	};
	Window->SlatePrepass();
	Slate.SetUserFocus(0, OutsideFocus, EFocusCause::SetDirectly);
	const auto Event = [](const FKey& Key, bool Repeat = false, bool Ctrl = false)
	{
		return FKeyEvent(Key, FModifierKeysState(false, false, Ctrl, false, false, false, false, false, false), 0, Repeat, 0, 0);
	};
	const auto Route = [&](const FKey& Key, bool Repeat = false)
	{
		const bool Handled = Slate.ProcessKeyDownEvent(Event(Key, Repeat));
		Slate.ProcessKeyUpEvent(Event(Key));
		return Handled;
	};
	Keypad->SetVisibility(ESlateVisibility::Visible);
	Layer->SetVisibility(EVisibility::Collapsed);
	FWidgetPath InactivePath;
	TestFalse(TEXT("Keypad does not become active before its containing layer"), Keypad->GetActivePath(InactivePath));
	TestFalse(TEXT("Hidden keypad does not handle a number"), Keypad->HandleCalculatorKey(Event(EKeys::One)));
	Layer->SetVisibility(EVisibility::Visible);
	Keypad->NativeTick(FGeometry(), 1.f / 60.f);
	TestTrue(TEXT("Late activation gives the visible keypad keyboard focus"), Slate.GetUserFocusedWidget(0) == KeypadSlate);

	const FKey Digits[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	const FKey Numpad[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree,
		EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	for (int32 Digit = 0; Digit < 10; ++Digit)
	{
		const int32 CountBefore = Receiver->Values.Num();
		TestTrue(TEXT("Top-row digit routes through the existing click graph"), Route(Digits[Digit]));
		TestTrue(TEXT("Numpad digit routes through the existing click graph"), Route(Numpad[Digit]));
		if (!TestEqual(TEXT("Exactly one dispatch per numeric key"), Receiver->Values.Num(), CountBefore + 2)) return false;
		TestEqual(TEXT("Top-row key emits the original digit"), Receiver->Values[CountBefore], Digit);
		TestEqual(TEXT("Numpad key emits the original digit"), Receiver->Values[CountBefore + 1], Digit);
	}
	TestTrue(TEXT("Backspace routes the existing Delete click"), Route(EKeys::BackSpace));
	TestEqual(TEXT("Delete preserves the original -1 dispatcher value"), Receiver->Values.Last(), -1);
	TestTrue(TEXT("Delete key also routes the Delete click"), Route(EKeys::Delete));
	UButton* Disabled = CastChecked<UButton>(Keypad->GetWidgetFromName(TEXT("Button_5")));
	Disabled->SetIsEnabled(false);
	const int32 CountBeforeDisabled = Receiver->Values.Num();
	TestTrue(TEXT("A disabled digit key is consumed while the keypad is active"), Keypad->HandleCalculatorKey(Event(EKeys::Five)));
	TestEqual(TEXT("Disabled button produces no numeric event"), Receiver->Values.Num(), CountBeforeDisabled);
	Disabled->SetIsEnabled(true);
	TestFalse(TEXT("Gameplay Tab is left unhandled"), Keypad->HandleCalculatorKey(Event(EKeys::Tab)));
	TestFalse(TEXT("Modified shortcuts are left unhandled"), Keypad->HandleCalculatorKey(Event(EKeys::One, false, true)));

	// Preview handling must win over a focused UButton's default Enter action.
	UButton* DigitButton = CastChecked<UButton>(Keypad->GetWidgetFromName(TEXT("Button_9")));
	Slate.SetUserFocus(0, DigitButton->TakeWidget(), EFocusCause::SetDirectly);
	const int32 CountBeforeConfirm = Receiver->Values.Num();
	Confirm->SetIsEnabled(false);
	TestTrue(TEXT("Disabled OK consumes Enter before the focused numeric button"), Route(EKeys::Enter));
	TestEqual(TEXT("Disabled OK does not dispatch another digit"), Receiver->Values.Num(), CountBeforeConfirm);
	TestEqual(TEXT("Disabled OK cannot confirm"), Receiver->ConfirmCount, 0);
	Confirm->SetIsEnabled(true);
	Confirm->SetVisibility(ESlateVisibility::Collapsed);
	TestTrue(TEXT("Hidden OK also prevents default numeric button Enter"), Route(EKeys::Enter));
	TestEqual(TEXT("Hidden OK does not dispatch another digit"), Receiver->Values.Num(), CountBeforeConfirm);
	TestEqual(TEXT("Hidden OK cannot confirm"), Receiver->ConfirmCount, 0);
	Confirm->SetVisibility(ESlateVisibility::Visible);
	TestTrue(TEXT("Enter confirms once even while a numeric button has focus"), Route(EKeys::Enter));
	TestEqual(TEXT("Existing OK delegate fires exactly once"), Receiver->ConfirmCount, 1);
	TestEqual(TEXT("Enter does not also click the focused numeric button"), Receiver->Values.Num(), CountBeforeConfirm);
	TestTrue(TEXT("Original OK graph still hides the keypad"), Keypad->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("Closed keypad cannot consume another Enter"), Keypad->HandleCalculatorKey(Event(EKeys::Enter)));

	Keypad->SetVisibility(ESlateVisibility::Visible);
	Keypad->NativeTick(FGeometry(), 1.f / 60.f);
	TestTrue(TEXT("Reopening acquires focus again"), Slate.GetUserFocusedWidget(0) == KeypadSlate);
	TestTrue(TEXT("Held Enter repeat is handled while active"), Keypad->HandleCalculatorKey(Event(EKeys::Enter, true)));
	TestEqual(TEXT("Held Enter does not confirm again"), Receiver->ConfirmCount, 1);
	TestTrue(TEXT("Held Enter leaves the reopened keypad visible"), Keypad->GetVisibility() == ESlateVisibility::Visible);

	Switcher->SetActiveWidgetIndex(1);
	Slate.SetUserFocus(0, OutsideFocus, EFocusCause::SetDirectly);
	Keypad->NativeTick(FGeometry(), 1.f / 60.f);
	TestFalse(TEXT("Inactive switcher page is not input-active"), Keypad->GetActivePath(InactivePath));
	TestFalse(TEXT("Inactive switcher keypad does not consume a number"), Keypad->HandleCalculatorKey(Event(EKeys::Two)));
	TestTrue(TEXT("Inactive page cannot steal focus"), Slate.GetUserFocusedWidget(0) == OutsideFocus);
	Switcher->SetActiveWidgetIndex(0);
	Keypad->NativeTick(FGeometry(), 1.f / 60.f);
	TestTrue(TEXT("Returning to the keypad page reacquires focus"), Slate.GetUserFocusedWidget(0) == KeypadSlate);
	Layer->SetVisibility(EVisibility::Collapsed);
	TestFalse(TEXT("Closing only the parent disables keyboard routing"), Keypad->HandleCalculatorKey(Event(EKeys::Three)));
	Keypad->NativeTick(FGeometry(), 1.f / 60.f);
	Layer->SetVisibility(EVisibility::Visible);
	Keypad->NativeTick(FGeometry(), 1.f / 60.f);
	TestTrue(TEXT("Parent reopen keeps the keypad usable"), Route(EKeys::Four));
	TestEqual(TEXT("Reopened keypad sends the correct number"), Receiver->Values.Last(), 4);
	// Dynamic consumers use UIManager, which focuses the keypad before its
	// first tick. Closing still needs to restore the underlying interaction UI.
	for (int32 Open = 0; Open < 2; ++Open)
	{
		Slate.SetUserFocus(0, OutsideFocus, EFocusCause::SetDirectly);
		Keypad->SetVisibility(ESlateVisibility::Visible);
		Slate.SetUserFocus(0, KeypadSlate, EFocusCause::SetDirectly);
		Keypad->NativeTick(FGeometry(), 1.f / 60.f);
		const int32 ConfirmsBefore = Receiver->ConfirmCount;
		TestTrue(TEXT("UIManager-prefocused keypad routes Enter once"), Route(EKeys::Enter));
		TestEqual(TEXT("UIManager-prefocused Enter confirms exactly once"), Receiver->ConfirmCount, ConfirmsBefore + 1);
		TestTrue(TEXT("Collapsed prefocused keypad restores underlying UI focus"), Slate.GetUserFocusedWidget(0) == OutsideFocus);
		TestFalse(TEXT("Collapsed prefocused keypad cannot swallow gameplay input"), Keypad->HandleCalculatorKey(Event(EKeys::Five)));
	}
	Slate.ClearUserFocus(0, EFocusCause::SetDirectly);
	Keypad->SetVisibility(ESlateVisibility::Visible);
	Slate.SetUserFocus(0, KeypadSlate, EFocusCause::SetDirectly);
	Keypad->NativeTick(FGeometry(), 1.f / 60.f);
	TestTrue(TEXT("Keypad with no prior focus still confirms"), Route(EKeys::Enter));
	TestFalse(TEXT("No prior focus cannot leave a hidden keypad focused"), Slate.GetUserFocusedWidget(0).IsValid());
	TestFalse(TEXT("No-prior-focus close leaves numbers unhandled"), Keypad->HandleCalculatorKey(Event(EKeys::Six)));
	Keypad->SetVisibility(ESlateVisibility::Visible);
	Slate.SetUserFocus(0, KeypadSlate, EFocusCause::SetDirectly);
	{
		TSharedPtr<SWidget> RemovedUI = SNew(SButton);
		Keypad->PreviousFocus = RemovedUI;
	}
	TestTrue(TEXT("Expired previous UI does not prevent original OK behavior"), Route(EKeys::Enter));
	TestFalse(TEXT("Expired previous UI cannot leave a hidden keypad focused"), Slate.GetUserFocusedWidget(0).IsValid());
	TestFalse(TEXT("Expired-UI close leaves Enter unhandled"), Keypad->HandleCalculatorKey(Event(EKeys::Enter)));
	Keypad->SetVisibility(ESlateVisibility::Visible);
	Window->SetContent(SNullWidget::NullWidget);
	TestFalse(TEXT("Detached keypad leaves gameplay keys unhandled"), Keypad->HandleCalculatorKey(Event(EKeys::Enter)));
	return true;
}
#endif
